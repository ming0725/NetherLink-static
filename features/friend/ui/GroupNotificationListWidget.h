#pragma once

#include "shared/ui/OverlayScrollListView.h"

#include <QPersistentModelIndex>
#include <QVector>

#include "shared/types/GroupNotification.h"

class GroupNotificationDelegate;
class GroupNotificationListModel;

class GroupNotificationListWidget : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit GroupNotificationListWidget(QWidget* parent = nullptr);

    void setNotifications(QVector<GroupNotification> notifications);
    void appendNotifications(QVector<GroupNotification> notifications);
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

    GroupNotificationListModel* m_model;
    GroupNotificationDelegate* m_delegate;
    QPersistentModelIndex m_hoveredIndex;
};
