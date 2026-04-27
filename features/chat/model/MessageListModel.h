#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "shared/types/RepositoryTypes.h"

class MessageListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        ConversationIdRole = Qt::UserRole + 1,
        TitleRole,
        AvatarPathRole,
        PreviewTextRole,
        LastTimeRole,
        UnreadCountRole,
        DoNotDisturbRole,
        IsPinnedRole,
        IsGroupRole,
        MemberCountRole,
        ContextMenuActiveRole
    };

    explicit MessageListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setConversations(QVector<ConversationSummary> conversations);
    void markConversationRead(const QString& conversationId);
    void markConversationUnread(const QString& conversationId, int unreadCount = 1);
    void setConversationDoNotDisturb(const QString& conversationId, bool enabled);
    void setConversationPinned(const QString& conversationId, bool pinned);
    void setContextMenuConversation(const QString& conversationId);
    bool removeConversation(const QString& conversationId);
    void updateConversationPreview(const QString& conversationId,
                                   const QString& previewText,
                                   const QDateTime& lastMessageTime);

    QString conversationIdAt(const QModelIndex& index) const;
    ConversationSummary conversationAt(const QModelIndex& index) const;
    ConversationSummary conversationById(const QString& conversationId) const;
    int indexOfConversation(const QString& conversationId) const;

private:
    void sortConversations();

    QVector<ConversationSummary> m_conversations;
    QString m_contextMenuConversationId;
};
