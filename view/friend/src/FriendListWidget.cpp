#include "FriendListWidget.h"

#include <QItemSelectionModel>
#include <QMouseEvent>

#include "FriendListDelegate.h"
#include "FriendListModel.h"
#include "UserRepository.h"

FriendListWidget::FriendListWidget(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new FriendListModel(this))
    , m_delegate(new FriendListDelegate(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setUniformItemSizes(true);
    setSpacing(0);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setStyleSheet("border-width:0px;border-style:solid;background:#ffffff;");
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    m_model->setFriends(UserRepository::instance().requestFriendList());
}

QString FriendListWidget::selectedFriendId() const
{
    return m_model->friendIdAt(currentIndex());
}

FriendSummary FriendListWidget::selectedFriend() const
{
    return m_model->friendAt(currentIndex());
}

void FriendListWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    if (currentIndex() == index) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    OverlayScrollListView::mousePressEvent(event);
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}
