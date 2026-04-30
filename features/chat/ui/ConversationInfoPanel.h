#pragma once

#include <QPoint>
#include <QWidget>

#include "shared/types/Group.h"
#include "shared/types/RepositoryTypes.h"
#include "shared/types/User.h"

class RedstoneLampSwitch;
class InlineEditableText;
class OverlayScrollArea;
class StatefulPushButton;
class QString;

class ConversationInfoPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConversationInfoPanel(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

class GroupConversationInfoPanel : public ConversationInfoPanel
{
    Q_OBJECT

public:
    explicit GroupConversationInfoPanel(QWidget* parent = nullptr);

    void setGroupSummary(const Group& group,
                         const ConversationMeta& meta,
                         const QVector<User>& previewMembers,
                         int totalMembers,
                         bool canEditGroupInfo,
                         bool canExitGroup);
    void appendGroupMembersPage(const GroupMembersPage& page);
    void resetTransientState();
    void releaseTransientResources();

signals:
    void groupNameChanged(const QString& name);
    void groupIntroductionChanged(const QString& introduction);
    void groupAnnouncementChanged(const QString& announcement);
    void currentUserNicknameChanged(const QString& nickname);
    void groupMemberNicknameChanged(const QString& userId, const QString& nickname);
    void groupMemberAdminPromotionRequested(const QString& userId);
    void groupMemberAdminCancellationRequested(const QString& userId);
    void groupMemberRemovalRequested(const QString& userId);
    void groupRemarkChanged(const QString& remark);
    void pinChanged(bool pinned);
    void doNotDisturbChanged(bool enabled);
    void clearChatHistoryRequested();
    void exitGroupRequested();
    void groupMembersPageRequested(const QString& keyword, int offset, int limit);

private:
    InlineEditableText* m_groupNameText = nullptr;
    InlineEditableText* m_groupIntroductionText = nullptr;
    InlineEditableText* m_groupAnnouncementText = nullptr;
    InlineEditableText* m_currentUserNicknameText = nullptr;
    InlineEditableText* m_groupRemarkText = nullptr;
    RedstoneLampSwitch* m_pinSwitch = nullptr;
    RedstoneLampSwitch* m_doNotDisturbSwitch = nullptr;
    StatefulPushButton* m_clearHistoryButton = nullptr;
    StatefulPushButton* m_exitGroupButton = nullptr;
    QWidget* m_groupInfoCard = nullptr;
    QWidget* m_memberSummaryCard = nullptr;
    QWidget* m_memberSummaryHeader = nullptr;
    QWidget* m_memberListPage = nullptr;
    QWidget* m_memberListHeader = nullptr;
    OverlayScrollArea* m_mainScrollArea = nullptr;
    OverlayScrollArea* m_memberListScrollArea = nullptr;
    class QStackedWidget* m_stack = nullptr;
    class QVBoxLayout* m_memberPreviewLayout = nullptr;
    class QVBoxLayout* m_memberFullListLayout = nullptr;
    class IconLineEdit* m_memberSearchInput = nullptr;
    Group m_group;
    QVector<User> m_members;
    QString m_memberKeyword;
    int m_memberTotalCount = 0;
    int m_memberLoadedCount = 0;
    bool m_memberHasMore = false;
    bool m_memberLoading = false;
    bool m_canEditGroupInfo = false;
    bool m_canExitGroup = false;

    void rebuildMemberPreview();
    void clearMemberFullList();
    void requestNextMemberPage();
    void showMainPage();
    void showMemberListPage();
    void showMemberContextMenu(const User& user, const QPoint& globalPos);
    void promptMemberNicknameChange(const User& user);
    void confirmMemberRemoval(const User& user);
    bool canEditMemberNickname(const User& user) const;
    bool canPromoteMemberToAdmin(const User& user) const;
    bool canCancelMemberAdmin(const User& user) const;
    bool canRemoveMember(const User& user) const;
};

class DirectConversationInfoPanel : public ConversationInfoPanel
{
    Q_OBJECT

public:
    explicit DirectConversationInfoPanel(QWidget* parent = nullptr);

    void setConversationMeta(const ConversationMeta& meta,
                             const QString& remark,
                             bool animated = false);

signals:
    void remarkChanged(const QString& remark);
    void pinChanged(bool pinned);
    void doNotDisturbChanged(bool enabled);
    void clearChatHistoryRequested();
    void deleteFriendRequested();

private:
    InlineEditableText* m_remarkText = nullptr;
    RedstoneLampSwitch* m_pinSwitch = nullptr;
    RedstoneLampSwitch* m_doNotDisturbSwitch = nullptr;
    StatefulPushButton* m_clearHistoryButton = nullptr;
    StatefulPushButton* m_deleteFriendButton = nullptr;
};
