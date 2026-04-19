#include "MessageRepository.h"
#include "UserRepository.h"
#include "GroupRepository.h"
#include <QRandomGenerator>

MessageRepository::MessageRepository(QObject* parent)
        : QObject(parent)
{
    auto applyRandomOffset = [](QSharedPointer<ChatMessage> &msg) {
        // 随机产生 [-180, +180] 之间的秒数
        int offset = QRandomGenerator::global()->bounded(0, 181);
        msg->setTimestamp(msg->getTimestamp().addSecs(offset));
    };

    // 群聊初始消息
    auto groups = GroupRepository::instance().getAllGroup();
    for (auto group : groups) {
        auto text = QString("欢迎来到%1!").arg(group.groupName);
        auto msg = QSharedPointer<ChatMessage>(new TextMessage(
                text,
                false,
                group.ownerId,
                true,
                UserRepository::instance().getName(group.ownerId),
                GroupRole::Owner));

        // 基准时间
        msg->setTimestamp(QDateTime::fromString("2024-05-21T13:14:00", Qt::ISODate));
        // 随机扰动 ±3 分钟
        applyRandomOffset(msg);

        addMessage(group.groupId, msg);
    }

    // 单聊初始消息
    auto users = UserRepository::instance().getAllUser();
    for (auto user : users) {
        auto text1 = QString("你好，我是%1!").arg(user.nick);
        auto text2 = QString("很高兴认识你！");

        auto msg1 = QSharedPointer<ChatMessage>(new TextMessage(
                text1,
                false,
                user.id,
                false,
                user.nick));
        auto msg2 = QSharedPointer<ChatMessage>(new TextMessage(
                text2,
                false,
                user.id,
                false,
                user.nick));

        msg1->setTimestamp(QDateTime::fromString("2024-05-21T13:14:00", Qt::ISODate));
        msg2->setTimestamp(QDateTime::fromString("2024-05-21T13:14:00", Qt::ISODate));
        applyRandomOffset(msg2);
        addMessage(user.id, msg1);
        addMessage(user.id, msg2);
    }
}

MessageRepository& MessageRepository::instance()
{
    static MessageRepository repo;
    return repo;
}

QVector<QSharedPointer<ChatMessage>>
MessageRepository::getMessages(const QString& conversationId)
{
    QMutexLocker locker(&m_mutex);
    return m_store.value(conversationId);
}

QSharedPointer<ChatMessage>
MessageRepository::getLastMessage(const QString& conversationId)
{
    QMutexLocker locker(&m_mutex);
    const auto vec = m_store.value(conversationId);
    if (!vec.isEmpty())
        return vec.last();
    return QSharedPointer<ChatMessage>(nullptr);
}

void MessageRepository::addMessage(const QString& conversationId,
                                   QSharedPointer<ChatMessage> message)
{
    {
        QMutexLocker locker(&m_mutex);
        m_store[conversationId].push_back(message);
    }
    // 发射最新一条
    emit lastMessageChanged(conversationId, message);
}

void MessageRepository::removeMessage(const QString& conversationId, int index)
{
    QSharedPointer<ChatMessage> lastMsg;
    {
        QMutexLocker locker(&m_mutex);
        auto &vec = m_store[conversationId];
        if (index >= 0 && index < vec.size())
            vec.removeAt(index);
        // 更新 lastMsg
        if (!vec.isEmpty())
            lastMsg = vec.last();
        else
            m_store.remove(conversationId);
    }
    emit lastMessageChanged(conversationId, lastMsg);
}
