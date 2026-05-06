#include "OverlayScrollArea.h"

#include <QCursor>
#include <QDateTime>
#include <QEvent>
#include <QLayout>
#include <QPalette>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "SmoothScrollBar.h"

OverlayScrollArea::OverlayScrollArea(QWidget* parent)
    : QAbstractScrollArea(parent)
    , contentWidget(new QWidget(viewport()))
    , m_overlayScrollBar(new SmoothScrollBar(this))
    , m_scrollAnimation(new QVariantAnimation(this))
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);

    viewport()->setMouseTracking(true);

    contentWidget->installEventFilter(this);
    contentWidget->move(0, 0);

    m_overlayScrollBar->hide();
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_scrollAnimation->setDuration(220);

    connect(m_overlayScrollBar, &SmoothScrollBar::valueChanged,
            this, [this](int value) { onOverlayScrollBarValueChanged(value); });
    connect(verticalScrollBar(), &QScrollBar::rangeChanged,
            this, [this](int minimum, int maximum) { onVerticalScrollRangeChanged(minimum, maximum); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) { onVerticalScrollValueChanged(value); });
    connect(m_scrollAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        const int animatedValue = value.toInt();
        m_animatedScrollValue = animatedValue;
        if (verticalScrollBar()->value() != animatedValue) {
            verticalScrollBar()->setValue(animatedValue);
        } else {
            syncContentPosition();
        }
    });
}

QVBoxLayout* OverlayScrollArea::useVerticalContentLayout(const QMargins& margins, int spacing)
{
    auto* layout = qobject_cast<QVBoxLayout*>(contentWidget->layout());
    if (!layout) {
        delete contentWidget->layout();
        layout = new QVBoxLayout(contentWidget);
    }

    layout->setContentsMargins(margins);
    layout->setSpacing(spacing);
    scheduleContentLayout();
    return layout;
}

QLayout* OverlayScrollArea::contentLayout() const
{
    return contentWidget->layout();
}

void OverlayScrollArea::setWheelStepPixels(int pixels)
{
    m_wheelStepPixels = qMax(24, pixels);
}

void OverlayScrollArea::setScrollBarInsets(int topBottomInset, int rightInset)
{
    m_scrollBarInset = qMax(0, topBottomInset);
    m_scrollBarRightInset = qMax(0, rightInset);
    updateOverlayScrollBarGeometry();
}

void OverlayScrollArea::setViewportBackgroundColor(const QColor& color)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    viewport()->setAttribute(Qt::WA_TranslucentBackground, false);
    viewport()->setAttribute(Qt::WA_NoSystemBackground, false);
    QPalette palette = viewport()->palette();
    palette.setColor(QPalette::Base, color);
    palette.setColor(QPalette::Window, color);
    viewport()->setPalette(palette);
    viewport()->setAutoFillBackground(true);
    setAutoFillBackground(false);
    contentWidget->setAutoFillBackground(false);
}

void OverlayScrollArea::clearViewportBackground()
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    viewport()->setAutoFillBackground(false);
    viewport()->setAttribute(Qt::WA_TranslucentBackground);
    viewport()->setAttribute(Qt::WA_NoSystemBackground);
    contentWidget->setAutoFillBackground(false);
    contentWidget->setAttribute(Qt::WA_TranslucentBackground);
    contentWidget->setAttribute(Qt::WA_NoSystemBackground);

    QPalette palette = viewport()->palette();
    palette.setColor(QPalette::Base, Qt::transparent);
    palette.setColor(QPalette::Window, Qt::transparent);
    viewport()->setPalette(palette);
}

void OverlayScrollArea::relayoutContent()
{
    refreshContentLayout();
}

void OverlayScrollArea::layoutContent()
{
    const int contentWidth = viewport()->width();
    contentWidget->setFixedWidth(contentWidth);

    if (QLayout* layout = contentWidget->layout()) {
        layout->invalidate();
        layout->activate();
    }

    QSize hint = contentWidget->sizeHint();
    if (QLayout* layout = contentWidget->layout()) {
        hint = layout->sizeHint();
        if (layout->hasHeightForWidth()) {
            hint.setHeight(layout->heightForWidth(contentWidth));
        }
    }

    const int contentHeight = qMax(0, hint.height());
    contentWidget->setFixedSize(contentWidth, contentHeight);

    if (QLayout* layout = contentWidget->layout()) {
        layout->setGeometry(QRect(QPoint(0, 0), contentWidget->size()));
    }
}

