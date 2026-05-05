#pragma once

#include <QDateTime>
#include <QString>

enum class GroupNotificationType {
    JoinRequest,
    MemberExited,
    AdminAssigned,
    OwnerTransferred
};

enum class GroupNotificationStatus {
    None,
    Pending,
    Accepted,
    Rejected
};

enum class GroupMemberRole {
    Owner,
    Admin,
    Member
};

struct GroupNotification {
    QString id;
    GroupNotificationType type = GroupNotificationType::JoinRequest;
    GroupNotificationStatus status = GroupNotificationStatus::None;
    QString groupId;
    QString actorUserId;
    QString operatorUserId;
    QString message;
    QDateTime createdAt;
    bool unread = true;
    GroupMemberRole actorRole = GroupMemberRole::Member;
};
