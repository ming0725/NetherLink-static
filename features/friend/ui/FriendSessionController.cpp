#include "features/friend/ui/FriendSessionController.h"

#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/FriendNotificationRepository.h"
#include "features/friend/data/GroupNotificationRepository.h"
#include "features/friend/data/UserRepository.h"

FriendSessionController::FriendSessionController(QObject* parent)
    : QObject(parent)
{
    connect(&UserRepository::instance(), &UserRepository::friendListChanged,
            this, &FriendSessionController::friendListChanged);
    connect(&GroupRepository::instance(), &GroupRepository::groupListChanged,
            this, &FriendSessionController::groupListChanged);
    connect(&FriendNotificationRepository::instance(), &FriendNotificationRepository::notificationListChanged,
            this, &FriendSessionController::friendNotificationListChanged);
    connect(&GroupNotificationRepository::instance(), &GroupNotificationRepository::notificationListChanged,
            this, &FriendSessionController::groupNotificationListChanged);
}

QVector<FriendGroupSummary> FriendSessionController::loadFriendGroupSummaries(const QString& keyword) const
{
    return UserRepository::instance().requestFriendGroupSummaries({keyword});
}

QVector<FriendSummary> FriendSessionController::loadFriendsInGroup(const QString& groupId,
                                                                   const QString& keyword,
                                                                   int offset,
                                                                   int limit) const
{
    return UserRepository::instance().requestFriendsInGroup({groupId, keyword, offset, limit});
}

User FriendSessionController::loadFriend(const QString& userId) const
{
    if (userId.isEmpty()) {
        return {};
    }
    return UserRepository::instance().requestUserDetail({userId});
}

QString FriendSessionController::userNickname(const QString& userId) const
{
    const User user = loadFriend(userId);
    if (user.id.isEmpty()) {
        return userId;
    }
    return user.nick.isEmpty() ? user.id : user.nick;
}

QString FriendSessionController::userAvatarPath(const QString& userId) const
{
    return UserRepository::instance().requestUserAvatarPath(userId);
}

QMap<QString, QString> FriendSessionController::loadFriendGroups() const
{
    return UserRepository::instance().requestFriendGroups();
}

bool FriendSessionController::saveFriend(const User& user)
{
    if (user.id.isEmpty()) {
        return false;
    }
    UserRepository::instance().saveUser(user);
    return true;
}

bool FriendSessionController::changeFriendGroup(const QString& userId,
                                                const QString& groupId,
                                                const QString& groupName)
{
    if (userId.isEmpty() || groupId.isEmpty()) {
        return false;
    }

    User user = UserRepository::instance().requestUserDetail({userId});
    if (user.id.isEmpty() || user.friendGroupId == groupId) {
        return false;
    }

    user.friendGroupId = groupId;
    user.friendGroupName = groupName;
    UserRepository::instance().saveUser(user);
    return true;
}

bool FriendSessionController::deleteFriend(const QString& userId)
{
    if (userId.isEmpty()) {
        return false;
    }

    MessageRepository::instance().removeConversation(userId);
    UserRepository::instance().removeUser(userId);
    return true;
}

QVector<GroupCategorySummary> FriendSessionController::loadGroupCategorySummaries(const QString& keyword) const
{
    return GroupRepository::instance().requestGroupCategorySummaries({keyword});
}

QVector<Group> FriendSessionController::loadGroupsInCategory(const QString& categoryId,
                                                            const QString& keyword,
                                                            int offset,
                                                            int limit) const
{
    return GroupRepository::instance().requestGroupsInCategory({categoryId, keyword, offset, limit});
}

Group FriendSessionController::loadGroup(const QString& groupId) const
{
    if (groupId.isEmpty()) {
        return {};
    }
    return GroupRepository::instance().requestGroupDetail({groupId});
}

QString FriendSessionController::groupDisplayName(const QString& groupId) const
{
    const Group group = loadGroup(groupId);
    if (group.groupId.isEmpty()) {
        return groupId;
    }
    return group.remark.isEmpty() ? group.groupName : group.remark;
}

