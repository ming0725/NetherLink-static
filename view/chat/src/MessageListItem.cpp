// MessageListItem.cpp
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QStyle>
#include <QFont>
#include "UserRepository.h"
#include "MessageListItem.h"

MessageListItem::MessageListItem(const MessageItemContent& data, QWidget* parent)
        : QWidget(parent)
        , avatarLabel(new QLabel(this))
        , badge(new NotificationBadge(this))
        , id(data.id)
        , fullName(data.name)
        , fullText(data.text)
        , lastTime(data.timestamp)
{
    setMouseTracking(true);
    setupUI(data);
    badge->setDoNotDisturb(data.doNotDisturb);
    badge->setCount(data.unreadCount);
    resizeEvent(nullptr);
}

QSize MessageListItem::sizeHint() const {
    return QSize(144, 72);
}

void MessageListItem::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);

    int w = width();
    int h = height();
    int cy = h / 2;

    // 头像位置
    avatarRect = QRect(leftPad,
                       cy - avatarSize/2,
                       avatarSize, avatarSize);
    avatarLabel->setGeometry(avatarRect);

    int contentX = avatarRect.right() + spacing;

    // 名称区域
    QFont nameFont = font();
    nameFont.setPixelSize(14);
    QFontMetrics fmName(nameFont, this);
    int nameH = fmName.height();
    int nameY = cy - between/2 - nameH;
    int nameW = w - contentX - leftPad - spacing - 40; // 留出时间宽度
    nameRect = QRect(contentX, nameY, nameW, nameH);

    // 时间区域
    QString timeStr = lastTime.toString("HH:mm");
    QFont timeFont = font();
    timeFont.setPixelSize(11);
    QFontMetrics fmTime(timeFont, this);
    QSize tsz = fmTime.size(Qt::TextSingleLine, timeStr);
    int tx = w - leftPad - tsz.width();
    int ty = nameY + (nameH - tsz.height())/2;
    timeRect = QRect(tx, ty, tsz.width(), tsz.height());

    // 文本区域
    QFont textFont = font();
    textFont.setPixelSize(13);
    QFontMetrics fmText(textFont, this);
    int textH = fmText.height();
    int textY = cy + between/2;
    int textW = w - contentX - leftPad - badge->sizeHint().width() - spacing;
    textRect = QRect(contentX, textY, textW, textH);

    // 徽章位置
    QSize badgeSz = badge->sizeHint();
    int bx = w - leftPad - badgeSz.width() + 2;
    badgeRect = QRect(bx, textY, badgeSz.width(), badgeSz.height());
    badge->setGeometry(badgeRect);
}


void MessageListItem::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 背景
    QColor bg = selected ? QColor(0x0099ff)
                         : hovered  ? QColor(0xf0f0f0)
                                    : QColor(0xffffff);
    p.fillRect(rect(), bg);

    auto f=font(); f.setPixelSize(14);
    // 姓名
    p.setFont(f);
    p.setPen(selected ? Qt::white : Qt::black);
    p.drawText(nameRect, Qt::AlignLeft|Qt::AlignVCenter,
               QFontMetrics(p.font()).elidedText(fullName, Qt::ElideRight, nameRect.width()));

    // 时间
    QString timeStr = lastTime.toString("HH:mm");
    f.setPixelSize(11);
    p.setFont(f);
    p.setPen(selected ? Qt::white : QColor(0x88,0x88,0x88));
    p.drawText(timeRect, Qt::AlignLeft|Qt::AlignVCenter, timeStr);

    // 文本
    f.setPixelSize(13);
    p.setFont(f);
    p.setPen(selected ? Qt::white : QColor(0x88,0x88,0x88));
    p.drawText(textRect, Qt::AlignLeft|Qt::AlignVCenter,
               QFontMetrics(p.font()).elidedText(fullText, Qt::ElideRight, textRect.width()));
}


void MessageListItem::setupUI(const MessageItemContent& data)
{
    if (data.isGroup) {
        QPixmap pixmap;
        QPixmapCache::find("group_avatar", &pixmap);
        avatarLabel->setPixmap(pixmap);
    }
    else {
        avatarLabel->setPixmap(UserRepository::instance().getAvatar(data.id));
    }
    avatarLabel->setFixedSize(avatarSize, avatarSize);
    badge->setDoNotDisturb(data.doNotDisturb);
    badge->setCount(data.unreadCount);
}


void MessageListItem::enterEvent(QEnterEvent*) {
    hovered = true;
    update();
}

void MessageListItem::leaveEvent(QEvent*) {
    hovered = false;
    update();
}

void MessageListItem::mousePressEvent(QMouseEvent* event) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
        emit itemClicked(this);
        badge->setCount(0);
        badge->setSelected(true);
    }
}

void MessageListItem::setSelected(bool select) {
    selected = select;
    badge->setSelected(select);
    update();
}

bool MessageListItem::isSelected() {
    return selected;
}