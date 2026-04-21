#include "ChatListView.h"

#include <QAbstractItemModel>
#include <QScrollBar>
#include <QTimer>

ChatListView::ChatListView(QWidget *parent)
    : OverlayScrollListView(parent)
{
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setScrollBarInsets(8, 4);
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
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
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
    const bool shouldAutoScroll
            = (verticalScrollBar()->maximum() - verticalScrollBar()->value()) <= kAutoScrollThreshold;

    QTimer::singleShot(0, this, [this, shouldAutoScroll]() {
        updateOverlayScrollBar();
        if (shouldAutoScroll) {
            scrollToBottom();
        }
    });
}
