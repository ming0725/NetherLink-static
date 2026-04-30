#pragma once

#include <QObject>

#include "shared/types/Group.h"
#include "shared/types/RepositoryTypes.h"
#include "shared/types/User.h"

class ChatSessionController : public QObject
{
    Q_OBJECT

public:
    explicit ChatSessionController(QObject* parent = nullptr);

    void open(const ConversationMeta& meta);
    void close();

    QString conversationId() const;
    bool isGroup() const;
    ConversationMeta meta() const;
    User directUser() const;
    Group group() const;
    bool canEditGroupInfo() const;
    bool canExitGroup() const;
    void loadPanelData();
    void loadGroupMembersPage(const QString& keyword, int offset, int limit);
    void cancelPanelLoads();

public slots:
    void saveGroupName(const QString& name);
    void saveGroupIntroduction(const QString& introduction);
    void saveGroupAnnouncement(const QString& announcement);
    void saveCurrentUserGroupNickname(const QString& nickname);
    void saveGroupMemberNickname(const QString& userId, const QString& nickname);
    void promoteGroupMemberToAdmin(const QString& userId);
    void cancelGroupMemberAdmin(const QString& userId);
    void removeGroupMember(const QString& userId);
    void saveGroupRemark(const QString& remark);
    void saveDirectRemark(const QString& remark);
    void setPinned(bool pinned);
    void setDoNotDisturb(bool enabled);
    void clearMessages();
    void deleteFriend();
    void exitGroup();

signals:
    void sessionChanged(const ConversationMeta& meta,
                        const User& directUser,
                        const Group& group);
    void directPanelDataLoaded(const ConversationMeta& meta,
                               const User& directUser);
    void groupPanelDataLoaded(const ConversationMeta& meta,
                              const Group& group,
                              const QVector<User>& previewMembers,
                              int totalMembers,
                              bool canEditGroupInfo,
                              bool canExitGroup);
    void groupMembersPageLoaded(const ConversationMeta& meta,
                                const GroupMembersPage& page);
    void messagesCleared();
    void conversationRemoved();

private:
    void refreshSessionData(bool emitChange);
    bool hasCurrentConversation(const QString& changedConversationId) const;
    bool canEditGroupInfo(const Group& group) const;
    bool canExitGroup(const Group& group) const;
    bool canEditMemberNickname(const Group& group, const QString& userId) const;
    bool canPromoteMemberToAdmin(const Group& group, const QString& userId) const;
    bool canCancelMemberAdmin(const Group& group, const QString& userId) const;
    bool canRemoveMember(const Group& group, const QString& userId) const;
    void saveGroupField(const QString& value, void (*assign)(Group&, const QString&));

    ConversationMeta m_meta;
    User m_directUser;
    Group m_group;
    int m_panelLoadToken = 0;
    int m_memberPageLoadToken = 0;
};
