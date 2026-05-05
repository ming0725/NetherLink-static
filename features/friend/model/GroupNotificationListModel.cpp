#include "GroupNotificationListModel.h"

#include <QSize>

GroupNotificationListModel::GroupNotificationListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int GroupNotificationListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_notifications.size() + 1;
}

QVariant GroupNotificationListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() > m_notifications.size()) {
        return {};
    }

    if (index.row() == m_notifications.size()) {
        if (role == BottomSpaceRole) {
            return true;
        }
        if (role == Qt::SizeHintRole) {
            return QSize(0, kBottomSpaceHeight);
        }
        return {};
    }

    const GroupNotification& notification = m_notifications.at(index.row());
    switch (role) {
    case IdRole:
        return notification.id;
    case TypeRole:
        return static_cast<int>(notification.type);
    case StatusRole:
        return static_cast<int>(notification.status);
    case GroupIdRole:
        return notification.groupId;
    case ActorUserIdRole:
        return notification.actorUserId;
    case OperatorUserIdRole:
        return notification.operatorUserId;
    case MessageRole:
        return notification.message;
    case CreatedAtRole:
        return notification.createdAt;
    case ActorRoleRole:
        return static_cast<int>(notification.actorRole);
    case HoveredButtonRole:
        return m_hoveredButtons.value(notification.id, kNoHoveredButton);
    case BottomSpaceRole:
        return false;
    default:
        return {};
    }
}

void GroupNotificationListModel::setNotifications(QVector<GroupNotification> notifications)
{
    beginResetModel();
    m_notifications = std::move(notifications);
    m_hoveredButtons.clear();
    endResetModel();
}

void GroupNotificationListModel::appendNotifications(QVector<GroupNotification> notifications)
{
    if (notifications.isEmpty()) {
        return;
    }

    const int firstRow = m_notifications.size();
    beginInsertRows(QModelIndex(), firstRow, firstRow + notifications.size() - 1);
    m_notifications += std::move(notifications);
    endInsertRows();
}

void GroupNotificationListModel::setHoveredButton(const QModelIndex& index, int buttonIndex)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_notifications.size()) {
        return;
    }

    const QString& id = m_notifications.at(index.row()).id;
    int& current = m_hoveredButtons[id];
    if (current == buttonIndex) {
        return;
    }
    current = buttonIndex;
    emit dataChanged(index, index, {HoveredButtonRole});
}

int GroupNotificationListModel::notificationCount() const
{
    return m_notifications.size();
}

bool GroupNotificationListModel::isBottomSpace(const QModelIndex& index) const
{
    return index.isValid() && index.row() == m_notifications.size();
}
