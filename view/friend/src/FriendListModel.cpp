#include "FriendListModel.h"

#include <algorithm>
#include <QCollator>
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
    sortFriends();
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

void FriendListModel::sortFriends()
{
    std::sort(m_friends.begin(), m_friends.end(),
              [](const FriendSummary& lhs, const FriendSummary& rhs) {
                  if (lhs.status != Offline && rhs.status == Offline) {
                      return true;
                  }
                  if (lhs.status == Offline && rhs.status != Offline) {
                      return false;
                  }
                  static QCollator localCollator(QLocale::Chinese);
                  localCollator.setNumericMode(true);
                  return localCollator.compare(lhs.displayName, rhs.displayName) < 0;
              });
}
