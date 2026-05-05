#pragma once

#include <QObject>
#include <QVector>

#include "shared/types/FriendNotification.h"

struct FriendNotificationListRequest {
    int offset = 0;
    int limit = 20;
};

class FriendNotificationRepository : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(FriendNotificationRepository)

public:
    static FriendNotificationRepository& instance();

    QVector<FriendNotification> requestNotificationList(
        const FriendNotificationListRequest& query = {}) const;

    int unreadCount() const;
    int notificationCount() const;
    void markAllRead();
    bool acceptRequest(const QString& notificationId);
    bool rejectRequest(const QString& notificationId);

signals:
    void notificationListChanged();

private:
    explicit FriendNotificationRepository(QObject* parent = nullptr);
    ~FriendNotificationRepository() override = default;

    void ensureLoaded() const;

    mutable bool m_loaded = false;
    QVector<FriendNotification> m_notifications;
};
