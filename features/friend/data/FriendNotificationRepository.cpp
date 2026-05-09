#include "FriendNotificationRepository.h"

#include <algorithm>

#include "app/state/CurrentUser.h"
#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/UserRepository.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/data/UnreadStateRepository.h"
#include "shared/types/ChatMessage.h"

namespace {

const QString kFriendRequestUnreadScope = QStringLiteral("friend_requests");
constexpr int kInitialUnreadCount = 36;

class NotificationListOperation final
    : public RepositoryTemplate<FriendNotificationListRequest, QVector<FriendNotification>>
{
public:
    NotificationListOperation(QVector<FriendNotification> notifications,
                              FriendNotificationListRequest query)
        : m_notifications(std::move(notifications))
        , m_query(std::move(query))
    {
    }

private:
    QVector<FriendNotification> doRequest(const FriendNotificationListRequest&) const override
    {
        const int offset = qBound(0, m_query.offset, m_notifications.size());
        const int limit = m_query.limit < 0
                ? m_notifications.size() - offset
                : qMax(0, m_query.limit);
        return m_notifications.mid(offset, limit);
    }

    QVector<FriendNotification> m_notifications;
    FriendNotificationListRequest m_query;
};

QString userDisplayName(const QString& userId)
{
    const User user = UserRepository::instance().requestUserDetail({userId});
    return user.nick.isEmpty() ? userId : user.nick;
}

QString groupDisplayName(const Group& group)
{
    return group.remark.isEmpty() ? group.groupName : group.remark;
}

FriendNotification makeNotification(const QString& id,
                                    const QString& fromUserId,
                                    const QString& message,
                                    int daysAgo,
                                    NotificationSourceType sourceType,
                                    const QString& groupId = {},
                                    const QString& sourceFriendName = {})
{
    FriendNotification notification;
    notification.id = id;
    notification.fromUserId = fromUserId;
    notification.message = message;
    notification.requestDate = QDateTime::currentDateTime().addDays(-daysAgo);
    notification.sourceType = sourceType;

    if (sourceType == NotificationSourceType::GroupChat) {
        const Group group = GroupRepository::instance().requestGroupDetail({groupId});
        notification.sourceGroupId = group.groupId;
        notification.sourceGroupName = groupDisplayName(group);
        notification.sourceGroupMemberName = group.memberNicknames.value(fromUserId, userDisplayName(fromUserId));
    } else if (sourceType == NotificationSourceType::FriendShare) {
        notification.sourceFriendName = sourceFriendName;
    }

    return notification;
}

QVector<FriendNotification> buildInitialNotifications()
{
    const QVector<QString> userIds = {
        QStringLiteral("u101"), QStringLiteral("u102"), QStringLiteral("u103"),
        QStringLiteral("u104"), QStringLiteral("u105"), QStringLiteral("u106"),
        QStringLiteral("u107"), QStringLiteral("u108"), QStringLiteral("u109"),
        QStringLiteral("u110"), QStringLiteral("u111"), QStringLiteral("u112"),
        QStringLiteral("u113"), QStringLiteral("u114"), QStringLiteral("u115"),
        QStringLiteral("u116"), QStringLiteral("u117"), QStringLiteral("u118"),
        QStringLiteral("u119"), QStringLiteral("u120"), QStringLiteral("u121"),
        QStringLiteral("u122"), QStringLiteral("u123"), QStringLiteral("u124"),
        QStringLiteral("u125"), QStringLiteral("u126"), QStringLiteral("u127"),
        QStringLiteral("u128"), QStringLiteral("u129"), QStringLiteral("u130"),
        QStringLiteral("u131"), QStringLiteral("u132"), QStringLiteral("u133"),
        QStringLiteral("u134"), QStringLiteral("u135"), QStringLiteral("u136"),
    };
    const QVector<QString> groupIds = {
        QStringLiteral("g004"), QStringLiteral("g005"), QStringLiteral("g011"),
        QStringLiteral("g012"), QStringLiteral("g013"), QStringLiteral("g014")
    };
    const QVector<QString> shareNames = {
        QStringLiteral("桃木匠"), QStringLiteral("中继器同学"), QStringLiteral("指令哥"),
        QStringLiteral("创意服群友"), QStringLiteral("红石群友"), QStringLiteral("共建队友")
    };
    const QVector<QString> messages = {
        QStringLiteral("看到你在创意服分享的建筑设计稿，想加好友继续交流。"),
        QStringLiteral("你做的红石机关很有启发，方便加好友吗？"),
        QStringLiteral("朋友把你的作品名片发给我了，想认识一下。"),
        QStringLiteral("通过主城坐标搜索找到你，想参观你的工坊。"),
        QStringLiteral("后续想请教地形笔刷和配色，方便通过一下。"),
        QStringLiteral("我们在同一个 Minecraft 群里聊过建造，想继续交流。")
    };

    QVector<FriendNotification> notifications;
    notifications.reserve(userIds.size());
    for (int i = 0; i < userIds.size(); ++i) {
        const NotificationSourceType sourceType =
                i % 3 == 0 ? NotificationSourceType::GroupChat
              : i % 3 == 1 ? NotificationSourceType::FriendShare
                            : NotificationSourceType::IdSearch;
        notifications.push_back(makeNotification(QStringLiteral("fn_%1").arg(i + 1, 3, 10, QChar('0')),
                                                 userIds.at(i),
                                                 messages.at(i % messages.size()),
                                                 i % 18,
                                                 sourceType,
                                                 groupIds.at(i % groupIds.size()),
                                                 shareNames.at(i % shareNames.size())));
    }
    return notifications;
}

} // namespace

