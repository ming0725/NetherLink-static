#include "ChatListView.h"

#include <algorithm>
#include <cmath>

#include <QAbstractItemModel>
#include <QScrollBar>
#include <QTimer>

namespace {

int scrollAnimationDuration(int distance)
{
    if (distance <= 0) {
        return 0;
    }

    constexpr double kMinDurationMs = 200.0;
    constexpr double kMaxDurationMs = 800.0;
    constexpr double kDistanceFactor = 0.5;

    const double normalized = std::min(distance / 1000.0, 1.0);
    const double curved = std::pow(normalized, kDistanceFactor);
    const double duration = kMaxDurationMs * curved;
    return static_cast<int>(std::clamp(duration, kMinDurationMs, kMaxDurationMs));
}

} // namespace

ChatListView::ChatListView(QWidget *parent)
    : OverlayScrollListView(parent)
    , m_scrollAnimation(new QPropertyAnimation(verticalScrollBar(), "value", this))
{
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setThemeBackgroundRole(ThemeColor::PageBackground);
    setScrollBarInsets(8, 4);

    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void ChatListView::setModel(QAbstractItemModel *model)
{
    if (this->model()) {
        disconnect(this->model(), &QAbstractItemModel::rowsInserted,
                   this, &ChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::rowsRemoved,
                   this, &ChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::modelReset,
                   this, &ChatListView::onModelRowsChanged);
    }

    OverlayScrollListView::setModel(model);

    if (!model) {
        return;
    }

    connect(model, &QAbstractItemModel::rowsInserted,
            this, &ChatListView::onModelRowsChanged);
    connect(model, &QAbstractItemModel::rowsRemoved,
            this, &ChatListView::onModelRowsChanged);
    connect(model, &QAbstractItemModel::modelReset,
            this, &ChatListView::onModelRowsChanged);

    QTimer::singleShot(0, this, [this]() { updateOverlayScrollBar(); });
}

void ChatListView::scrollToBottom()
{
    doItemsLayout();
    updateGeometries();

    QScrollBar* scrollBar = verticalScrollBar();
    const int targetValue = scrollBar->maximum();
    const int startValue = scrollBar->value();

    if (targetValue <= scrollBar->minimum() || startValue >= targetValue) {
        jumpToBottom();
        return;
    }

    m_scrollAnimation->stop();
    m_scrollAnimation->setDuration(scrollAnimationDuration(targetValue - startValue));
    m_scrollAnimation->setStartValue(startValue);
    m_scrollAnimation->setEndValue(targetValue);
    m_scrollAnimation->start();
}

void ChatListView::jumpToBottom()
{
    m_scrollAnimation->stop();
    doItemsLayout();
    updateGeometries();
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    updateOverlayScrollBar();
}

void ChatListView::preserveScrollPositionAfterPrepend(int previousValue, int previousMaximum)
{
    m_scrollAnimation->stop();
    QTimer::singleShot(0, this, [this, previousValue, previousMaximum]() {
        doItemsLayout();
        updateGeometries();
        QScrollBar* scrollBar = verticalScrollBar();
        const int addedHeight = qMax(0, scrollBar->maximum() - previousMaximum);
        scrollBar->setValue(qBound(scrollBar->minimum(),
                                   previousValue + addedHeight,
                                   scrollBar->maximum()));
        updateOverlayScrollBar();
    });
}

void ChatListView::resizeEvent(QResizeEvent* event)
{
    const bool wasNearBottom = verticalScrollBar()->maximum() - verticalScrollBar()->value() <= 5;

    OverlayScrollListView::resizeEvent(event);
    m_scrollAnimation->stop();
    doItemsLayout();
    updateGeometries();
    if (wasNearBottom) {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
    updateOverlayScrollBar();
}

void ChatListView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() && model()) {
        static_cast<ChatListModel*>(model())->clearSelection();
    }
    OverlayScrollListView::mousePressEvent(event);
}

void ChatListView::onModelRowsChanged()
{
    QTimer::singleShot(0, this, [this]() {
        doItemsLayout();
        updateGeometries();
        updateOverlayScrollBar();
    });
}
