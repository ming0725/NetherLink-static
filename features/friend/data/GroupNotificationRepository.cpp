#include "GroupNotificationRepository.h"

#include <algorithm>
#include <QSet>

#include "app/state/CurrentUser.h"
#include "features/chat/data/GroupRepository.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/data/UnreadStateRepository.h"

namespace {

const QString kGroupNotificationUnreadScope = QStringLiteral("group_notifications");
constexpr int kInitialUnreadCount = 28;

class GroupNotificationListOperation final
    : public RepositoryTemplate<GroupNotificationListRequest, QVector<GroupNotification>>
{
public:
    GroupNotificationListOperation(QVector<GroupNotification> notifications,
                                   GroupNotificationListRequest query)
        : m_notifications(std::move(notifications))
        , m_query(std::move(query))
    {
    }

private:
    QVector<GroupNotification> doRequest(const GroupNotificationListRequest&) const override
    {
        const int offset = qBound(0, m_query.offset, m_notifications.size());
        const int limit = m_query.limit < 0
                ? m_notifications.size() - offset
                : qMax(0, m_query.limit);
        return m_notifications.mid(offset, limit);
    }

    QVector<GroupNotification> m_notifications;
    GroupNotificationListRequest m_query;
};

bool canManageGroup(const Group& group)
{
    return GroupRepository::instance().isCurrentUserGroupOwner(group) ||
           GroupRepository::instance().isCurrentUserGroupAdmin(group);
}

bool isCurrentUserOwner(const Group& group)
{
    return GroupRepository::instance().isCurrentUserGroupOwner(group);
}

GroupMemberRole roleForUser(const Group& group, const QString& userId)
{
    if (group.ownerId == userId) {
        return GroupMemberRole::Owner;
    }
    if (group.adminsID.contains(userId)) {
        return GroupMemberRole::Admin;
    }
    return GroupMemberRole::Member;
}

GroupNotification makeNotification(const QString& id,
                                   GroupNotificationType type,
                                   const QString& groupId,
                                   const QString& actorUserId,
                                   int daysAgo,
                                   const QString& message = {},
                                   GroupNotificationStatus status = GroupNotificationStatus::None,
                                   const QString& operatorUserId = {})
{
    const Group group = GroupRepository::instance().requestGroupDetail({groupId});
    GroupNotification notification;
    notification.id = id;
    notification.type = type;
    notification.groupId = groupId;
    notification.actorUserId = actorUserId;
    notification.operatorUserId = operatorUserId;
    notification.message = message;
    notification.status = status;
    notification.createdAt = QDateTime::currentDateTime().addDays(-daysAgo);
    notification.actorRole = roleForUser(group, actorUserId);
    return notification;
}

QVector<GroupNotification> buildInitialNotifications()
{
    QVector<Group> groups = GroupRepository::instance().requestGroupList();
    QSet<QString> seen;
    QVector<Group> manageableGroups;
    for (const Group& group : std::as_const(groups)) {
        if (seen.contains(group.groupId) || !canManageGroup(group)) {
            continue;
        }
        seen.insert(group.groupId);
        manageableGroups.push_back(group);
    }

    const QVector<QString> requestUsers = {
        QStringLiteral("u101"), QStringLiteral("u102"), QStringLiteral("u103"),
        QStringLiteral("u104"), QStringLiteral("u105"), QStringLiteral("u106"),
        QStringLiteral("u107"), QStringLiteral("u108"), QStringLiteral("u109"),
        QStringLiteral("u110"), QStringLiteral("u111"), QStringLiteral("u112"),
        QStringLiteral("u113"), QStringLiteral("u114"), QStringLiteral("u115"),
        QStringLiteral("u116"), QStringLiteral("u117"), QStringLiteral("u118")
    };
    const QVector<QString> messages = {
        QStringLiteral("我想加入群聊，方便一起交流。"),
        QStringLiteral("朋友推荐我来这个群，想申请加入。"),
        QStringLiteral("之前关注过群里的内容，希望通过一下。"),
        QStringLiteral("想加入后续活动讨论。")
    };

    QVector<GroupNotification> notifications;
    notifications.reserve(34);
    int serial = 1;
    for (int i = 0; i < requestUsers.size() && !manageableGroups.isEmpty(); ++i) {
        const Group& group = manageableGroups.at(i % manageableGroups.size());
        notifications.push_back(makeNotification(
            QStringLiteral("gn_%1").arg(serial++, 3, 10, QChar('0')),
            GroupNotificationType::JoinRequest,
            group.groupId,
            requestUsers.at(i),
            i % 12,
            messages.at(i % messages.size()),
            GroupNotificationStatus::Pending));
    }

    auto addAcceptedJoinRequest = [&](const QString& groupId,
                                      const QString& actorUserId,
                                      const QString& operatorUserId,
                                      int daysAgo,
                                      const QString& message) {
        const auto it = std::find_if(manageableGroups.cbegin(),
                                     manageableGroups.cend(),
                                     [&](const Group& group) {
                                         return group.groupId == groupId;
                                     });
        if (it == manageableGroups.cend()) {
            return;
        }

        notifications.push_back(makeNotification(
            QStringLiteral("gn_%1").arg(serial++, 3, 10, QChar('0')),
            GroupNotificationType::JoinRequest,
            groupId,
            actorUserId,
            daysAgo,
            message,
            GroupNotificationStatus::Accepted,
            operatorUserId));
    };

    addAcceptedJoinRequest(QStringLiteral("g001"),
                           QStringLiteral("u119"),
                           QStringLiteral("u001"),
                           0,
                           QStringLiteral("我是亲戚介绍来的，麻烦通过一下。"));
    addAcceptedJoinRequest(QStringLiteral("g010"),
                           QStringLiteral("u120"),
                           QStringLiteral("u002"),
                           1,
                           QStringLiteral("参与服务器维护值班，申请加入。"));
    addAcceptedJoinRequest(QStringLiteral("g004"),
                           QStringLiteral("u121"),
                           QStringLiteral("u002"),
                           1,
                           QStringLiteral("想加入 Qt 学习讨论。"));
    addAcceptedJoinRequest(QStringLiteral("g005"),
                           QStringLiteral("u122"),
                           QStringLiteral("u007"),
                           2,
                           QStringLiteral("希望加入技术交流。"));
    addAcceptedJoinRequest(QStringLiteral("g012"),
                           QStringLiteral("u123"),
                           QStringLiteral("u005"),
                           2,
                           QStringLiteral("想一起交流像素美术。"));
    addAcceptedJoinRequest(QStringLiteral("g014"),
                           QStringLiteral("u124"),
                           QStringLiteral("u007"),
                           3,
                           QStringLiteral("朋友邀请我周末一起开黑。"));

    for (int i = 0; i < manageableGroups.size(); ++i) {
        const Group& group = manageableGroups.at(i);
        QString actor = group.membersID.isEmpty() ? QStringLiteral("u101") : group.membersID.last();
        if (isCurrentUserOwner(group) && !group.adminsID.isEmpty() && i % 2 == 0) {
            actor = group.adminsID.first();
        } else if (!isCurrentUserOwner(group) && group.adminsID.contains(actor)) {
            actor = group.membersID.value(qMax(0, group.membersID.size() - 2), QStringLiteral("u102"));
        }
        notifications.push_back(makeNotification(
            QStringLiteral("gn_%1").arg(serial++, 3, 10, QChar('0')),
            GroupNotificationType::MemberExited,
            group.groupId,
            actor,
            (i + 3) % 15));
    }

    for (int i = 0; i < manageableGroups.size(); ++i) {
        const Group& group = manageableGroups.at(i);
        notifications.push_back(makeNotification(
            QStringLiteral("gn_%1").arg(serial++, 3, 10, QChar('0')),
            i % 2 == 0 ? GroupNotificationType::AdminAssigned : GroupNotificationType::OwnerTransferred,
            group.groupId,
            CurrentUser::instance().getUserId(),
            (i + 5) % 16,
            {},
            GroupNotificationStatus::None,
            group.ownerId));
    }

    return notifications;
}

} // namespace

