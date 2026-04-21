#pragma once

#include "OverlayScrollListView.h"
#include "RepositoryTypes.h"

class FriendListDelegate;
class FriendListModel;

class FriendListWidget : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit FriendListWidget(QWidget* parent = nullptr);

    QString selectedFriendId() const;
    FriendSummary selectedFriend() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    FriendListModel* m_model;
    FriendListDelegate* m_delegate;
};
