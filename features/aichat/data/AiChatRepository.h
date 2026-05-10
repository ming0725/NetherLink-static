#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QVector>

#include "shared/types/RepositoryTypes.h"

class AiChatRepository : public QObject {
public:
    static AiChatRepository& instance();

    QVector<AiChatListEntry> requestAiChatList(const AiChatListRequest& query = {}) const;
    QVector<AiChatMessage> requestAiChatMessages(const QString& conversationId) const;
    QString createAiChatConversation(const QString& title,
                                     const QDateTime& time = QDateTime::currentDateTime());
    AiChatMessage addAiChatMessage(const QString& conversationId,
                                   const QString& text,
                                   bool isFromUser,
                                   const QDateTime& time = QDateTime::currentDateTime());
    bool updateAiChatMessageText(const QString& conversationId,
                                 const QString& messageId,
                                 const QString& text,
                                 const QDateTime& time = QDateTime::currentDateTime());
    bool renameAiChatConversation(const QString& conversationId, const QString& title);
    bool removeAiChatConversation(const QString& conversationId);

private:
    explicit AiChatRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(AiChatRepository)

    void appendInitialMessages(const AiChatListEntry& entry, int sampleIndex);

    mutable QMutex m_mutex;
    QVector<AiChatListEntry> m_entries;
    QHash<QString, QVector<AiChatMessage>> m_messages;
    int m_nextConversationId = 1;
    int m_nextMessageId = 1;
};
