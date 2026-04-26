#include "MessageRepository.h"

#include <QSet>
#include <QTime>

#include <algorithm>
#include <random>

#include "features/chat/data/GroupRepository.h"
#include "shared/data/RepositoryTemplate.h"
#include "features/friend/data/UserRepository.h"
#include "app/state/CurrentUser.h"

namespace {

constexpr int kFriendConversationSampleCount = 8;
constexpr int kGroupConversationSampleCount = 6;

QString buildPreviewText(const QSharedPointer<ChatMessage>& message,
                         bool isGroup)
{
    if (!message) {
        return {};
    }

    if (isGroup) {
        const QString senderName = message->getSenderName().isEmpty()
                ? UserRepository::instance().requestUserName(message->getSenderId())
                : message->getSenderName();
        return QString("%1：%2").arg(senderName, message->getContent());
    }
    return message->getContent();
}

QString groupDisplayName(const Group& group)
{
    return group.remark.isEmpty() ? group.groupName : group.remark;
}

QString userDisplayName(const User& user)
{
    return user.remark.isEmpty() ? user.nick : user.remark;
}

int sampleUnreadCount(int ordinal, int messageCount)
{
    switch (ordinal % 5) {
    case 0:
        return 3;
    case 1:
        return 18;
    case 2:
        return 128;
    case 3:
        return 7;
    default:
        return messageCount > 99 ? 101 : 12;
    }
}

bool sampleDoNotDisturb(int ordinal)
{
    return ordinal % 5 == 1 || ordinal % 7 == 3;
}

bool matchesConversationKeyword(const Group& group, const QString& keyword)
{
    return keyword.isEmpty() ||
           group.groupName.contains(keyword, Qt::CaseInsensitive) ||
           group.remark.contains(keyword, Qt::CaseInsensitive);
}

bool matchesConversationKeyword(const FriendSummary& friendSummary, const QString& keyword)
{
    return keyword.isEmpty() ||
           friendSummary.nickName.contains(keyword, Qt::CaseInsensitive) ||
           friendSummary.remark.contains(keyword, Qt::CaseInsensitive);
}

QDateTime dateTimeAt(const QDate& date, int hour, int minute)
{
    return QDateTime(date, QTime(hour, minute));
}

void appendCluster(QVector<QDateTime>& timeline,
                   const QDateTime& start,
                   int count,
                   int stepMinutes)
{
    for (int index = 0; index < count; ++index) {
        timeline.push_back(start.addSecs(index * stepMinutes * 60));
    }
}

enum class SampleLatestBucket {
    Today,
    Yesterday,
    DaysAgo,
    OneMonthAgo,
    MonthsAgo,
    OneYearAgo,
    YearsAgo
};

SampleLatestBucket sampleLatestBucket(int ordinal)
{
    switch (ordinal % 7) {
    case 0:
        return SampleLatestBucket::Today;
    case 1:
        return SampleLatestBucket::Yesterday;
    case 2:
        return SampleLatestBucket::DaysAgo;
    case 3:
        return SampleLatestBucket::OneMonthAgo;
    case 4:
        return SampleLatestBucket::MonthsAgo;
    case 5:
        return SampleLatestBucket::OneYearAgo;
    default:
        return SampleLatestBucket::YearsAgo;
    }
}

QVector<QDateTime> buildHistoryTimeline(int ordinal)
{
    QVector<QDateTime> timeline;
    timeline.reserve(36);

    const QDateTime now = QDateTime::currentDateTime();
    const int hourOffset = (ordinal % 3) * 2;
    const int minuteOffset = (ordinal % 4) * 3;
    const SampleLatestBucket bucket = sampleLatestBucket(ordinal);

    appendCluster(timeline,
                  dateTimeAt(now.date().addYears(-2), 8 + hourOffset, 12 + minuteOffset),
                  4,
                  7);
    if (bucket == SampleLatestBucket::YearsAgo) {
        return timeline;
    }

    appendCluster(timeline,
                  dateTimeAt(now.date().addYears(-1), 9 + hourOffset, 6 + minuteOffset),
                  4,
                  6);
    if (bucket == SampleLatestBucket::OneYearAgo) {
        return timeline;
    }

    appendCluster(timeline,
                  dateTimeAt(now.date().addMonths(-4), 10 + hourOffset, 18 + minuteOffset),
                  4,
                  5);
    if (bucket == SampleLatestBucket::MonthsAgo) {
        return timeline;
    }

    appendCluster(timeline,
                  dateTimeAt(now.date().addMonths(-1), 11 + hourOffset, 9 + minuteOffset),
                  4,
                  6);
    if (bucket == SampleLatestBucket::OneMonthAgo) {
        return timeline;
    }

    appendCluster(timeline,
                  dateTimeAt(now.date().addDays(-10), 13 + hourOffset, 15 + minuteOffset),
                  4,
                  4);
    appendCluster(timeline,
                  dateTimeAt(now.date().addDays(-3), 15 + hourOffset, 8 + minuteOffset),
                  4,
                  5);
    if (bucket == SampleLatestBucket::DaysAgo) {
        return timeline;
    }

    appendCluster(timeline,
                  dateTimeAt(now.date().addDays(-2), 16 + hourOffset, 5 + minuteOffset),
                  4,
                  4);
    appendCluster(timeline,
                  dateTimeAt(now.date().addDays(-1), 18 + hourOffset, 10 + minuteOffset),
                  4,
                  4);
    if (bucket == SampleLatestBucket::Yesterday) {
        return timeline;
    }

    appendCluster(timeline,
                  now.addSecs((-54 + ordinal) * 60),
                  4,
                  8);

    return timeline;
}

QVector<FriendSummary> sampledFriendsForMessages()
{
    QVector<FriendSummary> friends = UserRepository::instance().requestFriendList();
    std::mt19937 generator(20240521);
    std::shuffle(friends.begin(), friends.end(), generator);
    friends.resize(qMin(kFriendConversationSampleCount, friends.size()));
    return friends;
}

class ConversationMessagesRequestOperation final
    : public RepositoryTemplate<ConversationMessagesRequest, ChatMessageList> {
public:
    explicit ConversationMessagesRequestOperation(QMap<QString, ChatMessageList> store)
        : m_store(std::move(store))
    {
    }

private:
    ChatMessageList doRequest(const ConversationMessagesRequest& query) const override
    {
        const ChatMessageList messages = m_store.value(query.conversationId);
        if (messages.isEmpty()) {
            return {};
        }

        const int total = messages.size();
        const int offset = qBound(0, query.offsetFromLatest, total);
        const int end = total - offset;
        if (end <= 0 || query.limit == 0) {
            return {};
        }

        const int start = query.limit < 0
                ? 0
                : qMax(0, end - qMax(0, query.limit));
        return messages.mid(start, end - start);
    }

    QMap<QString, ChatMessageList> m_store;
};

class ConversationMetaRequestOperation final
    : public RepositoryTemplate<ConversationMetaRequest, ConversationMeta> {
public:
    ConversationMeta doRequest(const ConversationMetaRequest& query) const override
    {
        if (GroupRepository::instance().contains(query.conversationId)) {
            const Group group = GroupRepository::instance().requestGroupDetail({query.conversationId});
            return ConversationMeta{
                    group.groupId,
                    groupDisplayName(group),
                    group.groupAvatarPath,
                    true,
                    group.memberNum,
                    Offline,
                    group.isDnd
            };
        }

        const User user = UserRepository::instance().requestUserDetail({query.conversationId});
        return ConversationMeta{
                user.id,
                userDisplayName(user),
                user.avatarPath,
                false,
                0,
                user.status,
                user.isDnd
        };
    }
};

class ConversationListRequestOperation final
    : public RepositoryTemplate<ConversationListRequest, QVector<ConversationSummary>> {
public:
    ConversationListRequestOperation(QMap<QString, ChatMessageList> store,
                                     QMap<QString, ConversationSyncState> conversationStates)
        : m_store(std::move(store))
        , m_conversationStates(std::move(conversationStates))
    {
    }

private:
    ConversationSyncState stateFor(const QString& conversationId) const
    {
        ConversationSyncState state = m_conversationStates.value(conversationId);
        state.conversationId = conversationId;
        return state;
    }

    QDateTime effectiveMessageListTime(const QString& conversationId,
                                       const QSharedPointer<ChatMessage>& lastMessage) const
    {
        const QDateTime messageTime = lastMessage ? lastMessage->getTimestamp() : QDateTime();
        const QDateTime recentTime = stateFor(conversationId).messageListTime;
        return recentTime > messageTime ? recentTime : messageTime;
    }

    QVector<ConversationSummary> doRequest(const ConversationListRequest& query) const override
    {
        QVector<ConversationSummary> result;
        QSet<QString> seenConversationIds;

        const QVector<Group> groups = GroupRepository::instance().requestGroupList();
        for (const Group& group : groups) {
            if (!matchesConversationKeyword(group, query.keyword)) {
                continue;
            }
            if (!m_store.contains(group.groupId) &&
                !m_conversationStates.contains(group.groupId)) {
                continue;
            }

            if (seenConversationIds.contains(group.groupId)) {
                continue;
            }

            seenConversationIds.insert(group.groupId);
            const ChatMessageList messages = m_store.value(group.groupId);
            const QSharedPointer<ChatMessage> lastMessage = messages.isEmpty() ? QSharedPointer<ChatMessage>() : messages.last();
            const ConversationSyncState state = stateFor(group.groupId);
            result.push_back(ConversationSummary{
                    group.groupId,
                    groupDisplayName(group),
                    group.groupAvatarPath,
                    buildPreviewText(lastMessage, true),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(group.groupId, lastMessage),
                    state.unreadCount,
                    group.isDnd || state.isDoNotDisturb,
                    true,
                    group.memberNum
            });
        }

        const QVector<FriendSummary> friends = UserRepository::instance().requestFriendList();
        for (const FriendSummary& friendSummary : friends) {
            if (!matchesConversationKeyword(friendSummary, query.keyword)) {
                continue;
            }
            if (!m_store.contains(friendSummary.userId) &&
                !m_conversationStates.contains(friendSummary.userId)) {
                continue;
            }

            if (seenConversationIds.contains(friendSummary.userId)) {
                continue;
            }

            seenConversationIds.insert(friendSummary.userId);
            const ChatMessageList messages = m_store.value(friendSummary.userId);
            const QSharedPointer<ChatMessage> lastMessage = messages.isEmpty() ? QSharedPointer<ChatMessage>() : messages.last();
            const ConversationSyncState state = stateFor(friendSummary.userId);
            result.push_back(ConversationSummary{
                    friendSummary.userId,
                    friendSummary.displayName,
                    friendSummary.avatarPath,
                    buildPreviewText(lastMessage, false),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(friendSummary.userId, lastMessage),
                    state.unreadCount,
                    friendSummary.isDoNotDisturb || state.isDoNotDisturb,
                    false,
                    0
            });
        }

        return result;
    }

    void onAfterRequest(const ConversationListRequest&, QVector<ConversationSummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const ConversationSummary& lhs, const ConversationSummary& rhs) {
            return lhs.messageListTime > rhs.messageListTime;
        });
    }

    QMap<QString, ChatMessageList> m_store;
    QMap<QString, ConversationSyncState> m_conversationStates;
};

} // namespace

