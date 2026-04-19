#include "FriendListItem.h"
#include "UserRepository.h"
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QRandomGenerator>
#include <QPushButton>

FriendListItem::FriendListItem(const User& user, QWidget* parent)
    : QWidget(parent)
    , avatarLabel(new QLabel(this))
    , fullNameText(user.nick)
    , status(user.status)
{
    fullStatusAndSignText = QString("[%1] %2").arg(statusText(user.status), user.signature);
    setMouseTracking(true);
    setupUI(user);
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void FriendListItem::setupUI(const User& user)
{
    const int avatarSize = 48;
    avatarLabel->setFixedSize(avatarSize, avatarSize);
    avatarLabel->setPixmap(UserRepository::instance().getAvatar(user.id));
    nameLabel = new QLabel(fullNameText, this);
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFont font;
    font.setBold(false);
    font.setPixelSize(14);

    nameLabel->setFont(font);

    const int statusIconSize = 13;
    statusIconLabel = new QLabel(this);
    statusIconLabel->setPixmap(QPixmap(statusIconPath(user.status))
                                       .scaled(statusIconSize, statusIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    statusIconLabel->setFixedSize(statusIconSize + 2, statusIconSize + 2);

    statusTextLabel = new QLabel(this);
    statusTextLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusTextLabel->setText(fullStatusAndSignText);
    font.setBold(false);
    font.setPixelSize(12);
    statusTextLabel->setFont(font);
    QPalette palette = statusTextLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::gray);
    statusTextLabel->setPalette(palette);
}

QSize FriendListItem::sizeHint() const
{
    return QSize(144, 72);
}


void FriendListItem::enterEvent(QEnterEvent*)
{
    hovered = true;
    update();
}

void FriendListItem::leaveEvent(QEvent*) {
    hovered = false;
    update();
}

void FriendListItem::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (selected) {
        painter.fillRect(rect(), QColor(0x0099ff));
        QPalette palette = statusTextLabel->palette();
        palette.setColor(QPalette::WindowText, Qt::white);
        statusTextLabel->setPalette(palette);
        nameLabel->setPalette(palette);
        return;
    }
    else {
        painter.fillRect(rect(), QColor(0xffffff));
        QPalette palette = statusTextLabel->palette();
        palette.setColor(QPalette::WindowText, Qt::gray);
        statusTextLabel->setPalette(palette);
        palette.setColor(QPalette::WindowText, Qt::black);
        nameLabel->setPalette(palette);
    }
    if (hovered) {
        painter.fillRect(rect(), QColor(0xf0f0f0));
    }
    else {
        painter.fillRect(rect(), QColor(0xffffff));
    }
}

void FriendListItem::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    static constexpr int avatarSize    = 48;
    static constexpr int leftPad       = 12;
    static constexpr int spacing       = 6;
    static constexpr int between       = 6;
    static constexpr int iconSize      = 12;
    static constexpr int icoTextDist   = 4;
    const int w = width();
    const int h = height();
    const int cy = h / 2;       // 垂直中心线 y
    // 1. 头像：垂直居中
    avatarLabel->setGeometry(
            leftPad,
            cy - avatarSize / 2,
            avatarSize,
            avatarSize
    );

    // 2. 计算文字区域起始 X
    const int contentX = leftPad + avatarSize + spacing;

    // 3. 两行文字的高度与偏移
    QFontMetrics fmName(nameLabel->font());
    QFontMetrics fmText(statusTextLabel->font());
    const int nameH   = fmName.height();
    const int textH   = fmText.height();
    // 根据 between 和行高，计算两行底部（姓名）与中轴线的距离
    // 假设文字对齐方式：姓名在上，消息在下

    // —— 第一行：姓名 ——
    const int nameY_bottom = cy - between / 2;    // 姓名底部 y
    const int nameY_top    = nameY_bottom - nameH;
    // 4. 姓名和时间戳底部对齐
    nameLabel->setGeometry(
            contentX,
            nameY_top,
            w - contentX - leftPad - spacing,
            nameH
    );
    {
        QFontMetrics fm(nameLabel->font());
        int nameW = nameLabel->width();
        nameLabel->setText(fm.elidedText(fullNameText, Qt::ElideRight, nameW));
    }
    // —— 第二行：消息 ——
    const int textY_top = cy + between / 2;
    const int textW_available = w - contentX - leftPad - iconSize - spacing;
    statusIconLabel->setGeometry(
            contentX,
            textY_top + 1,
            statusIconLabel->width(),
            statusIconLabel->height()
    );
    statusTextLabel->setGeometry(
            contentX + statusIconLabel->width() + icoTextDist,
            textY_top + 1,
            textW_available,
            textH
    );
    {
        QFontMetrics fm(statusTextLabel->font());
        statusTextLabel->setText(fm.elidedText(fullStatusAndSignText, Qt::ElideRight, textW_available));
    }
}

void FriendListItem::mousePressEvent(QMouseEvent* event)
{
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
        emit itemClicked(this);
    }
}

void FriendListItem::setSelected(bool select)
{
    this->selected = select;
    update();
}

bool FriendListItem::isSelected()
{
    return selected;
}
