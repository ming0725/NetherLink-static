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

QString userNameForIdentity(const QString& userId)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    return currentUser.isCurrentUserId(userId)
            ? currentUser.getUserName()
            : UserRepository::instance().requestUserName(userId);
}

GroupRole groupRoleForUser(const Group& group, const QString& userId)
{
    if (!group.ownerId.isEmpty() && group.ownerId == userId) {
        return GroupRole::Owner;
    }
    if (group.adminsID.contains(userId)) {
        return GroupRole::Admin;
    }
    return GroupRole::Member;
}

struct SampleParticipant {
    QString userId;
    QString displayName;
    GroupRole role = GroupRole::Member;
};

void appendGroupParticipant(QVector<SampleParticipant>& participants,
                            QSet<QString>& seen,
                            const Group& group,
                            const QString& userId)
{
    if (userId.isEmpty() || seen.contains(userId)) {
        return;
    }

    const CurrentUser& currentUser = CurrentUser::instance();
    QString displayName = currentUser.isCurrentUserId(userId)
            ? currentUser.getUserName()
            : group.memberNicknames.value(userId);
    if (displayName.isEmpty()) {
        displayName = userNameForIdentity(userId);
    }
    if (displayName.isEmpty()) {
        displayName = userId;
    }

    participants.push_back(SampleParticipant{
            userId,
            displayName,
            groupRoleForUser(group, userId)
    });
    seen.insert(userId);
}

QVector<SampleParticipant> sampleParticipantsForGroup(const Group& group, int ordinal)
{
    QVector<SampleParticipant> candidates;
    QSet<QString> seen;
    const CurrentUser& currentUser = CurrentUser::instance();

    appendGroupParticipant(candidates, seen, group, currentUser.getUserId());
    appendGroupParticipant(candidates, seen, group, group.ownerId);
    for (const QString& adminId : group.adminsID) {
        appendGroupParticipant(candidates, seen, group, adminId);
    }
    for (const QString& memberId : group.membersID) {
        appendGroupParticipant(candidates, seen, group, memberId);
    }

    if (candidates.size() <= 2) {
        return candidates;
    }

    const SampleParticipant currentUserParticipant = candidates.takeFirst();
    std::mt19937 generator(20240521 + ordinal * 97);
    std::shuffle(candidates.begin(), candidates.end(), generator);

    QVector<SampleParticipant> activeParticipants;
    activeParticipants.reserve(qMin(7, candidates.size() + 1));
    activeParticipants.push_back(currentUserParticipant);

    const int targetCount = qMin(candidates.size() + 1, qBound(3, 3 + (ordinal % 5), 7));
    for (int index = 0; index < candidates.size() && activeParticipants.size() < targetCount; ++index) {
        activeParticipants.push_back(candidates.at(index));
    }

    std::shuffle(activeParticipants.begin(), activeParticipants.end(), generator);
    return activeParticipants;
}

