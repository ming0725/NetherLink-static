#pragma once

#include "shared/ui/OverlayScrollListView.h"

#include <QPersistentModelIndex>
#include <QVector>

#include "shared/types/FriendNotification.h"

class FriendNotificationListModel;
class FriendNotificationDelegate;

class FriendNotificationListWidget : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit FriendNotificationListWidget(QWidget* parent = nullptr);

    void setNotifications(QVector<FriendNotification> notifications);
    void appendNotifications(QVector<FriendNotification> notifications);
    int notificationCount() const;

signals:
    void acceptRequest(const QString& notificationId);
    void rejectRequest(const QString& notificationId);
    void loadMoreRequested();

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void updateButtonHover(const QPoint& pos);
    void maybeRequestMore();

    FriendNotificationListModel* m_model;
    FriendNotificationDelegate* m_delegate;
    QPersistentModelIndex m_hoveredIndex;
};
