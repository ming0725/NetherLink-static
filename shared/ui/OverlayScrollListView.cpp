#include "OverlayScrollListView.h"

#include <QCursor>
#include <QDateTime>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include "SmoothScrollBar.h"

OverlayScrollListView::OverlayScrollListView(QWidget* parent)
    : QListView(parent)
    , m_overlayScrollBar(new SmoothScrollBar(this))
    , m_scrollAnimation(new QPropertyAnimation(this, "animatedScrollValue", this))
{
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    m_overlayScrollBar->hide();
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_scrollAnimation->setDuration(220);
    verticalScrollBar()->setSingleStep(24);

    connect(m_overlayScrollBar, &SmoothScrollBar::valueChanged,
            this, &OverlayScrollListView::onOverlayScrollBarValueChanged);
    connect(verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &OverlayScrollListView::onVerticalScrollRangeChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &OverlayScrollListView::onVerticalScrollValueChanged);
}

void OverlayScrollListView::setWheelStepPixels(int pixels)
{
    m_wheelStepPixels = qMax(24, pixels);
}

void OverlayScrollListView::setScrollBarInsets(int topBottomInset, int rightInset)
{
    m_scrollBarInset = qMax(0, topBottomInset);
    m_scrollBarRightInset = qMax(0, rightInset);
    updateOverlayScrollBarGeometry();
}

void OverlayScrollListView::resizeEvent(QResizeEvent* event)
{
    QListView::resizeEvent(event);
    updateOverlayScrollBarGeometry();
    updateOverlayScrollBar();
}

void OverlayScrollListView::wheelEvent(QWheelEvent* event)
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
    const int startValue = (m_scrollAnimation->state() == QPropertyAnimation::Running)
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

void OverlayScrollListView::enterEvent(QEnterEvent* event)
{
    QListView::enterEvent(event);
    m_hovered = true;
    showOverlayScrollBar();
}

void OverlayScrollListView::leaveEvent(QEvent* event)
{
    QListView::leaveEvent(event);
    m_hovered = false;

    const QPoint localPos = mapFromGlobal(QCursor::pos());
    if (!rect().contains(localPos) && !m_overlayScrollBar->geometry().contains(localPos)) {
        m_overlayScrollBar->startFadeOut();
    }
}

void OverlayScrollListView::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        showOverlayScrollBar();
        event->accept();
        return;
    }

    QListView::mouseMoveEvent(event);
}

bool OverlayScrollListView::viewportEvent(QEvent* event)
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

    return QListView::viewportEvent(event);
}

void OverlayScrollListView::showOverlayScrollBar()
{
    updateOverlayScrollBar();
    if (canScroll()) {
        m_overlayScrollBar->showScrollBar();
        m_overlayScrollBar->raise();
    }
}

void OverlayScrollListView::updateOverlayScrollBar()
{
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

void OverlayScrollListView::onOverlayScrollBarValueChanged(int value)
{
    if (verticalScrollBar()->value() != value) {
        verticalScrollBar()->setValue(value);
    }
}

void OverlayScrollListView::onVerticalScrollRangeChanged(int, int)
{
    updateOverlayScrollBar();
}

void OverlayScrollListView::onVerticalScrollValueChanged(int value)
{
    m_animatedScrollValue = value;
    updateOverlayScrollBar();
}

void OverlayScrollListView::setAnimatedScrollValue(int value)
{
    if (m_animatedScrollValue == value) {
        return;
    }

    m_animatedScrollValue = value;
    if (verticalScrollBar()->value() != value) {
        verticalScrollBar()->setValue(value);
    }
}

void OverlayScrollListView::updateOverlayScrollBarGeometry()
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

bool OverlayScrollListView::canScroll() const
{
    return verticalScrollBar()->maximum() > verticalScrollBar()->minimum();
}

bool OverlayScrollListView::shouldUseAnimatedWheel(const QWheelEvent* event) const
{
#ifdef Q_OS_MACOS
    Q_UNUSED(event);
    return false;
#else
    return event->pixelDelta().isNull() && event->phase() == Qt::NoScrollPhase;
#endif
}

void OverlayScrollListView::handlePrecisionWheel(QWheelEvent* event)
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

int OverlayScrollListView::nextAnimatedWheelTarget(const QWheelEvent* event, int startValue)
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
