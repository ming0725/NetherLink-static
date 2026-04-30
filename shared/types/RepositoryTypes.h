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

struct FriendGroupListRequest {
    QString keyword;
};

struct FriendGroupItemsRequest {
    QString groupId;
    QString keyword;
    int offset = 0;
    int limit = 64;
};

struct GroupListRequest {
    QString keyword;
};

struct GroupCategoryListRequest {
    QString keyword;
};

struct GroupCategoryItemsRequest {
    QString categoryId;
    QString keyword;
    int offset = 0;
    int limit = 64;
};

struct UserDetailRequest {
    QString userId;
};

struct GroupDetailRequest {
    QString groupId;
};

struct ConversationListRequest {
    QString keyword;
};

struct ConversationMessagesRequest {
    QString conversationId;
    int offsetFromLatest = 0;
    int limit = 30;
};

struct ConversationMetaRequest {
    QString conversationId;
};

struct ConversationThreadRequest {
    QString conversationId;
    int offsetFromLatest = 0;
    int limit = 30;
};

struct AiChatListRequest {
    int offset = 0;
    int limit = 20;
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
    QString groupId = "default";
    QString groupName = "默认分组";
    QString nickName;
    QString remark;
};

struct FriendGroupSummary {
    QString groupId = "default";
    QString groupName = "默认分组";
    int friendCount = 0;
};

struct GroupCategorySummary {
    QString categoryId = "gg_joined";
    QString categoryName = "我加入的群聊";
    int groupCount = 0;
};

struct ConversationSummary {
    QString conversationId;
    QString title;
    QString avatarPath;
    QString previewText;
    QDateTime lastMessageTime;
    QDateTime messageListTime;
    int unreadCount = 0;
    bool isDoNotDisturb = false;
    bool isPinned = false;
    bool isGroup = false;
    int memberCount = 0;
};

struct ConversationSyncState {
    QString conversationId;
    int unreadCount = 0;
    bool isDoNotDisturb = false;
    bool isPinned = false;
    QDateTime messageListTime;
    QDateTime lastReadAt;
};

struct ConversationMeta {
    QString conversationId;
    QString title;
    QString avatarPath;
    bool isGroup = false;
    int memberCount = 0;
    UserStatus status = Offline;
    bool isDoNotDisturb = false;
    bool isPinned = false;
};

using ChatMessageList = QVector<QSharedPointer<ChatMessage>>;

struct ConversationThreadData {
    ConversationMeta meta;
    ChatMessageList messages;
    int loadedMessageCount = 0;
    bool hasMoreBefore = false;
};

struct GroupMembersPage {
    QString groupId;
    QString keyword;
    QVector<User> members;
    int offset = 0;
    int totalCount = 0;
    bool hasMore = false;
};

struct AiChatListEntry {
    QString conversationId;
    QString title;
    QDateTime time;
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
