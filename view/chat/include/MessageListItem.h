// MessageListItem.h
#pragma once
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QDateTime>
#include "NotificationBadge.h"

struct MessageItemContent {
    QString name;
    QString text;
    QDateTime timestamp;
    int     unreadCount;
    bool    doNotDisturb;
    QString avatarPath;
    QString id;
    bool isGroup = false;
};

class MessageListItem : public QWidget {
    Q_OBJECT
public:
    explicit MessageListItem(const MessageItemContent& data, QWidget* parent = nullptr);
    QSize sizeHint() const Q_DECL_OVERRIDE;
    void setSelected(bool select);
    bool isSelected();

    void setLastTime(QDateTime time) { lastTime = time; update();
        resizeEvent(nullptr); }
    QDateTime getLastTime() const { return lastTime; }
    QString getChatID() const { return id; }
    void setLastText(QString text) { fullText = text; update();
        resizeEvent(nullptr); }

protected:
    void resizeEvent(QResizeEvent* ev) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent* ev) Q_DECL_OVERRIDE;
    void enterEvent(QEnterEvent*) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent*) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
signals:
    void itemClicked(MessageListItem* item);

private:
    void setupUI(const MessageItemContent& data);
    QLabel*   avatarLabel;
    QString   fullName;
    QString   fullText;
    bool      hovered  = false;
    bool      selected = false;
    NotificationBadge* badge;
    QDateTime lastTime;
    QString id;

    QRect avatarRect;
    QRect nameRect;
    QRect timeRect;
    QRect textRect;
    QRect badgeRect;

    static constexpr int avatarSize    = 48;
    static constexpr int leftPad       = 12;
    static constexpr int spacing       = 6;   // 头像与文字间距
    static constexpr int between       = 6;   // 两行文字行距
    static constexpr int iconSize      = 12;  // 提醒图标高度
};