QString buildPreviewText(const QSharedPointer<ChatMessage>& message,
                         bool isGroup)
{
    if (!message) {
        return {};
    }

    if (isGroup) {
        QString senderName = message->getSenderName();
        if (senderName.isEmpty()) {
            senderName = userNameForIdentity(message->getSenderId());
        }
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

int sampleHistoryMessageCount(int ordinal)
{
    return 50 + (ordinal * 37) % 151;
}

QDateTime sampleLatestMessageTime(int ordinal)
{
    const QDateTime now = QDateTime::currentDateTime();
    const int hourOffset = (ordinal % 3) * 2;
    const int minuteOffset = (ordinal % 4) * 3;

    switch (sampleLatestBucket(ordinal)) {
    case SampleLatestBucket::Today:
        return now.addSecs((-12 - ordinal % 9) * 60);
    case SampleLatestBucket::Yesterday:
        return dateTimeAt(now.date().addDays(-1), 18 + hourOffset, 10 + minuteOffset);
    case SampleLatestBucket::DaysAgo:
        return dateTimeAt(now.date().addDays(-3), 15 + hourOffset, 8 + minuteOffset);
    case SampleLatestBucket::OneMonthAgo:
        return dateTimeAt(now.date().addMonths(-1), 11 + hourOffset, 9 + minuteOffset);
    case SampleLatestBucket::MonthsAgo:
        return dateTimeAt(now.date().addMonths(-4), 10 + hourOffset, 18 + minuteOffset);
    case SampleLatestBucket::OneYearAgo:
        return dateTimeAt(now.date().addYears(-1), 9 + hourOffset, 6 + minuteOffset);
    case SampleLatestBucket::YearsAgo:
    default:
        return dateTimeAt(now.date().addYears(-2), 8 + hourOffset, 12 + minuteOffset);
    }
}

QVector<QDateTime> buildHistoryTimeline(int ordinal)
{
    const int messageCount = sampleHistoryMessageCount(ordinal);
    QVector<QDateTime> timeline;
    timeline.resize(messageCount);

    QDateTime cursor = sampleLatestMessageTime(ordinal);
    for (int reverseIndex = 0; reverseIndex < messageCount; ++reverseIndex) {
        timeline[messageCount - reverseIndex - 1] = cursor;

        int gapMinutes = 3 + ((ordinal + reverseIndex * 7) % 11);
        if (reverseIndex % 12 == 11) {
            gapMinutes += 35 + (ordinal + reverseIndex) % 75;
        }
        if (reverseIndex % 37 == 36) {
            gapMinutes += (24 + (ordinal + reverseIndex) % 48) * 60;
        }
        cursor = cursor.addSecs(-gapMinutes * 60);
    }

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
    explicit ConversationMessagesRequestOperation(const QMap<QString, ChatMessageList>& store)
        : m_store(store)
    {
    }

private:
    ChatMessageList doRequest(const ConversationMessagesRequest& query) const override
    {
        const auto it = m_store.constFind(query.conversationId);
        if (it == m_store.constEnd() || it.value().isEmpty()) {
            return {};
        }

        const ChatMessageList& messages = it.value();
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

    const QMap<QString, ChatMessageList>& m_store;
};

class ConversationMetaRequestOperation final
    : public RepositoryTemplate<ConversationMetaRequest, ConversationMeta> {
public:
    explicit ConversationMetaRequestOperation(const QMap<QString, ConversationSyncState>& conversationStates)
        : m_conversationStates(conversationStates)
    {
    }

private:
    bool hasState(const QString& conversationId) const
    {
        return m_conversationStates.contains(conversationId);
    }

    ConversationSyncState stateFor(const QString& conversationId) const
    {
        ConversationSyncState state = m_conversationStates.value(conversationId);
        state.conversationId = conversationId;
        return state;
    }

    ConversationMeta doRequest(const ConversationMetaRequest& query) const override
    {
        if (GroupRepository::instance().contains(query.conversationId)) {
            const Group group = GroupRepository::instance().requestGroupDetail({query.conversationId});
            const ConversationSyncState state = stateFor(group.groupId);
            return ConversationMeta{
                    group.groupId,
                    groupDisplayName(group),
                    group.groupAvatarPath,
                    true,
                    group.memberNum,
                    Offline,
                    hasState(group.groupId) ? state.isDoNotDisturb : group.isDnd,
                    state.isPinned
            };
        }

        const User user = UserRepository::instance().requestUserDetail({query.conversationId});
        const ConversationSyncState state = stateFor(user.id);
        return ConversationMeta{
                user.id,
                userDisplayName(user),
                user.avatarPath,
                false,
                0,
                user.status,
                hasState(user.id) ? state.isDoNotDisturb : user.isDnd,
                state.isPinned
        };
    }

    const QMap<QString, ConversationSyncState>& m_conversationStates;
};

class ConversationListRequestOperation final
    : public RepositoryTemplate<ConversationListRequest, QVector<ConversationSummary>> {
public:
    ConversationListRequestOperation(const QMap<QString, ChatMessageList>& store,
                                     const QMap<QString, ConversationSyncState>& conversationStates)
        : m_store(store)
        , m_conversationStates(conversationStates)
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
            const auto messagesIt = m_store.constFind(group.groupId);
            const ChatMessageList* messages = messagesIt == m_store.constEnd() ? nullptr : &messagesIt.value();
            const QSharedPointer<ChatMessage> lastMessage = (!messages || messages->isEmpty())
                    ? QSharedPointer<ChatMessage>()
                    : messages->last();
            const ConversationSyncState state = stateFor(group.groupId);
            result.push_back(ConversationSummary{
                    group.groupId,
                    groupDisplayName(group),
                    group.groupAvatarPath,
                    buildPreviewText(lastMessage, true),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(group.groupId, lastMessage),
                    state.unreadCount,
                    state.isDoNotDisturb,
                    state.isPinned,
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
            const auto messagesIt = m_store.constFind(friendSummary.userId);
            const ChatMessageList* messages = messagesIt == m_store.constEnd() ? nullptr : &messagesIt.value();
            const QSharedPointer<ChatMessage> lastMessage = (!messages || messages->isEmpty())
                    ? QSharedPointer<ChatMessage>()
                    : messages->last();
            const ConversationSyncState state = stateFor(friendSummary.userId);
            result.push_back(ConversationSummary{
                    friendSummary.userId,
                    friendSummary.displayName,
                    friendSummary.avatarPath,
                    buildPreviewText(lastMessage, false),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    effectiveMessageListTime(friendSummary.userId, lastMessage),
                    state.unreadCount,
                    state.isDoNotDisturb,
                    state.isPinned,
                    false,
                    0
            });
        }

        return result;
    }

    void onAfterRequest(const ConversationListRequest&, QVector<ConversationSummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const ConversationSummary& lhs, const ConversationSummary& rhs) {
            if (lhs.isPinned != rhs.isPinned) {
                return lhs.isPinned;
            }
            return lhs.messageListTime > rhs.messageListTime;
        });
    }

    const QMap<QString, ChatMessageList>& m_store;
    const QMap<QString, ConversationSyncState>& m_conversationStates;
};

} // namespace

