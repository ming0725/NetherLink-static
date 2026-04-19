#include "CustomScrollArea.h"
#include <QScrollBar>
#include <QMouseEvent>
#include <QPainter>

CustomScrollArea::CustomScrollArea(QWidget* parent)
        : QAbstractScrollArea(parent)
        , contentWidget(new QWidget(viewport()))
        , thumb(new ScrollBarThumb(this))
        , scrollAnimation(new QTimeLine(380, this))
{
    // 1) 关闭系统滚动条
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy  (Qt::ScrollBarAlwaysOff);

    // 2) contentWidget
    contentWidget->move(0, 0);

    auto *opacity = new QGraphicsOpacityEffect(thumb);
    opacity->setOpacity(0.0);
    thumb->setGraphicsEffect(opacity);
    thumb->hide();

    // 4) 安装 eventFilter
    viewport()->installEventFilter(this);
    thumb->installEventFilter(this);
    installEventFilter(this);

    thumb->setColor(QColor(0x3f3f3f));
    thumb->installEventFilter(this);

    scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    scrollAnimation->setUpdateInterval(0);
    connect(scrollAnimation, &QTimeLine::frameChanged, this, [=](int value) {
        int maxContentOffset = contentWidget->height() - height();
        currentOffset = qBound(0, value, maxContentOffset);
        contentWidget->move(0, -currentOffset);
        int maxThumbOffset = height() - thumb->height();
        thumbOffset = currentOffset * maxThumbOffset / maxContentOffset;
        thumb->move(width() - 13, thumbOffset);
    });
    updateScrollBar();
}

CustomScrollArea::~CustomScrollArea() {
    delete contentWidget;
}

void CustomScrollArea::resizeEvent(QResizeEvent* ev) {
    QAbstractScrollArea::resizeEvent(ev);
    contentWidget->resize(viewport()->width(), contentWidget->height());
    layoutContent();
    updateScrollBar();
}

bool CustomScrollArea::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() == QEvent::Enter) {
        int contentHeight = contentWidget->height();
        int viewportHeight = height();

        if (contentHeight <= viewportHeight) {
            thumb->hide();
        }
        else {
            thumb->show();
            thumb->setVisible(contentHeight > viewportHeight);
            auto *anim = new QPropertyAnimation(thumb->graphicsEffect(), "opacity", this);
            anim->setDuration(200);
            anim->setStartValue(thumb->graphicsEffect()->property("opacity").toDouble());
            anim->setEndValue(1.0);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    else if (ev->type() == QEvent::Leave) {
        auto *anim = new QPropertyAnimation(thumb->graphicsEffect(), "opacity", this);
        anim->setDuration(200);
        anim->setStartValue(thumb->graphicsEffect()->property("opacity").toDouble());
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, thumb, &QWidget::hide);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    // 滚动条拖动逻辑
    else if (obj == thumb) {
        if (ev->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::LeftButton) {
                dragging = true;
                dragStartY = me->globalPosition().y();
                thumbOffsetAtStart = thumbOffset;
                return true;
            }
        } else if (ev->type() == QEvent::MouseMove && dragging && thumb->isVisible()) {
            auto *me = static_cast<QMouseEvent*>(ev);
            int deltaY = me->globalPosition().y() - dragStartY;
            int maxThumbOffset = height() - thumb->height();
            thumbOffset = qBound(0, thumbOffsetAtStart + deltaY, maxThumbOffset);
            if (thumbOffset <= 10) {
                emit reachedTop();
            }
            if (thumbOffset >= maxThumbOffset - 10) {
                emit reachedBottom();
            }
            int maxContentOffset = contentWidget->height() - height();
            currentOffset = thumbOffset * maxContentOffset / maxThumbOffset;

            contentWidget->move(0, -currentOffset);
            thumb->move(width() - 13, thumbOffset);
            return true;
        } else if (ev->type() == QEvent::MouseButtonRelease && dragging) {
            dragging = false;
            return true;
        }
    }
    return QAbstractScrollArea::eventFilter(obj, ev);
}

void CustomScrollArea::wheelEvent(QWheelEvent* ev) {
    if (contentWidget->height() <= height()) {
        ev->ignore();
        return;
    }
    // 正常滚动逻辑
    const int delta = -ev->angleDelta().y();
    animateTo(currentOffset + (delta>0?240:-240));
    ev->accept();
}

void CustomScrollArea::animateTo(int targetY) {
    int maxOffset = contentWidget->height() - height();
    // 无滚动空间时直接返回
    if (maxOffset <= 0) return;
    scrollAnimation->stop();
    targetY = qBound(0, targetY, maxOffset);
    if (targetY <= 10) {
        emit reachedTop();
    }
    if (targetY >= maxOffset - 10) {
        emit reachedBottom();
    }
    scrollAnimation->setFrameRange(currentOffset, targetY);
    scrollAnimation->start();
}

void CustomScrollArea::updateScrollBar() {
    int contentHeight = contentWidget->height();
    int viewportHeight = height();
    // 当内容不需滚动时，隐藏 thumb 并返回
    if (contentHeight <= viewportHeight) {
        thumb->setVisible(false);
        return;
    }
    // 否则显示并正常计算 thumb 大小和位置
    thumb->show();
    int thumbHeight = viewportHeight * viewportHeight / contentHeight;
    thumbHeight = qBound(qMin(20, viewportHeight),
                         thumbHeight,
                         qMax(20, viewportHeight));
    thumb->setGeometry(width() - 13, thumbOffset, 8, thumbHeight);
}
