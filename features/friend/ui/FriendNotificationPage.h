#pragma once

#include <QWidget>

#include <QVector>

#include "shared/types/FriendNotification.h"

class FriendNotificationListWidget;

class FriendNotificationPage : public QWidget
{
    Q_OBJECT

public:
    explicit FriendNotificationPage(QWidget* parent = nullptr);

    void reloadNotifications();
    void refreshLoadedNotifications();

signals:
    void acceptRequest(const QString& notificationId);
    void rejectRequest(const QString& notificationId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyTheme();
    void loadMoreNotifications();

    FriendNotificationListWidget* m_listWidget;
    QWidget* m_header;
    int m_loadedCount = 0;
    bool m_loading = false;
    bool m_hasMore = true;
};
