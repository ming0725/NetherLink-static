#pragma once

#include "shared/ui/OverlayScrollListView.h"
#include "shared/types/RepositoryTypes.h"

class FriendListDelegate;
class FriendListModel;

class FriendListWidget : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit FriendListWidget(QWidget* parent = nullptr);
    void ensureInitialized();

    QString selectedFriendId() const;
    FriendSummary selectedFriend() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    FriendListModel* m_model;
    FriendListDelegate* m_delegate;
    bool m_initialized = false;
};
