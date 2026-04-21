#pragma once

#include <QObject>
#include <QMutex>
#include <QVector>

#include "RepositoryTypes.h"

class AiChatRepository : public QObject {
public:
    static AiChatRepository& instance();

    QVector<AiChatListEntry> requestAiChatList(const AiChatListRequest& query = {}) const;
    QString createAiChatConversation(const QString& title,
                                     const QDateTime& time = QDateTime::currentDateTime());
    bool renameAiChatConversation(const QString& conversationId, const QString& title);
    bool removeAiChatConversation(const QString& conversationId);

private:
    explicit AiChatRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(AiChatRepository)

    mutable QMutex m_mutex;
    QVector<AiChatListEntry> m_entries;
    int m_nextConversationId = 1;
};
