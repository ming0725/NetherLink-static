#pragma once

#include <QDateTime>
#include <QSize>
#include <QSharedPointer>
#include <QString>
#include <QVector>

#include "ChatMessage.h"
#include "User.h"

struct EmptyRequest {
};

struct FriendListRequest {
    QString keyword;
};

struct UserDetailRequest {
    QString userId;
};

struct GroupDetailRequest {
    QString groupId;
};

struct ConversationListRequest {
};

struct ConversationMessagesRequest {
    QString conversationId;
};

struct ConversationMetaRequest {
    QString conversationId;
};

struct PostFeedRequest {
    int offset = 0;
    int limit = 20;
    bool followOnly = false;
};

struct PostDetailRequest {
    QString postId;
};

struct FriendSummary {
    QString userId;
    QString displayName;
    QString avatarPath;
    UserStatus status = Offline;
    QString signature;
    bool isDoNotDisturb = false;
};

struct ConversationSummary {
    QString conversationId;
    QString title;
    QString avatarPath;
    QString previewText;
    QDateTime lastMessageTime;
    int unreadCount = 0;
    bool isDoNotDisturb = false;
    bool isGroup = false;
    int memberCount = 0;
};

struct ConversationMeta {
    QString conversationId;
    QString title;
    QString avatarPath;
    bool isGroup = false;
    int memberCount = 0;
    UserStatus status = Offline;
    bool isDoNotDisturb = false;
};

struct PostSummary {
    QString postId;
    QString title;
    QString thumbnailImagePath;
    QSize thumbnailImageSize;
    QString authorId;
    QString authorName;
    QString authorAvatarPath;
    int likeCount = 0;
    int commentCount = 0;
    bool isLiked = false;
};

struct PostDetailData {
    QString postId;
    QString title;
    QString content;
    QString authorId;
    QString authorName;
    QString authorAvatarPath;
    QVector<QString> imagePaths;
    int likeCount = 0;
    int commentCount = 0;
    bool isLiked = false;
};

using ChatMessageList = QVector<QSharedPointer<ChatMessage>>;
