#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "shared/types/RepositoryTypes.h"

class AiChatMessageListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        MessageIdRole = Qt::UserRole + 1,
        ConversationIdRole,
        TextRole,
        IsFromUserRole,
        TimeRole,
        IsBottomSpaceRole,
        BottomSpaceHeightRole
    };

    explicit AiChatMessageListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setMessages(QVector<AiChatMessage> messages);
    void appendMessage(const AiChatMessage& message);
    bool updateMessageText(const QString& messageId, const QString& text);
    bool isBottomSpace(int row) const;
    void setBottomSpaceHeight(int height);
    AiChatMessage messageAt(const QModelIndex& index) const;
    void clear();

private:
    QVector<AiChatMessage> m_messages;
    int m_bottomSpaceHeight = 0;
};
