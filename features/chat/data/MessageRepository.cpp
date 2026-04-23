#include "MessageRepository.h"

#include <QRandomGenerator>

#include "features/chat/data/GroupRepository.h"
#include "shared/data/RepositoryTemplate.h"
#include "features/friend/data/UserRepository.h"

namespace {

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
        return m_store.value(query.conversationId);
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
                    group.groupName,
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
                user.nick,
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
    explicit ConversationListRequestOperation(QMap<QString, ChatMessageList> store)
        : m_store(std::move(store))
    {
    }

private:
    QVector<ConversationSummary> doRequest(const ConversationListRequest&) const override
    {
        QVector<ConversationSummary> result;

        const QVector<Group> groups = GroupRepository::instance().requestGroupList();
        for (const Group& group : groups) {
            const ChatMessageList messages = m_store.value(group.groupId);
            const QSharedPointer<ChatMessage> lastMessage = messages.isEmpty() ? QSharedPointer<ChatMessage>() : messages.last();
            result.push_back(ConversationSummary{
                    group.groupId,
                    group.groupName,
                    group.groupAvatarPath,
                    buildPreviewText(lastMessage, true),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    static_cast<int>(messages.size()),
                    group.isDnd,
                    true,
                    group.memberNum
            });
        }

        const QVector<FriendSummary> friends = UserRepository::instance().requestFriendList();
        for (const FriendSummary& friendSummary : friends) {
            const ChatMessageList messages = m_store.value(friendSummary.userId);
            const QSharedPointer<ChatMessage> lastMessage = messages.isEmpty() ? QSharedPointer<ChatMessage>() : messages.last();
            result.push_back(ConversationSummary{
                    friendSummary.userId,
                    friendSummary.displayName,
                    friendSummary.avatarPath,
                    buildPreviewText(lastMessage, false),
                    lastMessage ? lastMessage->getTimestamp() : QDateTime(),
                    static_cast<int>(messages.size()),
                    friendSummary.isDoNotDisturb,
                    false,
                    0
            });
        }

        return result;
    }

    void onAfterRequest(const ConversationListRequest&, QVector<ConversationSummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const ConversationSummary& lhs, const ConversationSummary& rhs) {
            return lhs.lastMessageTime > rhs.lastMessageTime;
        });
    }

    QMap<QString, ChatMessageList> m_store;
};

} // namespace

MessageRepository::MessageRepository(QObject* parent)
        : QObject(parent)
{
    auto applyRandomOffset = [](const QSharedPointer<ChatMessage>& msg) {
        const int offset = QRandomGenerator::global()->bounded(0, 181);
        msg->setTimestamp(msg->getTimestamp().addSecs(offset));
    };

    const QVector<Group> groups = GroupRepository::instance().requestGroupList();
    for (const Group& group : groups) {
        auto msg = QSharedPointer<ChatMessage>(new TextMessage(
                QString("欢迎来到%1!").arg(group.groupName),
                false,
                group.ownerId,
                true,
                UserRepository::instance().requestUserName(group.ownerId),
                GroupRole::Owner));
        msg->setTimestamp(QDateTime::fromString("2024-05-21T13:14:00", Qt::ISODate));
        applyRandomOffset(msg);
        addMessage(group.groupId, msg);
    }

    const QVector<FriendSummary> friends = UserRepository::instance().requestFriendList();
    for (const FriendSummary& friendSummary : friends) {
        auto msg1 = QSharedPointer<ChatMessage>(new TextMessage(
                QString("你好，我是%1!").arg(friendSummary.displayName),
                false,
                friendSummary.userId,
                false,
                friendSummary.displayName));
        auto msg2 = QSharedPointer<ChatMessage>(new TextMessage(
                "很高兴认识你！",
                false,
                friendSummary.userId,
                false,
                friendSummary.displayName));

        msg1->setTimestamp(QDateTime::fromString("2024-05-21T13:14:00", Qt::ISODate));
        msg2->setTimestamp(QDateTime::fromString("2024-05-21T13:14:00", Qt::ISODate));
        applyRandomOffset(msg2);
        addMessage(friendSummary.userId, msg1);
        addMessage(friendSummary.userId, msg2);
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
    return ConversationListRequestOperation(m_store).request(query);
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
        thread.messages = m_store.value(query.conversationId);
    }
    return thread;
}

void MessageRepository::addMessage(const QString& conversationId,
                                   QSharedPointer<ChatMessage> message)
{
    {
        QMutexLocker locker(&m_mutex);
        m_store[conversationId].push_back(message);
    }
    emit lastMessageChanged(conversationId, message);
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
        } else {
            m_store.remove(conversationId);
        }
    }
    emit lastMessageChanged(conversationId, lastMsg);
}