MessageRepository::MessageRepository(QObject* parent)
        : QObject(parent)
{
    auto addGeneratedHistory = [this](const QString& conversationId,
                                      const QString& peerId,
                                      const QString& peerName,
                                      bool isGroup,
                                      int ordinal,
                                      GroupRole role = GroupRole::Member) {
        const QVector<QDateTime> timeline = buildHistoryTimeline(ordinal);
        for (int index = 0; index < timeline.size(); ++index) {
            const bool fromMe = index % 4 == 1;
            const QString senderId = fromMe ? CurrentUser::instance().getUserId() : peerId;
            const QString senderName = fromMe ? CurrentUser::instance().getUserName() : peerName;
            auto msg = QSharedPointer<ChatMessage>(new TextMessage(
                    QStringLiteral("%1 历史消息 %2").arg(peerName).arg(index + 1),
                    fromMe,
                    senderId,
                    isGroup,
                    senderName,
                    fromMe ? GroupRole::Admin : role));
            msg->setTimestamp(timeline.at(index));
            addMessage(conversationId, msg);
        }
        ConversationSyncState state;
        state.conversationId = conversationId;
        state.unreadCount = sampleUnreadCount(ordinal, timeline.size());
        state.isDoNotDisturb = sampleDoNotDisturb(ordinal);
        state.messageListTime = m_conversationStates.value(conversationId).messageListTime;
        m_conversationStates.insert(conversationId, state);
    };

    const QVector<Group> groups = GroupRepository::instance().requestGroupList();
    int conversationOrdinal = 0;
    for (int index = 0; index < qMin(kGroupConversationSampleCount, groups.size()); ++index) {
        const Group& group = groups.at(index);
        addGeneratedHistory(group.groupId,
                            group.ownerId,
                            UserRepository::instance().requestUserName(group.ownerId),
                            true,
                            conversationOrdinal++,
                            GroupRole::Owner);
    }

    const QVector<FriendSummary> friends = sampledFriendsForMessages();
    for (const FriendSummary& friendSummary : friends) {
        addGeneratedHistory(friendSummary.userId,
                            friendSummary.userId,
                            friendSummary.displayName,
                            false,
                            conversationOrdinal++);
    }
}

