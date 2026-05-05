#pragma once

#include <QDateTime>
#include <QString>

enum class NotificationSourceType {
    IdSearch,
    GroupChat,
    FriendShare
};

enum class NotificationStatus {
    Pending,
    Accepted,
    Rejected
};

struct FriendNotification {
    QString id;
    QString fromUserId;
    QString message;
    QDateTime requestDate;
    NotificationSourceType sourceType = NotificationSourceType::IdSearch;
    bool unread = true;

    // For GroupChat source
    QString sourceGroupId;
    QString sourceGroupName;
    QString sourceGroupMemberName;

    // For FriendShare source
    QString sourceFriendId;
    QString sourceFriendName;

    NotificationStatus status = NotificationStatus::Pending;

    QString sourceText() const {
        switch (sourceType) {
        case NotificationSourceType::IdSearch:
            return QStringLiteral("ID搜索");
        case NotificationSourceType::GroupChat:
            if (!sourceGroupName.isEmpty()) {
                if (!sourceGroupMemberName.isEmpty()) {
                    return QStringLiteral("来自%1群聊中的%2").arg(sourceGroupName, sourceGroupMemberName);
                }
                return QStringLiteral("来自%1群聊").arg(sourceGroupName);
            }
            return QStringLiteral("来自群聊");
        case NotificationSourceType::FriendShare:
            if (!sourceFriendName.isEmpty()) {
                return QStringLiteral("%1好友分享").arg(sourceFriendName);
            }
            return QStringLiteral("好友分享");
        }
        return {};
    }
};
