#include "features/chat/ui/ChatSessionController.h"

#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/UserRepository.h"
#include "app/state/CurrentUser.h"

#include <QCollator>
#include <QPointer>
#include <QSet>
#include <QThread>

#include <algorithm>

namespace {
constexpr int kPanelMemberPreviewLimit = 5;

QString memberNickname(const Group& group, const QString& userId)
{
    const QString storedNickname = group.memberNicknames.value(userId).trimmed();
    if (!storedNickname.isEmpty()) {
        return storedNickname;
    }
    if (userId == CurrentUser::instance().getUserId() && !group.currentUserNickname.trimmed().isEmpty()) {
        return group.currentUserNickname.trimmed();
    }
    return {};
}

QString memberDisplayName(const Group& group, const User& user)
{
    const QString groupNickname = memberNickname(group, user.id);
    if (!groupNickname.isEmpty()) {
        return groupNickname;
    }
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(user.id)) {
        return currentUser.getUserName();
    }
    if (!user.remark.trimmed().isEmpty()) {
        return user.remark.trimmed();
    }
    return user.nick.trimmed().isEmpty() ? user.id : user.nick.trimmed();
}

GroupRole memberRole(const Group& group, const QString& userId)
{
    if (!group.ownerId.isEmpty() && group.ownerId == userId) {
        return GroupRole::Owner;
    }
    if (group.adminsID.contains(userId)) {
        return GroupRole::Admin;
    }
    return GroupRole::Member;
}

GroupRole memberRole(const Group& group, const User& user)
{
    return memberRole(group, user.id);
}

QVector<User> sortedGroupMembers(const Group& group,
                                 const QVector<User>& members,
                                 const QString& keyword = {})
{
    QVector<User> filtered;
    filtered.reserve(members.size());
    const QString normalizedKeyword = keyword.trimmed();
    for (const User& user : members) {
        const QString displayName = memberDisplayName(group, user);
        if (!normalizedKeyword.isEmpty() &&
            !displayName.contains(normalizedKeyword, Qt::CaseInsensitive) &&
            !user.nick.contains(normalizedKeyword, Qt::CaseInsensitive) &&
            !user.remark.contains(normalizedKeyword, Qt::CaseInsensitive)) {
            continue;
        }
        filtered.push_back(user);
    }

    QCollator collator(QLocale::Chinese);
    collator.setNumericMode(true);
    std::sort(filtered.begin(), filtered.end(), [&group, &collator](const User& lhs, const User& rhs) {
        const GroupRole lhsRole = memberRole(group, lhs);
        const GroupRole rhsRole = memberRole(group, rhs);
        const int lhsRank = lhsRole == GroupRole::Owner ? 0 : (lhsRole == GroupRole::Admin ? 1 : 2);
        const int rhsRank = rhsRole == GroupRole::Owner ? 0 : (rhsRole == GroupRole::Admin ? 1 : 2);
        if (lhsRank != rhsRank) {
            return lhsRank < rhsRank;
        }
        const int nameOrder = collator.compare(memberDisplayName(group, lhs), memberDisplayName(group, rhs));
        return nameOrder == 0 ? lhs.id < rhs.id : nameOrder < 0;
    });
    return filtered;
}

QVector<User> requestGroupMembers(const Group& group)
{
    QVector<User> members;
    members.reserve(group.membersID.size());
    for (const QString& memberId : group.membersID) {
        User user;
        const CurrentUser& currentUser = CurrentUser::instance();
        if (currentUser.isCurrentUserId(memberId)) {
            const CurrentUserProfile profile = currentUser.identity();
            user.id = profile.userId;
            user.nick = profile.nickName;
            user.avatarPath = profile.avatarPath;
            user.status = profile.status;
            user.isFriend = false;
        } else {
            user = UserRepository::instance().requestUserDetail({memberId});
        }
        if (!user.id.isEmpty()) {
            members.push_back(user);
        }
    }
    return members;
}