MessageRepository& MessageRepository::instance()
{
    static MessageRepository repo;
    return repo;
}

QVector<ConversationSummary> MessageRepository::requestConversationList(const ConversationListRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return ConversationListRequestOperation(m_store, m_conversationStates).request(query);
}

ChatMessageList MessageRepository::requestConversationMessages(const ConversationMessagesRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return ConversationMessagesRequestOperation(m_store).request(query);
}

ConversationMeta MessageRepository::requestConversationMeta(const ConversationMetaRequest& query) const
{
    return ConversationMetaRequestOperation().request(query);
}

ConversationThreadData MessageRepository::requestConversationThread(const ConversationThreadRequest& query) const
{
    ConversationThreadData thread;
    thread.meta = requestConversationMeta({query.conversationId});
    {
        QMutexLocker locker(&m_mutex);
        const ChatMessageList allMessages = m_store.value(query.conversationId);
        thread.messages = ConversationMessagesRequestOperation(m_store).request({
                query.conversationId,
                query.offsetFromLatest,
                query.limit
        });
        thread.loadedMessageCount = qMin(allMessages.size(),
                                         qMax(0, query.offsetFromLatest) + thread.messages.size());
        thread.hasMoreBefore = thread.loadedMessageCount < allMessages.size();
    }
    return thread;
}

