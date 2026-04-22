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
    void initMessage(QVector<ChatMessagePtr>&);
    void addMessage(ChatMessagePtr message);
    void addImageMessage(QSharedPointer<ImageMessage> message,
                         const QDateTime& timestamp = QDateTime::currentDateTime());
    void addTextMessage(QSharedPointer<TextMessage> message,
                        const QDateTime& timestamp = QDateTime::currentDateTime());
    void setConversationMeta(const ConversationMeta& meta);
    void setMessageId(QString id);
    void clearAll();
protected:
    void resizeEvent(QResizeEvent *event) override;
signals:
    void sendMessage(const QString& chatId,
                     const QString& text,
                     const QDateTime& timestamp);
private slots:
    void onScrollValueChanged(int value);
    void onNewMessageNotifierClicked();
    void onSendImage(const QString &path);
    void onSendText(const QString &text);
    void onSendTextAsPeer(const QString &text);

private:
    ChatListView* chatView;
    ChatListModel* chatModel;
    ChatItemDelegate* chatDelegate;
    NewMessageNotifier* newMessageNotifier;
    FloatingInputBar* inputBar;
    QLabel* statusIcon;
    QLabel* nameLabel;
    
    int unreadMessageCount;
    bool isAtBottom;
    bool isGroupMode;
    QString messageId;
    ConversationMeta m_conversationMeta;
    
    void updateNewMessageNotifier();
    void updateNewMessageNotifierPosition();
    void scrollToBottom();
    bool isScrollAtBottom() const;
    bool isNearBottom() const;
    void adjustBottomSpace();
    void updateInputBarPosition();
    QString previewTextForMessage(const ChatMessage* message) const;
};

#endif // CHATAREA_H 
