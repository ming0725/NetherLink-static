#include "SmoothScrollWidget.h"
#include "ScrollAreaNoWheel.h"
#include "ScrollBarThumb.h"
#include <QPainter>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QVBoxLayout>

SmoothScrollWidget::SmoothScrollWidget(QWidget *parent)
    : QWidget(parent)
    , scrollArea(new ScrollAreaNoWheel(this))
    , scrollBarThumb(new ScrollBarThumb(this))
    , contentWidget(new QWidget)
    , scrollAnimation(new QTimeLine(300, this))
    , contentOffset(0), thumbOffset(0)
    , dragging(false)
{
    scrollBarThumb->installEventFilter(this);
    installEventFilter(this);
    scrollArea->viewport()->installEventFilter(this);
    contentWidget->installEventFilter(this);

    auto *opacity = new QGraphicsOpacityEffect(scrollBarThumb);
    opacity->setOpacity(0.0);
    scrollBarThumb->setGraphicsEffect(opacity);
    scrollBarThumb->hide();

    scrollArea->setWidget(contentWidget);
    scrollArea->setGeometry(rect());

    scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    scrollAnimation->setUpdateInterval(0);

    connect(scrollAnimation, &QTimeLine::frameChanged, this, [=](int value) {
        int maxOffset = qMax(0, contentWidget->height() - height());
        contentOffset = qBound(0, value, maxOffset);
        contentWidget->move(0, -contentOffset);

        int maxThumbOffset = height() - scrollBarThumb->height();
        thumbOffset = contentOffset * maxThumbOffset / qMax(1, maxOffset);
        scrollBarThumb->move(width() - 13, thumbOffset);
    });
}

SmoothScrollWidget::~SmoothScrollWidget() {}

void SmoothScrollWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    scrollArea->setGeometry(rect());
    updateScrollBar();
}

void SmoothScrollWidget::wheelEvent(QWheelEvent *event)
{
    if (contentWidget->height() <= height()) {
        event->ignore();
        return;
    }
    int delta = -event->angleDelta().y();
    animateTo(contentOffset + (delta > 0 ? 240 : -240));
    event->accept();
}

bool SmoothScrollWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        if (contentWidget->height() > height()) {
            scrollBarThumb->show();
            auto *anim = new QPropertyAnimation(scrollBarThumb->graphicsEffect(), "opacity", this);
            anim->setDuration(150);
            anim->setEndValue(1.0);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    else if (event->type() == QEvent::Leave) {
        auto *anim = new QPropertyAnimation(scrollBarThumb->graphicsEffect(), "opacity", this);
        anim->setDuration(150);
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, scrollBarThumb, &QWidget::hide);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    else if (obj == scrollBarThumb) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
            dragging = true;
            dragStartY = me->globalPosition().y();
            thumbOffsetAtDragStart = thumbOffset;
            return true;
        }
        else if (event->type() == QEvent::MouseMove && dragging) {
            int deltaY = static_cast<QMouseEvent*>(event)->globalPosition().y() - dragStartY;
            int maxThumbOffset = height() - scrollBarThumb->height();
            thumbOffset = qBound(0, thumbOffsetAtDragStart + deltaY, maxThumbOffset);
            int maxOffset = qMax(0, contentWidget->height() - height());
            contentOffset = thumbOffset * maxOffset / qMax(1, maxThumbOffset);
            contentWidget->move(0, -contentOffset);
            scrollBarThumb->move(width() - 13, thumbOffset);
            return true;
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            dragging = false;
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SmoothScrollWidget::animateTo(int targetOffset)
{
    int maxOffset = contentWidget->height() - height();
    if (maxOffset <= 0) return;
    scrollAnimation->stop();
    scrollAnimation->setFrameRange(contentOffset, qBound(0, targetOffset, maxOffset));
    scrollAnimation->start();
}

void SmoothScrollWidget::updateScrollBar()
{
    int ch = contentWidget->height(), vh = height();
    contentOffset = qBound(0, contentOffset, qMax(0, ch - vh));
    if (ch <= vh) {
        scrollBarThumb->hide();
        return;
    }
    scrollBarThumb->show();
    int thumbHeight = qBound(qMin(20, vh), vh * vh / ch, qMax(20, vh));
    scrollBarThumb->setGeometry(width() - 13, thumbOffset, 8, thumbHeight);
}

void SmoothScrollWidget::setContentMinimumHeight(int height)
{
    contentWidget->resize(width(), height);
    contentWidget->move(0, -contentOffset);
    updateScrollBar();
}

void SmoothScrollWidget::relayoutContent()
{
    int totalH = contentWidget->height();
    int viewH  = height();
    int maxOffset = qMax(0, totalH - viewH);
    contentOffset = qBound(0, contentOffset, maxOffset);
    contentWidget->move(0, -contentOffset);
    int thumbH   = qBound(20, viewH * viewH / qMax(1, totalH), viewH);
    int thumbMax = viewH - thumbH;
    thumbOffset  = (maxOffset>0 ? contentOffset * thumbMax / maxOffset : 0);
    scrollBarThumb->setGeometry(width() - 13, thumbOffset, 8, thumbH);
}
