#pragma once

#include "shared/ui/OverlayScrollListView.h"
#include "shared/types/RepositoryTypes.h"

class MessageListDelegate;
class MessageListModel;
class QTimer;

class MessageListWidget : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit MessageListWidget(QWidget* parent = nullptr);
    void setKeyword(const QString& keyword);

    QString selectedConversationId() const;
    ConversationSummary selectedConversation() const;
    void setCurrentConversation(const QString& conversationId);
    void clearCurrentConversationSelection();

signals:
    void conversationActivated(const QString& conversationId);

private slots:
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void onRepositoryLastMessageChanged(const QString& conversationId,
                                        QSharedPointer<ChatMessage> lastMessage);
    void reloadConversations();

private:
    struct ViewState {
        QString keyword;
    };

    void restoreSelection(const QString& conversationId);
    QString previewTextForMessage(const QString& conversationId,
                                  const QSharedPointer<ChatMessage>& message) const;

    MessageListModel* m_model;
    MessageListDelegate* m_delegate;
    QTimer* m_searchDebounceTimer;
    ViewState m_state;
    bool m_restoringSelection = false;
};
