#pragma once

#include <QObject>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <QSharedPointer>
#include "shared/types/RepositoryTypes.h"
#include "shared/types/ChatMessage.h"

class MessageRepository : public QObject {
    Q_OBJECT
public:
    static MessageRepository& instance();

    QVector<ConversationSummary> requestConversationList(const ConversationListRequest& query = {}) const;
    ChatMessageList requestConversationMessages(const ConversationMessagesRequest& query) const;
    ConversationMeta requestConversationMeta(const ConversationMetaRequest& query) const;
    ConversationThreadData requestConversationThread(const ConversationThreadRequest& query) const;

public slots:
    void touchConversation(const QString& conversationId,
                           const QDateTime& timestamp = QDateTime::currentDateTime());
    void markConversationRead(const QString& conversationId);
    void markConversationUnread(const QString& conversationId, int unreadCount = 1);
    void setConversationDoNotDisturb(const QString& conversationId, bool enabled);
    void removeConversation(const QString& conversationId);
    void addMessage(const QString& conversationId,
                    QSharedPointer<ChatMessage> message);
    void removeMessage(const QString& conversationId, int index);
    bool removeMessage(const QString& conversationId,
                       const QSharedPointer<ChatMessage>& message);

signals:
    // conversationId 对应的最后一条消息已更新（nullptr 表示已无消息）
    void lastMessageChanged(const QString& conversationId,
                            QSharedPointer<ChatMessage> lastMessage);
    void conversationListChanged(const QString& conversationId);

private:
    explicit MessageRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(MessageRepository)

    QMap<QString, QVector<QSharedPointer<ChatMessage>>> m_store;
    QMap<QString, ConversationSyncState> m_conversationStates;
    mutable QMutex m_mutex;
};
