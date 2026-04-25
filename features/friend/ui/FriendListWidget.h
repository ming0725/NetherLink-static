#pragma once

#include "shared/ui/OverlayScrollListView.h"
#include "shared/types/RepositoryTypes.h"

#include <QHash>
#include <QPointer>
#include <QString>

class FriendListDelegate;
class FriendListModel;
class QEvent;
class QPaintEvent;
class QPainter;
class QVariantAnimation;

class FriendListWidget : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit FriendListWidget(QWidget* parent = nullptr);
    void ensureInitialized();
    void setKeyword(const QString& keyword);

    QString selectedFriendId() const;
    FriendSummary selectedFriend() const;

signals:
    void selectedFriendChanged(const QString& userId);

protected:
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
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
    struct StickyGroupData {
        QString groupId;
        QString title;
        int count = 0;
        qreal progress = 0.0;
    };

    struct StickyHeaderState {
        bool visible = false;
        StickyGroupData group;
        int offsetY = 0;
    };

    void restoreSelection();
    void toggleGroup(const QModelIndex& index);
    void toggleStickyGroupById(const QString& groupId);
    void toggleGroupById(const QString& groupId, bool keepGroupAtTop = false);
    void collapseGroupById(const QString& groupId, bool keepGroupAtTop = false);
    void setGroupExpandedAnimated(const QString& groupId, bool expanded, bool keepGroupAtTop = false);
    bool isGroupPinnedAtTop(const QString& groupId) const;
    void scrollGroupToTop(const QString& groupId);
    void clearCurrentSelection();
    void updateStickyHeader();
    StickyHeaderState calculateStickyHeaderState() const;
    StickyGroupData stickyGroupDataForRow(int row) const;
    void drawStickyHeader() const;
    void drawStickyGroup(QPainter* painter,
                         const QRect& rect,
                         const StickyGroupData& group) const;

    FriendListModel* m_model;
    FriendListDelegate* m_delegate;
    ViewState m_state;
    QHash<QString, QPointer<QVariantAnimation>> m_groupAnimations;
    StickyGroupData m_stickyGroup;
    bool m_stickyVisible = false;
    int m_stickyOffsetY = 0;
};
