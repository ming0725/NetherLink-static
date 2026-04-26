#include "MessageListModel.h"

#include <QSize>

#include <algorithm>

MessageListModel::MessageListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int MessageListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_conversations.size());
}

QVariant MessageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_conversations.size()) {
        return {};
    }

    const ConversationSummary& conversation = m_conversations.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return conversation.title;
    case ConversationIdRole:
        return conversation.conversationId;
    case AvatarPathRole:
        return conversation.avatarPath;
    case PreviewTextRole:
        return conversation.previewText;
    case LastTimeRole:
        return conversation.messageListTime;
    case UnreadCountRole:
        return conversation.unreadCount;
    case DoNotDisturbRole:
        return conversation.isDoNotDisturb;
    case IsGroupRole:
        return conversation.isGroup;
    case MemberCountRole:
        return conversation.memberCount;
    case Qt::SizeHintRole:
        return QSize(0, 72);
    default:
        return {};
    }
}

Qt::ItemFlags MessageListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void MessageListModel::setConversations(QVector<ConversationSummary> conversations)
{
    beginResetModel();
    m_conversations = std::move(conversations);
    sortConversations();
    endResetModel();
}

void MessageListModel::markConversationRead(const QString& conversationId)
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return;
    }

    if (m_conversations[row].unreadCount == 0) {
        return;
    }

    m_conversations[row].unreadCount = 0;
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {UnreadCountRole});
}

void MessageListModel::updateConversationPreview(const QString& conversationId,
                                                 const QString& previewText,
                                                 const QDateTime& lastMessageTime)
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return;
    }

    m_conversations[row].previewText = previewText;
    m_conversations[row].lastMessageTime = lastMessageTime;
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {PreviewTextRole});
}

QString MessageListModel::conversationIdAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_conversations.size()) {
        return {};
    }
    return m_conversations.at(index.row()).conversationId;
}

ConversationSummary MessageListModel::conversationAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_conversations.size()) {
        return {};
    }
    return m_conversations.at(index.row());
}

ConversationSummary MessageListModel::conversationById(const QString& conversationId) const
{
    const int row = indexOfConversation(conversationId);
    if (row < 0) {
        return {};
    }
    return m_conversations.at(row);
}

int MessageListModel::indexOfConversation(const QString& conversationId) const
{
    for (int row = 0; row < m_conversations.size(); ++row) {
        if (m_conversations.at(row).conversationId == conversationId) {
            return row;
        }
    }
    return -1;
}

void MessageListModel::sortConversations()
{
    std::sort(m_conversations.begin(), m_conversations.end(),
              [](const ConversationSummary& lhs, const ConversationSummary& rhs) {
                  return lhs.messageListTime > rhs.messageListTime;
              });
}
