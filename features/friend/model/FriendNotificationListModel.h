#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "shared/types/FriendNotification.h"

class FriendNotificationListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        FromUserIdRole,
        DisplayNameRole,
        AvatarPathRole,
        MessageRole,
        RequestDateRole,
        SourceTextRole,
        StatusRole,
        HoveredButtonRole,
        BottomSpaceRole
    };

    static constexpr int kBottomSpaceHeight = 48;

    explicit FriendNotificationListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setNotifications(QVector<FriendNotification> notifications);
    void appendNotifications(QVector<FriendNotification> notifications);
    void setHoveredButton(const QModelIndex& index, int buttonIndex); // -1 = none, 0 = accept, 1 = reject
    FriendNotification notificationAt(const QModelIndex& index) const;
    int notificationCount() const;
    bool isBottomSpace(const QModelIndex& index) const;

private:
    QVector<FriendNotification> m_notifications;
    QHash<QString, int> m_hoveredButtons; // notification id -> button index
};
