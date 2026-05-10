#include "AiChatMessageListModel.h"

#include <utility>

AiChatMessageListModel::AiChatMessageListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int AiChatMessageListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_messages.size();
}

QVariant AiChatMessageListModel::data(const QModelIndex& index, int role) const
{
    const AiChatMessage message = messageAt(index);
    if (message.messageId.isEmpty()) {
        return {};
    }

    switch (role) {
    case MessageIdRole:
        return message.messageId;
    case ConversationIdRole:
        return message.conversationId;
    case Qt::DisplayRole:
    case TextRole:
        return message.text;
    case IsFromUserRole:
        return message.isFromUser;
    case TimeRole:
        return message.time;
    default:
        return {};
    }
}

Qt::ItemFlags AiChatMessageListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled;
}

void AiChatMessageListModel::setMessages(QVector<AiChatMessage> messages)
{
    beginResetModel();
    m_messages = std::move(messages);
    endResetModel();
}

void AiChatMessageListModel::appendMessage(const AiChatMessage& message)
{
    if (message.messageId.isEmpty()) {
        return;
    }

    const int row = m_messages.size();
    beginInsertRows(QModelIndex(), row, row);
    m_messages.push_back(message);
    endInsertRows();
}

bool AiChatMessageListModel::updateMessageText(const QString& messageId, const QString& text)
{
    if (messageId.isEmpty() || text.isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages[row].messageId == messageId) {
            m_messages[row].text = text;
            const QModelIndex changedIndex = index(row, 0);
            emit dataChanged(changedIndex, changedIndex, QVector<int>{Qt::DisplayRole, TextRole});
            return true;
        }
    }

    return false;
}

AiChatMessage AiChatMessageListModel::messageAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size()) {
        return {};
    }
    return m_messages.at(index.row());
}

void AiChatMessageListModel::clear()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
}