MessageRepository::MessageRepository(QObject* parent)
        : QObject(parent)
{
    auto addGeneratedDirectHistory = [this](const QString& conversationId,
                                            const QString& peerId,
                                            const QString& peerName,
                                            int ordinal) {
        const QVector<QDateTime> timeline = buildHistoryTimeline(ordinal);
        for (int index = 0; index < timeline.size(); ++index) {
            const bool fromMe = index % 4 == 1;
            const QString senderId = fromMe ? CurrentUser::instance().getUserId() : peerId;
            const QString senderName = fromMe ? CurrentUser::instance().getUserName() : peerName;
            auto msg = QSharedPointer<ChatMessage>(new TextMessage(
                    QStringLiteral("%1 历史消息 %2").arg(peerName).arg(index + 1),
                    fromMe,
                    senderId,
                    false,
                    senderName,
                    GroupRole::Member));
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

    auto addGeneratedGroupHistory = [this](const Group& group, int ordinal) {
        const QVector<QDateTime> timeline = buildHistoryTimeline(ordinal);
        const QVector<SampleParticipant> participants = sampleParticipantsForGroup(group, ordinal);
        if (participants.isEmpty()) {
            return;
        }

        QString lastSenderId;
        for (int index = 0; index < timeline.size(); ++index) {
            int senderIndex = (index + ordinal * 3 + index / participants.size()) % participants.size();
            if (participants.size() > 1 && participants.at(senderIndex).userId == lastSenderId) {
                senderIndex = (senderIndex + 1) % participants.size();
            }

            const SampleParticipant& sender = participants.at(senderIndex);
            const bool fromMe = CurrentUser::instance().isCurrentUserId(sender.userId);
            auto msg = QSharedPointer<ChatMessage>(new TextMessage(
                    QStringLiteral("%1 群聊消息 %2").arg(groupDisplayName(group)).arg(index + 1),
                    fromMe,
                    sender.userId,
                    true,
                    sender.displayName,
                    sender.role));
            msg->setTimestamp(timeline.at(index));
            addMessage(group.groupId, msg);
            lastSenderId = sender.userId;
        }

        ConversationSyncState state;
        state.conversationId = group.groupId;
        state.unreadCount = sampleUnreadCount(ordinal, timeline.size());
        state.isDoNotDisturb = sampleDoNotDisturb(ordinal);
        state.messageListTime = m_conversationStates.value(group.groupId).messageListTime;
        m_conversationStates.insert(group.groupId, state);
    };

    const QVector<Group> groups = GroupRepository::instance().requestGroupList();
    int conversationOrdinal = 0;
    for (int index = 0; index < qMin(kGroupConversationSampleCount, groups.size()); ++index) {
        const Group& group = groups.at(index);
        addGeneratedGroupHistory(group, conversationOrdinal++);
    }

    const QVector<FriendSummary> friends = sampledFriendsForMessages();
    for (const FriendSummary& friendSummary : friends) {
        addGeneratedDirectHistory(friendSummary.userId,
                                  friendSummary.userId,
                                  friendSummary.displayName,
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
    QMutexLocker locker(&m_mutex);
    return ConversationMetaRequestOperation(m_conversationStates).request(query);
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

void MessageRepository::markConversationUnread(const QString& conversationId, int unreadCount)
{
    if (conversationId.isEmpty()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.unreadCount = qMax(1, unreadCount);
    }
    emit conversationListChanged(conversationId);
}

void MessageRepository::setConversationDoNotDisturb(const QString& conversationId, bool enabled)
{
    if (conversationId.isEmpty()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        state.isDoNotDisturb = enabled;
    }
    emit conversationListChanged(conversationId);
}

void MessageRepository::setConversationPinned(const QString& conversationId, bool pinned)
{
    if (conversationId.isEmpty()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        if (state.isPinned == pinned) {
            return;
        }
        state.isPinned = pinned;
    }
    emit conversationListChanged(conversationId);
}

void MessageRepository::clearConversationMessages(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        changed = m_store.remove(conversationId) > 0;
        ConversationSyncState& state = m_conversationStates[conversationId];
        state.conversationId = conversationId;
        if (state.unreadCount != 0) {
            changed = true;
        }
        state.unreadCount = 0;
        state.lastReadAt = QDateTime::currentDateTime();
    }

    if (!changed) {
        return;
    }

    emit lastMessageChanged(conversationId, {});
    emit conversationListChanged(conversationId);
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
