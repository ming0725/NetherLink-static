#ifndef CHATAREA_H
#define CHATAREA_H

#include <QWidget>
#include <QSharedPointer>
#include <QDateTime>
#include "ChatListView.h"
#include "features/chat/model/ChatListModel.h"
#include "ChatItemDelegate.h"
#include "shared/types/ChatMessage.h"
#include "NewMessageNotifier.h"
#include "shared/ui/FloatingInputBar.h"
#include "shared/types/RepositoryTypes.h"


class ChatArea : public QWidget
{
    Q_OBJECT
    using ChatMessagePtr = QSharedPointer<ChatMessage>;
public:
    explicit ChatArea(QWidget *parent = nullptr);
    void openConversation(const ConversationThreadData& conversation);
    void addMessage(ChatMessagePtr message);
    void addImageMessage(QSharedPointer<ImageMessage> message,
                         const QDateTime& timestamp = QDateTime::currentDateTime());
    void addTextMessage(QSharedPointer<TextMessage> message,
                        const QDateTime& timestamp = QDateTime::currentDateTime());
    void clearMessageSelection();
    void handleGlobalMousePress(const QPoint& globalPos);
protected:
    void resizeEvent(QResizeEvent *event) override;
private slots:
    void onScrollValueChanged(int value);
    void onNewMessageNotifierClicked();
    void onSendImage(const QString &path);
    void onSendText(const QString &text);
    void onSendTextAsPeer(const QString &text);

private:
    struct ConversationState {
        ConversationMeta meta;
        int unreadMessageCount = 0;
        bool isAtBottom = true;
    };

    ChatListView* chatView;
    ChatListModel* chatModel;
    ChatItemDelegate* chatDelegate;
    NewMessageNotifier* newMessageNotifier;
    QWidget* bottomGapGradientOverlay;
    FloatingInputBar* inputBar;
    QLabel* statusIcon;
    QLabel* nameLabel;
    ConversationState m_state;
    
    void updateNewMessageNotifier();
    void updateNewMessageNotifierPosition();
    void scrollToBottom();
    bool isScrollAtBottom() const;
    bool isNearBottom() const;
    void adjustBottomSpace();
    void updateInputBarPosition();
    void applyConversationMeta();
    void clearConversation();
    QString conversationId() const;
    bool isGroupMode() const;
};

#endif // CHATAREA_H 