void appendPreviewMember(QVector<User>& members, QSet<QString>& seen, const QString& userId)
{
    if (userId.isEmpty() || seen.contains(userId) || members.size() >= kPanelMemberPreviewLimit) {
        return;
    }

    User user;
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(userId)) {
        const CurrentUserProfile profile = currentUser.identity();
        user.id = profile.userId;
        user.nick = profile.nickName;
        user.avatarPath = profile.avatarPath;
        user.status = profile.status;
        user.isFriend = false;
    } else {
        user = UserRepository::instance().requestUserDetail({userId});
    }
    if (user.id.isEmpty()) {
        return;
    }

    members.push_back(user);
    seen.insert(user.id);
}

QVector<User> requestGroupMemberPreview(const Group& group)
{
    QVector<User> members;
    QSet<QString> seen;
    members.reserve(kPanelMemberPreviewLimit);
    appendPreviewMember(members, seen, group.ownerId);
    for (const QString& adminId : group.adminsID) {
        appendPreviewMember(members, seen, adminId);
    }
    for (const QString& memberId : group.membersID) {
        appendPreviewMember(members, seen, memberId);
    }
    return sortedGroupMembers(group, members);
}
} // namespace

ChatSessionController::ChatSessionController(QObject* parent)
    : QObject(parent)
{
    connect(&MessageRepository::instance(), &MessageRepository::conversationListChanged,
            this, [this](const QString& changedConversationId) {
                if (hasCurrentConversation(changedConversationId)) {
                    refreshSessionData(true);
                }
            });
    connect(&GroupRepository::instance(), &GroupRepository::groupListChanged,
            this, [this]() {
                if (!m_meta.conversationId.isEmpty() && m_meta.isGroup) {
                    refreshSessionData(true);
                }
            });
    connect(&UserRepository::instance(), &UserRepository::friendListChanged,
            this, [this]() {
                if (!m_meta.conversationId.isEmpty() && !m_meta.isGroup) {
                    refreshSessionData(true);
                }
            });
}

void ChatSessionController::open(const ConversationMeta& meta)
{
    cancelPanelLoads();
    m_meta = meta;
    refreshSessionData(true);
}

void ChatSessionController::close()
{
    cancelPanelLoads();
    m_meta = {};
    m_directUser = {};
    m_group = {};
    emit sessionChanged(m_meta, m_directUser, m_group);
}

QString ChatSessionController::conversationId() const
{
    return m_meta.conversationId;
}

bool ChatSessionController::isGroup() const
{
    return m_meta.isGroup;
}

ConversationMeta ChatSessionController::meta() const
{
    return m_meta;
}

User ChatSessionController::directUser() const
{
    return m_directUser;
}

Group ChatSessionController::group() const
{
    return m_group;
}

bool ChatSessionController::canEditGroupInfo() const
{
    return canEditGroupInfo(m_group);
}

bool ChatSessionController::canExitGroup() const
{
    return canExitGroup(m_group);
}

