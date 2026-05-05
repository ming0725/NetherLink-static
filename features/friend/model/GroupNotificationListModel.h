#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "shared/types/GroupNotification.h"

class GroupNotificationListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TypeRole,
        StatusRole,
        GroupIdRole,
        ActorUserIdRole,
        OperatorUserIdRole,
        MessageRole,
        CreatedAtRole,
        ActorRoleRole,
        HoveredButtonRole,
        BottomSpaceRole
    };

    static constexpr int kBottomSpaceHeight = 48;
    static constexpr int kNoHoveredButton = -1;
    static constexpr int kHoveredItemNoButton = -2;

    explicit GroupNotificationListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setNotifications(QVector<GroupNotification> notifications);
    void appendNotifications(QVector<GroupNotification> notifications);
    void setHoveredButton(const QModelIndex& index, int buttonIndex);
    int notificationCount() const;
    bool isBottomSpace(const QModelIndex& index) const;

private:
    QVector<GroupNotification> m_notifications;
    QHash<QString, int> m_hoveredButtons;
};
