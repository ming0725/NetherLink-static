#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QString>
#include <QDateTime>
#include <memory>

enum class MessageType {
    Text,
    Image,
    File,
    Voice
};

enum class GroupRole {
    Owner,      // 群主
    Admin,      // 管理员
    Member      // 普通成员
};

class ChatMessage {
public:
    ChatMessage(bool isFromMe, const QString& senderId, bool isGroupChat = false, 
               const QString& senderName = QString(), GroupRole role = GroupRole::Member)
        : fromMe(isFromMe), senderId(senderId), timestamp(QDateTime::currentDateTime()), 
          isSelected(false), isGroupChat(isGroupChat), senderName(senderName), role(role) {}
    
    virtual ~ChatMessage() = default;
    virtual QString getContent() const = 0;
    virtual MessageType getType() const = 0;
    
    bool isFromMe() const { return fromMe; }
    QString getSenderId() const { return senderId; }
    QDateTime getTimestamp() const { return timestamp; }
    void setTimestamp(const QDateTime& newTimestamp) { timestamp = newTimestamp; }
    
    bool getIsSelected() const { return isSelected; }
    void setSelected(bool selected) { isSelected = selected; }

    // 群聊相关
    bool isInGroupChat() const { return isGroupChat; }
    QString getSenderName() const { return senderName; }
    GroupRole getRole() const { return role; }

protected:
    bool fromMe;
    QString senderId;
    QDateTime timestamp;
    bool isSelected;
    
    // 群聊相关属性
    bool isGroupChat;
    QString senderName;
    GroupRole role;
};

class TextMessage : public ChatMessage {
public:
    TextMessage(const QString& text, bool isFromMe, const QString& senderId,
               bool isGroupChat = false, const QString& senderName = QString(),
               GroupRole role = GroupRole::Member)
        : ChatMessage(isFromMe, senderId, isGroupChat, senderName, role), text(text) {}

    QString getContent() const override { return text; }
    MessageType getType() const override { return MessageType::Text; }
    QString getText() const { return text; }

private:
    QString text;
};

class ImageMessage : public ChatMessage {
public:
    ImageMessage(const QString& imageSource, bool isFromMe, const QString& senderId,
                bool isGroupChat = false, const QString& senderName = QString(),
                GroupRole role = GroupRole::Member)
        : ChatMessage(isFromMe, senderId, isGroupChat, senderName, role), imageSource(imageSource) {}
    
    QString getContent() const override { return "[图片]"; }
    MessageType getType() const override { return MessageType::Image; }
    QString getImageSource() const { return imageSource; }

private:
    QString imageSource;
};

#endif // CHATMESSAGE_H
