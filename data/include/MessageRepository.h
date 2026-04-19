#pragma once

#include <QObject>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <QSharedPointer>
#include "ChatMessage.h"

class MessageRepository : public QObject {
    Q_OBJECT
public:
    static MessageRepository& instance();

    // 获取某会话的全部消息
    QVector<QSharedPointer<ChatMessage>> getMessages(const QString& conversationId);

    // 获取某会话的最后一条消息（可能为 nullptr）
    QSharedPointer<ChatMessage> getLastMessage(const QString& conversationId);

public slots:
    // 添加一条消息到会话（单聊或群聊），会发 lastMessageChanged
    void addMessage(const QString& conversationId,
                    QSharedPointer<ChatMessage> message);

    // 删除某会话中索引为 index 的消息，之后发 lastMessageChanged
    void removeMessage(const QString& conversationId, int index);

signals:
    // conversationId 对应的最后一条消息已更新（nullptr 表示已无消息）
    void lastMessageChanged(const QString& conversationId,
                            QSharedPointer<ChatMessage> lastMessage);

private:
    explicit MessageRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(MessageRepository)

    QMap<QString, QVector<QSharedPointer<ChatMessage>>> m_store;
    QMutex m_mutex;
};
