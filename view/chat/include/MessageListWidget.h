#pragma once

#include "OverlayScrollListView.h"
#include "RepositoryTypes.h"

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

public slots:
    void onLastMessageUpdated(const QString& chatId,
                              const QString& text,
                              const QDateTime& timestamp);

private slots:
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);

private:
    void restoreSelection(const QString& conversationId);

    MessageListModel* m_model;
    MessageListDelegate* m_delegate;
    bool m_restoringSelection = false;
};