FriendNotificationRepository& FriendNotificationRepository::instance()
{
    static FriendNotificationRepository repo;
    return repo;
}

FriendNotificationRepository::FriendNotificationRepository(QObject* parent)
    : QObject(parent)
{
}

void FriendNotificationRepository::ensureLoaded() const
{
    if (m_loaded) {
        return;
    }

    auto* self = const_cast<FriendNotificationRepository*>(this);
    self->m_notifications = buildInitialNotifications();
    for (const FriendNotification& notification : self->m_notifications) {
        if (notification.unread) {
            UnreadStateRepository::instance().setUnread(kFriendRequestUnreadScope,
                                                        notification.id,
                                                        true);
        }
    }
    self->m_loaded = true;
}

QVector<FriendNotification> FriendNotificationRepository::requestNotificationList(
    const FriendNotificationListRequest& query) const
{
    ensureLoaded();
    QVector<FriendNotification> notifications = m_notifications;
    for (FriendNotification& notification : notifications) {
        notification.unread = UnreadStateRepository::instance().isUnread(kFriendRequestUnreadScope,
                                                                         notification.id);
    }
    std::sort(notifications.begin(), notifications.end(),
              [](const FriendNotification& lhs, const FriendNotification& rhs) {
                  return lhs.requestDate > rhs.requestDate;
              });
    return NotificationListOperation(std::move(notifications), query).request(query);
}

int FriendNotificationRepository::unreadCount() const
{
    return m_loaded
            ? UnreadStateRepository::instance().unreadCount(kFriendRequestUnreadScope)
            : kInitialUnreadCount;
}

int FriendNotificationRepository::notificationCount() const
{
    ensureLoaded();
    return m_notifications.size();
}

void FriendNotificationRepository::markAllRead()
{
    ensureLoaded();
    UnreadStateRepository::instance().markAllRead(kFriendRequestUnreadScope);
    emit notificationListChanged();
}

bool FriendNotificationRepository::acceptRequest(const QString& notificationId)
{
    ensureLoaded();
    for (FriendNotification& notification : m_notifications) {
        if (notification.id != notificationId ||
            notification.status != NotificationStatus::Pending) {
            continue;
        }

        const QDateTime acceptedAt = QDateTime::currentDateTime();
        notification.status = NotificationStatus::Accepted;
        UserRepository::instance().addFriend(notification.fromUserId);

        auto message = QSharedPointer<ChatMessage>(new TextMessage(
            QStringLiteral("我们已经是好友了，现在可以开始聊天了"),
            true,
            CurrentUser::instance().getUserId(),
            false,
            CurrentUser::instance().getUserName()));
        message->setTimestamp(acceptedAt);
        MessageRepository::instance().addMessage(notification.fromUserId, message);
        MessageRepository::instance().markConversationRead(notification.fromUserId);

        emit notificationListChanged();
        return true;
    }
    return false;
}

bool FriendNotificationRepository::rejectRequest(const QString& notificationId)
{
    ensureLoaded();
    for (FriendNotification& notification : m_notifications) {
        if (notification.id != notificationId ||
            notification.status != NotificationStatus::Pending) {
            continue;
        }

        notification.status = NotificationStatus::Rejected;
        emit notificationListChanged();
        return true;
    }
    return false;
}