void OverlayScrollArea::refreshContentLayout()
{
    if (m_refreshingLayout) {
        return;
    }

    m_refreshingLayout = true;
    m_layoutScheduled = false;
    layoutContent();
    updateScrollMetrics();
    syncContentPosition();
    updateOverlayScrollBar();
    m_refreshingLayout = false;
}

void OverlayScrollArea::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    refreshContentLayout();
}

void OverlayScrollArea::wheelEvent(QWheelEvent* event)
{
    if (!canScroll()) {
        event->accept();
        return;
    }

    if (!shouldUseAnimatedWheel(event)) {
        m_scrollAnimation->stop();
        handlePrecisionWheel(event);
        return;
    }

    if (event->angleDelta().y() == 0) {
        handlePrecisionWheel(event);
        return;
    }

    QScrollBar* scrollBar = verticalScrollBar();
    const int startValue = (m_scrollAnimation->state() == QAbstractAnimation::Running)
            ? m_animatedScrollValue
            : scrollBar->value();
    const int targetValue = nextAnimatedWheelTarget(event, startValue);

    m_scrollAnimation->stop();
    m_scrollAnimation->setStartValue(startValue);
    m_scrollAnimation->setEndValue(targetValue);
    m_scrollAnimation->start();

    showOverlayScrollBar();
    event->accept();
}

void OverlayScrollArea::enterEvent(QEnterEvent* event)
{
    QAbstractScrollArea::enterEvent(event);
    m_hovered = true;
    showOverlayScrollBar();
}

void OverlayScrollArea::leaveEvent(QEvent* event)
{
    QAbstractScrollArea::leaveEvent(event);
    m_hovered = false;

    const QPoint localPos = mapFromGlobal(QCursor::pos());
    if (!rect().contains(localPos) && !m_overlayScrollBar->geometry().contains(localPos)) {
        m_overlayScrollBar->startFadeOut();
    }
}

bool OverlayScrollArea::viewportEvent(QEvent* event)
{
    switch (event->type()) {
    case QEvent::Enter:
    case QEvent::MouseMove:
        showOverlayScrollBar();
        break;
    case QEvent::Leave: {
        const QPoint localPos = mapFromGlobal(QCursor::pos());
        if (!rect().contains(localPos) && !m_overlayScrollBar->geometry().contains(localPos)) {
            m_overlayScrollBar->startFadeOut();
        }
        break;
    }
    default:
        break;
    }

    return QAbstractScrollArea::viewportEvent(event);
}

void OverlayScrollArea::scheduleContentLayout()
{
    if (m_layoutScheduled || m_refreshingLayout) {
        return;
    }

    m_layoutScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        if (!m_layoutScheduled) {
            return;
        }
        refreshContentLayout();
    });
}

void OverlayScrollArea::showOverlayScrollBar()
{
    updateOverlayScrollBar();
    if (canScroll()) {
        m_overlayScrollBar->showScrollBar();
        m_overlayScrollBar->raise();
    }
}

void OverlayScrollArea::updateOverlayScrollBar()
{
    updateScrollMetrics();

    QScrollBar* scrollBar = verticalScrollBar();
    m_overlayScrollBar->setRange(scrollBar->minimum(), scrollBar->maximum());
    m_overlayScrollBar->setPageStep(scrollBar->pageStep());
    m_overlayScrollBar->setValue(scrollBar->value());
    updateOverlayScrollBarGeometry();

    if (!canScroll()) {
        m_overlayScrollBar->hide();
    } else if (m_hovered || m_overlayScrollBar->isDragging()) {
        m_overlayScrollBar->showScrollBar();
    }
}