GroupNotificationRepository& GroupNotificationRepository::instance()
{
    static GroupNotificationRepository repo;
    return repo;
}

GroupNotificationRepository::GroupNotificationRepository(QObject* parent)
    : QObject(parent)
{
}

void GroupNotificationRepository::ensureLoaded() const
{
    if (m_loaded) {
        return;
    }

    auto* self = const_cast<GroupNotificationRepository*>(this);
    self->m_notifications = buildInitialNotifications();
    for (const GroupNotification& notification : self->m_notifications) {
        if (notification.unread) {
            UnreadStateRepository::instance().setUnread(kGroupNotificationUnreadScope,
                                                        notification.id,
                                                        true);
        }
    }
    self->m_loaded = true;
}

QVector<GroupNotification> GroupNotificationRepository::requestNotificationList(
    const GroupNotificationListRequest& query) const
{
    ensureLoaded();
    QVector<GroupNotification> notifications = m_notifications;
    for (GroupNotification& notification : notifications) {
        notification.unread = UnreadStateRepository::instance().isUnread(kGroupNotificationUnreadScope,
                                                                         notification.id);
    }
    std::sort(notifications.begin(), notifications.end(),
              [](const GroupNotification& lhs, const GroupNotification& rhs) {
                  return lhs.createdAt > rhs.createdAt;
              });
    return GroupNotificationListOperation(std::move(notifications), query).request(query);
}

int GroupNotificationRepository::unreadCount() const
{
    return m_loaded
            ? UnreadStateRepository::instance().unreadCount(kGroupNotificationUnreadScope)
            : kInitialUnreadCount;
}

int GroupNotificationRepository::notificationCount() const
{
    ensureLoaded();
    return m_notifications.size();
}

void GroupNotificationRepository::markAllRead()
{
    ensureLoaded();
    UnreadStateRepository::instance().markAllRead(kGroupNotificationUnreadScope);
    emit notificationListChanged();
}

bool GroupNotificationRepository::acceptJoinRequest(const QString& notificationId)
{
    ensureLoaded();
    for (GroupNotification& notification : m_notifications) {
        if (notification.id != notificationId ||
            notification.type != GroupNotificationType::JoinRequest ||
            notification.status != GroupNotificationStatus::Pending) {
            continue;
        }

        notification.status = GroupNotificationStatus::Accepted;
        notification.operatorUserId = CurrentUser::instance().getUserId();
        GroupRepository::instance().addMember(notification.groupId, notification.actorUserId);
        emit notificationListChanged();
        return true;
    }
    return false;
}

bool GroupNotificationRepository::rejectJoinRequest(const QString& notificationId)
{
    ensureLoaded();
    for (GroupNotification& notification : m_notifications) {
        if (notification.id != notificationId ||
            notification.type != GroupNotificationType::JoinRequest ||
            notification.status != GroupNotificationStatus::Pending) {
            continue;
        }

        notification.status = GroupNotificationStatus::Rejected;
        notification.operatorUserId = CurrentUser::instance().getUserId();
        emit notificationListChanged();
        return true;
    }
    return false;
}