QString FriendSessionController::groupNicknameFor(const QString& groupId, const QString& userId) const
{
    const Group group = loadGroup(groupId);
    const QString groupNickname = group.memberNicknames.value(userId);
    return groupNickname.isEmpty() ? userNickname(userId) : groupNickname;
}

QString FriendSessionController::groupManagerRoleText(const QString& groupId, const QString& userId) const
{
    const Group group = loadGroup(groupId);
    return group.ownerId == userId ? QStringLiteral("群主") : QStringLiteral("管理员");
}

QString FriendSessionController::groupAvatarPath(const QString& groupId) const
{
    return GroupRepository::instance().requestGroupAvatarPath(groupId);
}

QMap<QString, QString> FriendSessionController::loadGroupCategories() const
{
    return GroupRepository::instance().requestGroupCategories();
}

bool FriendSessionController::canExitGroup(const Group& group) const
{
    return !group.groupId.isEmpty() && !GroupRepository::instance().isCurrentUserGroupOwner(group);
}

bool FriendSessionController::saveGroup(const Group& group)
{
    if (group.groupId.isEmpty()) {
        return false;
    }
    GroupRepository::instance().saveGroup(group);
    return true;
}

bool FriendSessionController::changeGroupCategory(const QString& groupId,
                                                  const QString& categoryId,
                                                  const QString& categoryName)
{
    if (groupId.isEmpty() || categoryId.isEmpty()) {
        return false;
    }

    Group group = GroupRepository::instance().requestGroupDetail({groupId});
    const QString currentCategoryId = group.listGroupId.isEmpty()
            ? QStringLiteral("gg_joined")
            : group.listGroupId;
    if (group.groupId.isEmpty() || categoryId == currentCategoryId) {
        return false;
    }

    group.listGroupId = categoryId;
    group.listGroupName = categoryName;
    GroupRepository::instance().saveGroup(group);
    return true;
}

bool FriendSessionController::exitGroup(const QString& groupId)
{
    if (groupId.isEmpty()) {
        return false;
    }

    const Group group = GroupRepository::instance().requestGroupDetail({groupId});
    if (!canExitGroup(group)) {
        return false;
    }

    MessageRepository::instance().removeConversation(groupId);
    GroupRepository::instance().removeGroup(groupId);
    return true;
}

QVector<FriendNotification> FriendSessionController::loadFriendNotifications(int offset, int limit) const
{
    return FriendNotificationRepository::instance().requestNotificationList({offset, limit});
}

int FriendSessionController::friendNotificationCount() const
{
    return FriendNotificationRepository::instance().notificationCount();
}

int FriendSessionController::friendUnreadCount() const
{
    return FriendNotificationRepository::instance().unreadCount();
}

void FriendSessionController::markFriendNotificationsRead()
{
    FriendNotificationRepository::instance().markAllRead();
}

bool FriendSessionController::acceptFriendRequest(const QString& notificationId)
{
    return FriendNotificationRepository::instance().acceptRequest(notificationId);
}

bool FriendSessionController::rejectFriendRequest(const QString& notificationId)
{
    return FriendNotificationRepository::instance().rejectRequest(notificationId);
}

QVector<GroupNotification> FriendSessionController::loadGroupNotifications(int offset, int limit) const
{
    return GroupNotificationRepository::instance().requestNotificationList({offset, limit});
}

int FriendSessionController::groupNotificationCount() const
{
    return GroupNotificationRepository::instance().notificationCount();
}

int FriendSessionController::groupUnreadCount() const
{
    return GroupNotificationRepository::instance().unreadCount();
}

void FriendSessionController::markGroupNotificationsRead()
{
    GroupNotificationRepository::instance().markAllRead();
}

bool FriendSessionController::acceptGroupJoinRequest(const QString& notificationId)
{
    return GroupNotificationRepository::instance().acceptJoinRequest(notificationId);
}

bool FriendSessionController::rejectGroupJoinRequest(const QString& notificationId)
{
    return GroupNotificationRepository::instance().rejectJoinRequest(notificationId);
}