void MessageRepository::touchConversation(const QString& conversationId,
                                          const QDateTime& timestamp)
{
    if (conversationId.isEmpty() || !timestamp.isValid()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.messageListTime = timestamp;
    }
    emit conversationListChanged(conversationId);
}

void MessageRepository::markConversationRead(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.unreadCount = 0;
        state.lastReadAt = QDateTime::currentDateTime();
    }
}

void MessageRepository::removeConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        changed = m_store.remove(conversationId) > 0;
        changed = m_conversationStates.remove(conversationId) > 0 || changed;
    }

    if (!changed) {
        return;
    }

    emit lastMessageChanged(conversationId, {});
    emit conversationListChanged(conversationId);
}

void MessageRepository::addMessage(const QString& conversationId,
                                   QSharedPointer<ChatMessage> message)
{
    {
        QMutexLocker locker(&m_mutex);
        m_store[conversationId].push_back(message);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.messageListTime = message ? message->getTimestamp() : QDateTime::currentDateTime();
    }
    emit lastMessageChanged(conversationId, message);
    emit conversationListChanged(conversationId);
}

void MessageRepository::removeMessage(const QString& conversationId, int index)
{
    QSharedPointer<ChatMessage> lastMsg;
    {
        QMutexLocker locker(&m_mutex);
        auto& vec = m_store[conversationId];
        if (index >= 0 && index < vec.size()) {
            vec.removeAt(index);
        }
        if (!vec.isEmpty()) {
            lastMsg = vec.last();
            ConversationSyncState& state = m_conversationStates[conversationId];
            state.conversationId = conversationId;
            state.messageListTime = lastMsg->getTimestamp();
        } else {
            m_store.remove(conversationId);
        }
    }
    emit lastMessageChanged(conversationId, lastMsg);
    emit conversationListChanged(conversationId);
}

bool MessageRepository::removeMessage(const QString& conversationId,
                                      const QSharedPointer<ChatMessage>& message)
{
    if (conversationId.isEmpty() || message.isNull()) {
        return false;
    }

    int removeIndex = -1;
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_store.constFind(conversationId);
        if (it == m_store.constEnd()) {
            return false;
        }

        const ChatMessageList& messages = it.value();
        for (int index = 0; index < messages.size(); ++index) {
            if (messages.at(index).data() == message.data()) {
                removeIndex = index;
                break;
            }
        }
    }

    if (removeIndex < 0) {
        return false;
    }

    removeMessage(conversationId, removeIndex);
    return true;
}
