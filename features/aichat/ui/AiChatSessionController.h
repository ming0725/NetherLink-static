#pragma once

#include <QObject>

#include "shared/types/RepositoryTypes.h"

class AiChatStreamClient;

class AiChatSessionController : public QObject
{
    Q_OBJECT

public:
    explicit AiChatSessionController(QObject* parent = nullptr);

    QVector<AiChatListEntry> loadConversations(const AiChatListRequest& query = {}) const;
    QVector<AiChatMessage> loadMessages(const QString& conversationId) const;
    QString createConversation(const QString& title);
    AiChatMessage submitUserMessage(const QString& conversationId, const QString& text);
    bool renameConversation(const QString& conversationId, const QString& title);
    bool deleteConversation(const QString& conversationId);

    bool hasActiveAiReplyStream() const;
    QString activeStreamConversationId() const;
    QString activeStreamMessageId() const;

public slots:
    void cancelActiveAiReplyStream();

signals:
    void conversationsChanged();
    void aiReplyStarted(const QString& conversationId);
    void aiReplyMessageAdded(const AiChatMessage& message);
    void aiReplyMessageUpdated(const QString& conversationId,
                               const QString& messageId,
                               const QString& text);
    void aiReplyFinished(const QString& conversationId, const QString& messageId);
    void aiReplyCanceled(const QString& conversationId, const QString& messageId);

private slots:
    void onAiReplyChunkReceived(const QString& chunk);
    void onAiReplyFinished();

private:
    void startAiReplyStream(const QString& conversationId, const QString& prompt);
    void resetActiveAiReplyStream();

    AiChatStreamClient* m_streamClient = nullptr;
    QString m_streamConversationId;
    QString m_streamMessageId;
    QString m_streamVisibleText;
};
