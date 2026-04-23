#include "FriendListModel.h"

#include <QSize>

FriendListModel::FriendListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int FriendListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_friends.size());
}

QVariant FriendListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_friends.size()) {
        return {};
    }

    const FriendSummary& friendSummary = m_friends.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole:
        return friendSummary.displayName;
    case UserIdRole:
        return friendSummary.userId;
    case AvatarPathRole:
        return friendSummary.avatarPath;
    case StatusRole:
        return static_cast<int>(friendSummary.status);
    case SignatureRole:
        return friendSummary.signature;
    case DoNotDisturbRole:
        return friendSummary.isDoNotDisturb;
    case Qt::SizeHintRole:
        return QSize(0, 72);
    default:
        return {};
    }
}

Qt::ItemFlags FriendListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void FriendListModel::setFriends(QVector<FriendSummary> friends)
{
    beginResetModel();
    m_friends = std::move(friends);
    endResetModel();
}

QString FriendListModel::friendIdAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_friends.size()) {
        return {};
    }
    return m_friends.at(index.row()).userId;
}

FriendSummary FriendListModel::friendAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_friends.size()) {
        return {};
    }
    return m_friends.at(index.row());
}

int FriendListModel::indexOfFriend(const QString& userId) const
{
    for (int row = 0; row < m_friends.size(); ++row) {
        if (m_friends.at(row).userId == userId) {
            return row;
        }
    }
    return -1;
}
