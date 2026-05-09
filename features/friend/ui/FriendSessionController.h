#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QVector>

#include "shared/types/FriendNotification.h"
#include "shared/types/Group.h"
#include "shared/types/GroupNotification.h"
#include "shared/types/RepositoryTypes.h"
#include "shared/types/User.h"

class FriendSessionController : public QObject
{
    Q_OBJECT

public:
    explicit FriendSessionController(QObject* parent = nullptr);

    QVector<FriendGroupSummary> loadFriendGroupSummaries(const QString& keyword) const;
    QVector<FriendSummary> loadFriendsInGroup(const QString& groupId,
                                              const QString& keyword,
                                              int offset,
                                              int limit) const;
    User loadFriend(const QString& userId) const;
    QString userNickname(const QString& userId) const;
    QString userAvatarPath(const QString& userId) const;
    QMap<QString, QString> loadFriendGroups() const;
    bool saveFriend(const User& user);
    bool changeFriendGroup(const QString& userId, const QString& groupId, const QString& groupName);
    bool deleteFriend(const QString& userId);

    QVector<GroupCategorySummary> loadGroupCategorySummaries(const QString& keyword) const;
    QVector<Group> loadGroupsInCategory(const QString& categoryId,
                                        const QString& keyword,
                                        int offset,
                                        int limit) const;
    Group loadGroup(const QString& groupId) const;
    QString groupDisplayName(const QString& groupId) const;
    QString groupNicknameFor(const QString& groupId, const QString& userId) const;
    QString groupManagerRoleText(const QString& groupId, const QString& userId) const;
    QString groupAvatarPath(const QString& groupId) const;
    QMap<QString, QString> loadGroupCategories() const;
    bool canExitGroup(const Group& group) const;
    bool saveGroup(const Group& group);
    bool changeGroupCategory(const QString& groupId,
                             const QString& categoryId,
                             const QString& categoryName);
    bool exitGroup(const QString& groupId);

    QVector<FriendNotification> loadFriendNotifications(int offset, int limit) const;
    int friendNotificationCount() const;
    int friendUnreadCount() const;
    void markFriendNotificationsRead();
    bool acceptFriendRequest(const QString& notificationId);
    bool rejectFriendRequest(const QString& notificationId);

    QVector<GroupNotification> loadGroupNotifications(int offset, int limit) const;
    int groupNotificationCount() const;
    int groupUnreadCount() const;
    void markGroupNotificationsRead();
    bool acceptGroupJoinRequest(const QString& notificationId);
    bool rejectGroupJoinRequest(const QString& notificationId);

signals:
    void friendListChanged();
    void groupListChanged();
    void friendNotificationListChanged();
    void groupNotificationListChanged();
};
