#pragma once

#include <QWidget>

class GroupNotificationListWidget;
class FriendSessionController;

class GroupNotificationPage : public QWidget
{
    Q_OBJECT

public:
    explicit GroupNotificationPage(QWidget* parent = nullptr);

    void setController(FriendSessionController* controller);
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

    GroupNotificationListWidget* m_listWidget;
    FriendSessionController* m_controller = nullptr;
    QWidget* m_header;
    int m_loadedCount = 0;
    bool m_loading = false;
    bool m_hasMore = true;
};