void OverlayScrollArea::updateScrollMetrics()
{
    const int maxOffset = qMax(0, contentWidget->height() - viewport()->height());
    verticalScrollBar()->setRange(0, maxOffset);
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(24);
    if (verticalScrollBar()->value() > maxOffset) {
        verticalScrollBar()->setValue(maxOffset);
    }
}

void OverlayScrollArea::syncContentPosition()
{
    contentWidget->move(0, -verticalScrollBar()->value());
}

void OverlayScrollArea::updateOverlayScrollBarGeometry()
{
    const int topInset = m_scrollBarInset;
    const int bottomInset = m_scrollBarInset;
    const int heightValue = qMax(0, height() - topInset - bottomInset);
    m_overlayScrollBar->setGeometry(width() - m_overlayScrollBar->width() - m_scrollBarRightInset,
                                    topInset,
                                    m_overlayScrollBar->width(),
                                    heightValue);
    m_overlayScrollBar->raise();
}

bool OverlayScrollArea::canScroll() const
{
    return verticalScrollBar()->maximum() > verticalScrollBar()->minimum();
}

bool OverlayScrollArea::shouldUseAnimatedWheel(const QWheelEvent* event) const
{
#ifdef Q_OS_MACOS
    Q_UNUSED(event);
    return false;
#else
    return event->pixelDelta().isNull() && event->phase() == Qt::NoScrollPhase;
#endif
}

void OverlayScrollArea::handlePrecisionWheel(QWheelEvent* event)
{
    QScrollBar* scrollBar = verticalScrollBar();
    const QPoint pixelDelta = event->pixelDelta();

    int scrollDelta = 0;
    if (!pixelDelta.isNull()) {
        scrollDelta = qRound(-pixelDelta.y() * m_precisionScrollScale);
    } else if (event->angleDelta().y() != 0) {
        const qreal steps = qreal(event->angleDelta().y()) / 120.0;
        scrollDelta = qRound(-steps * m_wheelStepPixels * m_precisionScrollScale);
    }

    if (scrollDelta != 0) {
        scrollBar->setValue(qBound(scrollBar->minimum(),
                                   scrollBar->value() + scrollDelta,
                                   scrollBar->maximum()));
    }

    updateOverlayScrollBar();
    showOverlayScrollBar();
    event->accept();
}

int OverlayScrollArea::nextAnimatedWheelTarget(const QWheelEvent* event, int startValue)
{
    const int delta = event->angleDelta().y();
    const int direction = (delta > 0) ? 1 : -1;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    if (m_lastWheelDirection == direction && (nowMs - m_lastWheelEventMs) <= 180) {
        ++m_wheelStreak;
    } else {
        m_wheelStreak = 1;
    }

    m_lastWheelDirection = direction;
    m_lastWheelEventMs = nowMs;

    const qreal steps = qAbs(qreal(delta) / 120.0);
    const qreal acceleration = qMin(2.2, 1.0 + 0.18 * (m_wheelStreak - 1));
    const qreal baseDistance = steps * m_wheelStepPixels * acceleration;
    const int scrollDelta = -direction * qRound(baseDistance);
    const int tailDistance = -direction * qMin(18, qRound((6.0 + 4.0 * steps) * acceleration));

    return qBound(verticalScrollBar()->minimum(),
                  startValue + scrollDelta + tailDistance,
                  verticalScrollBar()->maximum());
}

void OverlayScrollArea::onOverlayScrollBarValueChanged(int value)
{
    if (verticalScrollBar()->value() != value) {
        verticalScrollBar()->setValue(value);
    }
}

void OverlayScrollArea::onVerticalScrollRangeChanged(int, int)
{
    updateOverlayScrollBar();
}

void OverlayScrollArea::onVerticalScrollValueChanged(int value)
{
    m_animatedScrollValue = value;
    syncContentPosition();
    updateOverlayScrollBar();
}

bool OverlayScrollArea::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == contentWidget) {
        switch (event->type()) {
        case QEvent::LayoutRequest:
        case QEvent::ChildAdded:
        case QEvent::ChildRemoved:
        case QEvent::Show:
        case QEvent::Hide:
            scheduleContentLayout();
            break;
        default:
            break;
        }
    }

    return QAbstractScrollArea::eventFilter(watched, event);
}