void ChatSessionController::loadPanelData()
{
    if (m_meta.conversationId.isEmpty()) {
        return;
    }

    const int token = ++m_panelLoadToken;
    const ConversationMeta meta = m_meta;
    QPointer<ChatSessionController> controller(this);

    QThread* thread = QThread::create([controller, token, meta]() {
        ConversationMeta loadedMeta = MessageRepository::instance().requestConversationMeta({meta.conversationId});
        if (loadedMeta.conversationId.isEmpty()) {
            loadedMeta = meta;
        }

        if (meta.isGroup) {
            const Group group = GroupRepository::instance().requestGroupDetail({meta.conversationId});
            const QVector<User> previewMembers = requestGroupMemberPreview(group);
            const bool validGroup = !group.groupId.isEmpty();
            const bool canEdit = validGroup &&
                                 (GroupRepository::instance().isCurrentUserGroupOwner(group) ||
                                  GroupRepository::instance().isCurrentUserGroupAdmin(group));
            const bool canExit = validGroup && !GroupRepository::instance().isCurrentUserGroupOwner(group);

            if (!controller) {
                return;
            }
            QMetaObject::invokeMethod(controller.data(), [controller, token, loadedMeta, group, previewMembers, canEdit, canExit]() {
                if (!controller || token != controller->m_panelLoadToken ||
                    loadedMeta.conversationId != controller->m_meta.conversationId) {
                    return;
                }

                controller->m_meta = loadedMeta;
                controller->m_group = group;
                controller->m_directUser = {};
                emit controller->groupPanelDataLoaded(loadedMeta,
                                                      group,
                                                      previewMembers,
                                                      group.memberNum,
                                                      canEdit,
                                                      canExit);
            }, Qt::QueuedConnection);
            return;
        }

        const User directUser = UserRepository::instance().requestUserDetail({meta.conversationId});
        if (!controller) {
            return;
        }
        QMetaObject::invokeMethod(controller.data(), [controller, token, loadedMeta, directUser]() {
            if (!controller || token != controller->m_panelLoadToken ||
                loadedMeta.conversationId != controller->m_meta.conversationId) {
                return;
            }

            controller->m_meta = loadedMeta;
            controller->m_directUser = directUser;
            controller->m_group = {};
            emit controller->directPanelDataLoaded(loadedMeta, directUser);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void ChatSessionController::loadGroupMembersPage(const QString& keyword, int offset, int limit)
{
    if (m_meta.conversationId.isEmpty() || !m_meta.isGroup || limit <= 0) {
        return;
    }

    const int token = ++m_memberPageLoadToken;
    const ConversationMeta meta = m_meta;
    const QString normalizedKeyword = keyword.trimmed();
    const int safeOffset = qMax(0, offset);
    const int safeLimit = qMax(1, limit);
    QPointer<ChatSessionController> controller(this);

    QThread* thread = QThread::create([controller, token, meta, normalizedKeyword, safeOffset, safeLimit]() {
        const Group group = GroupRepository::instance().requestGroupDetail({meta.conversationId});
        const QVector<User> members = sortedGroupMembers(group,
                                                         requestGroupMembers(group),
                                                         normalizedKeyword);
        GroupMembersPage page;
        page.groupId = group.groupId;
        page.keyword = normalizedKeyword;
        page.offset = qBound(0, safeOffset, members.size());
        page.totalCount = members.size();
        page.members = members.mid(page.offset, safeLimit);
        page.hasMore = page.offset + page.members.size() < members.size();

        if (!controller) {
            return;
        }
        QMetaObject::invokeMethod(controller.data(), [controller, token, meta, page]() {
            if (!controller || token != controller->m_memberPageLoadToken ||
                meta.conversationId != controller->m_meta.conversationId) {
                return;
            }

            emit controller->groupMembersPageLoaded(meta, page);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void ChatSessionController::cancelPanelLoads()
{
    ++m_panelLoadToken;
    ++m_memberPageLoadToken;
}

void ChatSessionController::saveGroupName(const QString& name)
{
    saveGroupField(name, [](Group& group, const QString& value) {
        group.groupName = value;
    });
}

void ChatSessionController::saveGroupIntroduction(const QString& introduction)
{
    saveGroupField(introduction, [](Group& group, const QString& value) {
        group.introduction = value;
    });
}

void ChatSessionController::saveGroupAnnouncement(const QString& announcement)
{
    saveGroupField(announcement, [](Group& group, const QString& value) {
        group.announcement = value;
    });
}

void ChatSessionController::saveCurrentUserGroupNickname(const QString& nickname)
{
    saveGroupMemberNickname(CurrentUser::instance().getUserId(), nickname);
}

void ChatSessionController::saveGroupMemberNickname(const QString& userId, const QString& nickname)
{
    if (m_meta.conversationId.isEmpty() || !m_meta.isGroup || userId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({m_meta.conversationId});
    if (group.groupId.isEmpty() || !canEditMemberNickname(group, userId)) {
        return;
    }

    const QString nextNickname = nickname.trimmed();
    const QString currentUserId = CurrentUser::instance().getUserId();
    const QString previousNickname = memberNickname(group, userId);
    if (previousNickname == nextNickname &&
        (userId != currentUserId || group.currentUserNickname == nextNickname)) {
        return;
    }

    if (nextNickname.isEmpty()) {
        group.memberNicknames.remove(userId);
    } else {
        group.memberNicknames.insert(userId, nextNickname);
    }
    if (userId == currentUserId) {
        group.currentUserNickname = nextNickname;
    }

    GroupRepository::instance().saveGroup(group);
}

void ChatSessionController::promoteGroupMemberToAdmin(const QString& userId)
{
    if (m_meta.conversationId.isEmpty() || !m_meta.isGroup || userId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({m_meta.conversationId});
    if (group.groupId.isEmpty() || !canPromoteMemberToAdmin(group, userId)) {
        return;
    }

    if (!group.membersID.contains(userId)) {
        return;
    }

    group.adminsID.push_back(userId);
    GroupRepository::instance().saveGroup(group);
}

void ChatSessionController::cancelGroupMemberAdmin(const QString& userId)
{
    if (m_meta.conversationId.isEmpty() || !m_meta.isGroup || userId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({m_meta.conversationId});
    if (group.groupId.isEmpty() || !canCancelMemberAdmin(group, userId)) {
        return;
    }

    if (group.adminsID.removeAll(userId) <= 0) {
        return;
    }

    GroupRepository::instance().saveGroup(group);
}

void ChatSessionController::removeGroupMember(const QString& userId)
{
    if (m_meta.conversationId.isEmpty() || !m_meta.isGroup || userId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({m_meta.conversationId});
    if (group.groupId.isEmpty() || !canRemoveMember(group, userId)) {
        return;
    }

    const int removedMembers = group.membersID.removeAll(userId);
    const int removedAdmins = group.adminsID.removeAll(userId);
    const bool removedNickname = group.memberNicknames.remove(userId) > 0;
    if (removedMembers <= 0 && removedAdmins <= 0 && !removedNickname) {
        return;
    }

    group.memberNum = group.membersID.size();
    GroupRepository::instance().saveGroup(group);
}

void ChatSessionController::saveGroupRemark(const QString& remark)
{
    saveGroupField(remark, [](Group& group, const QString& value) {
        group.remark = value;
    });
}

void ChatSessionController::saveDirectRemark(const QString& remark)
{
    if (m_meta.conversationId.isEmpty() || m_meta.isGroup) {
        return;
    }

    User user = UserRepository::instance().requestUserDetail({m_meta.conversationId});
    const QString nextRemark = remark.trimmed();
    if (user.id.isEmpty() || user.remark == nextRemark) {
        return;
    }

    user.remark = nextRemark;
    UserRepository::instance().saveUser(user);
}

void ChatSessionController::setPinned(bool pinned)
{
    if (m_meta.conversationId.isEmpty() || m_meta.isPinned == pinned) {
        return;
    }

    m_meta.isPinned = pinned;
    MessageRepository::instance().setConversationPinned(m_meta.conversationId, pinned);
}

void ChatSessionController::setDoNotDisturb(bool enabled)
{
    if (m_meta.conversationId.isEmpty() || m_meta.isDoNotDisturb == enabled) {
        return;
    }

    m_meta.isDoNotDisturb = enabled;
    MessageRepository::instance().setConversationDoNotDisturb(m_meta.conversationId, enabled);
}

void ChatSessionController::clearMessages()
{
    if (m_meta.conversationId.isEmpty()) {
        return;
    }

    MessageRepository::instance().clearConversationMessages(m_meta.conversationId);
    emit messagesCleared();
}

void ChatSessionController::deleteFriend()
{
    if (m_meta.conversationId.isEmpty() || m_meta.isGroup) {
        return;
    }

    const QString friendId = m_meta.conversationId;
    MessageRepository::instance().removeConversation(friendId);
    UserRepository::instance().removeUser(friendId);
    close();
    emit conversationRemoved();
}

void ChatSessionController::exitGroup()
{
    if (m_meta.conversationId.isEmpty() || !m_meta.isGroup || !canExitGroup(m_group)) {
        return;
    }

    const QString groupId = m_meta.conversationId;
    MessageRepository::instance().removeConversation(groupId);
    GroupRepository::instance().removeGroup(groupId);
    close();
    emit conversationRemoved();
}

void ChatSessionController::refreshSessionData(bool emitChange)
{
    if (m_meta.conversationId.isEmpty()) {
        m_directUser = {};
        m_group = {};
    } else {
        m_meta = MessageRepository::instance().requestConversationMeta({m_meta.conversationId});
        m_group = {};
        m_directUser = {};
    }

    if (emitChange) {
        emit sessionChanged(m_meta, m_directUser, m_group);
    }
}

bool ChatSessionController::hasCurrentConversation(const QString& changedConversationId) const
{
    return !changedConversationId.isEmpty() && changedConversationId == m_meta.conversationId;
}

bool ChatSessionController::canEditGroupInfo(const Group& group) const
{
    return !group.groupId.isEmpty() &&
           (GroupRepository::instance().isCurrentUserGroupOwner(group) ||
            GroupRepository::instance().isCurrentUserGroupAdmin(group));
}

bool ChatSessionController::canExitGroup(const Group& group) const
{
    return !group.groupId.isEmpty() &&
           !GroupRepository::instance().isCurrentUserGroupOwner(group);
}

bool ChatSessionController::canEditMemberNickname(const Group& group, const QString& userId) const
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return false;
    }

    const QString currentUserId = CurrentUser::instance().getUserId();
    if (userId == currentUserId) {
        return true;
    }

    const GroupRole currentRole = memberRole(group, currentUserId);
    const GroupRole targetRole = memberRole(group, userId);
    if (currentRole == GroupRole::Owner) {
        return true;
    }
    return currentRole == GroupRole::Admin && targetRole == GroupRole::Member;
}

bool ChatSessionController::canPromoteMemberToAdmin(const Group& group, const QString& userId) const
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return false;
    }

    const QString currentUserId = CurrentUser::instance().getUserId();
    if (userId == currentUserId) {
        return false;
    }

    const GroupRole currentRole = memberRole(group, currentUserId);
    const GroupRole targetRole = memberRole(group, userId);
    return currentRole == GroupRole::Owner && targetRole == GroupRole::Member;
}

bool ChatSessionController::canCancelMemberAdmin(const Group& group, const QString& userId) const
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return false;
    }

    const QString currentUserId = CurrentUser::instance().getUserId();
    if (userId == currentUserId) {
        return false;
    }

    const GroupRole currentRole = memberRole(group, currentUserId);
    const GroupRole targetRole = memberRole(group, userId);
    return currentRole == GroupRole::Owner && targetRole == GroupRole::Admin;
}

bool ChatSessionController::canRemoveMember(const Group& group, const QString& userId) const
{
    if (group.groupId.isEmpty() || userId.isEmpty()) {
        return false;
    }

    const QString currentUserId = CurrentUser::instance().getUserId();
    if (userId == currentUserId) {
        return false;
    }

    const GroupRole currentRole = memberRole(group, currentUserId);
    const GroupRole targetRole = memberRole(group, userId);
    if (currentRole == GroupRole::Owner) {
        return true;
    }
    return currentRole == GroupRole::Admin && targetRole == GroupRole::Member;
}

void ChatSessionController::saveGroupField(const QString& value, void (*assign)(Group&, const QString&))
{
    if (m_meta.conversationId.isEmpty() || !m_meta.isGroup) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({m_meta.conversationId});
    if (group.groupId.isEmpty()) {
        return;
    }

    const QString nextValue = value.trimmed();
    const Group previousGroup = group;
    assign(group, nextValue);
    if (group.groupName != previousGroup.groupName && !canEditGroupInfo(previousGroup)) {
        return;
    }
    if (group.introduction != previousGroup.introduction && !canEditGroupInfo(previousGroup)) {
        return;
    }
    if (group.announcement != previousGroup.announcement && !canEditGroupInfo(previousGroup)) {
        return;
    }
    if (group.groupName == previousGroup.groupName &&
        group.introduction == previousGroup.introduction &&
        group.announcement == previousGroup.announcement &&
        group.currentUserNickname == previousGroup.currentUserNickname &&
        group.remark == previousGroup.remark) {
        return;
    }

    GroupRepository::instance().saveGroup(group);
}
