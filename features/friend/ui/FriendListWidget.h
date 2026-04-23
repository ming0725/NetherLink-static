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
    void setKeyword(const QString& keyword);

    QString selectedFriendId() const;
    FriendSummary selectedFriend() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct ViewState {
        QString keyword;
        QString selectedFriendId;
        bool initialized = false;
    };

private slots:
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void reloadFriends();

private:
    void restoreSelection();

    FriendListModel* m_model;
    FriendListDelegate* m_delegate;
    ViewState m_state;
};
