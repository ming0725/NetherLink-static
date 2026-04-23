#include "FriendListWidget.h"

#include <QItemSelectionModel>
#include <QMouseEvent>

#include "features/friend/ui/FriendListDelegate.h"
#include "features/friend/model/FriendListModel.h"
#include "features/friend/data/UserRepository.h"

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

    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &FriendListWidget::onCurrentChanged);
    connect(&UserRepository::instance(), &UserRepository::friendListChanged,
            this, &FriendListWidget::reloadFriends);
}

void FriendListWidget::ensureInitialized()
{
    if (m_state.initialized) {
        return;
    }

    m_state.initialized = true;
    reloadFriends();
}

void FriendListWidget::setKeyword(const QString& keyword)
{
    if (m_state.keyword == keyword) {
        return;
    }

    m_state.keyword = keyword;
    if (m_state.initialized) {
        reloadFriends();
    }
}

QString FriendListWidget::selectedFriendId() const
{
    return m_model->friendIdAt(currentIndex());
}

FriendSummary FriendListWidget::selectedFriend() const
{
    return m_model->friendAt(currentIndex());
}

void FriendListWidget::showEvent(QShowEvent* event)
{
    OverlayScrollListView::showEvent(event);
    ensureInitialized();
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
        m_state.selectedFriendId.clear();
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    if (currentIndex() == index) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        m_state.selectedFriendId.clear();
        return;
    }

    OverlayScrollListView::mousePressEvent(event);
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void FriendListWidget::onCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    m_state.selectedFriendId = m_model->friendIdAt(current);
}

void FriendListWidget::reloadFriends()
{
    if (!m_state.initialized) {
        return;
    }

    m_model->setFriends(UserRepository::instance().requestFriendList({m_state.keyword}));
    restoreSelection();
}

void FriendListWidget::restoreSelection()
{
    if (m_state.selectedFriendId.isEmpty()) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    const int row = m_model->indexOfFriend(m_state.selectedFriendId);
    if (row < 0) {
        m_state.selectedFriendId.clear();
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    const QModelIndex index = m_model->index(row, 0);
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}
