#pragma once

#include "shared/ui/OverlayScrollListView.h"
#include "shared/types/RepositoryTypes.h"

class MessageListDelegate;
class MessageListModel;

class MessageListWidget : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit MessageListWidget(QWidget* parent = nullptr);

    QString selectedConversationId() const;
    ConversationSummary selectedConversation() const;
    void clearCurrentConversationSelection();

signals:
    void conversationActivated(const QString& conversationId);

private slots:
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void onRepositoryLastMessageChanged(const QString& conversationId,
                                        QSharedPointer<ChatMessage> lastMessage);

private:
    void restoreSelection(const QString& conversationId);
    QString previewTextForMessage(const QString& conversationId,
                                  const QSharedPointer<ChatMessage>& message) const;

    MessageListModel* m_model;
    MessageListDelegate* m_delegate;
    bool m_restoringSelection = false;
};
