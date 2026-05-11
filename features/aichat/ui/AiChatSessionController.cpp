#include "features/aichat/ui/AiChatSessionController.h"

#include "features/aichat/data/AiChatRepository.h"
#include "features/aichat/data/AiChatStreamClient.h"

AiChatSessionController::AiChatSessionController(QObject* parent)
    : QObject(parent)
    , m_streamClient(new AiChatStreamClient(this))
{
    connect(m_streamClient, &AiChatStreamClient::chunkReceived,
            this, &AiChatSessionController::onAiReplyChunkReceived);
    connect(m_streamClient, &AiChatStreamClient::finished,
            this, &AiChatSessionController::onAiReplyFinished);
}

QVector<AiChatListEntry> AiChatSessionController::loadConversations(const AiChatListRequest& query) const
{
    return AiChatRepository::instance().requestAiChatList(query);
}

QVector<AiChatMessage> AiChatSessionController::loadMessages(const QString& conversationId) const
{
    return AiChatRepository::instance().requestAiChatMessages(conversationId);
}

QString AiChatSessionController::createConversation(const QString& title)
{
    const QString conversationId = AiChatRepository::instance().createAiChatConversation(title);
    if (!conversationId.isEmpty()) {
        emit conversationsChanged();
    }
    return conversationId;
}

AiChatMessage AiChatSessionController::submitUserMessage(const QString& conversationId, const QString& text)
{
    if (conversationId.isEmpty() || hasActiveAiReplyStream()) {
        return {};
    }

    const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
            conversationId,
            text,
            true);
    if (message.messageId.isEmpty()) {
        return {};
    }

    startAiReplyStream(conversationId, text);
    return message;
}

bool AiChatSessionController::renameConversation(const QString& conversationId, const QString& title)
{
    const bool renamed = AiChatRepository::instance().renameAiChatConversation(conversationId, title);
    if (renamed) {
        emit conversationsChanged();
    }
    return renamed;
}

bool AiChatSessionController::deleteConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    if (conversationId == m_streamConversationId) {
        cancelActiveAiReplyStream();
    }

    const bool removed = AiChatRepository::instance().removeAiChatConversation(conversationId);
    if (removed) {
        emit conversationsChanged();
    }
    return removed;
}

bool AiChatSessionController::hasActiveAiReplyStream() const
{
    return !m_streamConversationId.isEmpty() || (m_streamClient && m_streamClient->isRunning());
}

QString AiChatSessionController::activeStreamConversationId() const
{
    return m_streamConversationId;
}

QString AiChatSessionController::activeStreamMessageId() const
{
    return m_streamMessageId;
}

void AiChatSessionController::cancelActiveAiReplyStream()
{
    const QString conversationId = m_streamConversationId;
    const QString messageId = m_streamMessageId;
    if (m_streamClient) {
        m_streamClient->cancel();
    }
    resetActiveAiReplyStream();
    if (!conversationId.isEmpty()) {
        emit aiReplyCanceled(conversationId, messageId);
    }
}

void AiChatSessionController::onAiReplyChunkReceived(const QString& chunk)
{
    if (chunk.isEmpty() || m_streamConversationId.isEmpty()) {
        return;
    }

    m_streamVisibleText += chunk;
    if (m_streamMessageId.isEmpty()) {
        const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
                m_streamConversationId,
                m_streamVisibleText,
                false);
        if (message.messageId.isEmpty()) {
            cancelActiveAiReplyStream();
            return;
        }

        m_streamMessageId = message.messageId;
        emit aiReplyMessageAdded(message);
        return;
    }

    AiChatRepository::instance().updateAiChatMessageText(m_streamConversationId,
                                                         m_streamMessageId,
                                                         m_streamVisibleText);
    emit aiReplyMessageUpdated(m_streamConversationId, m_streamMessageId, m_streamVisibleText);
}

void AiChatSessionController::onAiReplyFinished()
{
    const QString conversationId = m_streamConversationId;
    const QString messageId = m_streamMessageId;
    resetActiveAiReplyStream();
    if (!conversationId.isEmpty()) {
        emit conversationsChanged();
        emit aiReplyFinished(conversationId, messageId);
    }
}

void AiChatSessionController::startAiReplyStream(const QString& conversationId, const QString& prompt)
{
    if (conversationId.isEmpty() || hasActiveAiReplyStream()) {
        return;
    }

    m_streamConversationId = conversationId;
    m_streamMessageId.clear();
    m_streamVisibleText.clear();
    emit aiReplyStarted(conversationId);

    m_streamClient->start(prompt);
    if (!m_streamClient->isRunning()) {
        cancelActiveAiReplyStream();
    }
}

void AiChatSessionController::resetActiveAiReplyStream()
{
    m_streamConversationId.clear();
    m_streamMessageId.clear();
    m_streamVisibleText.clear();
}
