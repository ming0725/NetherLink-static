#include "MessageListDelegate.h"

#include <QApplication>
#include <QPainter>

#include "ImageService.h"
#include "MessageListModel.h"

MessageListDelegate::MessageListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void MessageListDelegate::paint(QPainter* painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const QColor backgroundColor = selected
            ? QColor(0x00, 0x99, 0xff)
            : (hovered ? QColor(0xf0, 0xf0, 0xf0) : QColor(0xff, 0xff, 0xff));
    painter->fillRect(option.rect, backgroundColor);

    const int itemCenterY = option.rect.center().y();
    const QRect avatarRect(option.rect.left() + kLeftPadding,
                           option.rect.top() + (kItemHeight - kAvatarSize) / 2,
                           kAvatarSize,
                           kAvatarSize);
    const QString avatarPath = index.data(MessageListModel::AvatarPathRole).toString();
    const QPixmap avatar = ImageService::instance().circularAvatar(avatarPath,
                                                                   kAvatarSize,
                                                                   painter->device()->devicePixelRatioF());
    painter->drawPixmap(avatarRect, avatar);

    const int contentLeft = avatarRect.right() + kContentSpacing + 1;
    const int rightEdge = option.rect.right() - kRightPadding;

    QFont timeFont = option.font;
    timeFont.setPixelSize(11);
    painter->setFont(timeFont);
    const QDateTime lastTime = index.data(MessageListModel::LastTimeRole).toDateTime();
    const QString timeText = lastTime.isValid() ? lastTime.toString("HH:mm") : QString();
    const QFontMetrics timeMetrics(timeFont);
    const int timeWidth = timeText.isEmpty() ? 0 : timeMetrics.horizontalAdvance(timeText);
    const int timeHeight = timeMetrics.height();

    const QFont nameFont = [&option]() {
        QFont font = option.font;
        font.setPixelSize(14);
        return font;
    }();
    const QFontMetrics nameMetrics(nameFont);
    const int nameHeight = nameMetrics.height();
    const int nameY = itemCenterY - kLineSpacing / 2 - nameHeight;

    const QRect timeRect(rightEdge - timeWidth,
                         nameY + (nameHeight - timeHeight) / 2,
                         timeWidth,
                         timeHeight);

    const BadgeLayout badgeLayout = badgeLayoutForItem(index, selected);
    const int badgeX = rightEdge - badgeLayout.size.width();

    const QFont previewFont = [&option]() {
        QFont font = option.font;
        font.setPixelSize(13);
        return font;
    }();
    const QFontMetrics previewMetrics(previewFont);
    const int previewHeight = previewMetrics.height();
    const int previewY = itemCenterY + kLineSpacing / 2;

    const int previewRight = badgeLayout.size.isValid()
            ? badgeX - kBadgeGap
            : rightEdge;
    const QRect previewRect(contentLeft,
                            previewY,
                            qMax(0, previewRight - contentLeft),
                            previewHeight);

    const int nameRight = timeText.isEmpty()
            ? rightEdge
            : timeRect.left() - kTimeGap;
    const QRect nameRect(contentLeft,
                         nameY,
                         qMax(0, nameRight - contentLeft),
                         nameHeight);

    painter->setFont(nameFont);
    painter->setPen(selected ? Qt::white : Qt::black);
    painter->drawText(nameRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(index.data(MessageListModel::TitleRole).toString(),
                                             Qt::ElideRight,
                                             nameRect.width()));

    if (!timeText.isEmpty()) {
        painter->setFont(timeFont);
        painter->setPen(selected ? Qt::white : QColor(0x88, 0x88, 0x88));
        painter->drawText(timeRect, Qt::AlignLeft | Qt::AlignVCenter, timeText);
    }

    painter->setFont(previewFont);
    painter->setPen(selected ? Qt::white : QColor(0x88, 0x88, 0x88));
    painter->drawText(previewRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      previewMetrics.elidedText(index.data(MessageListModel::PreviewTextRole).toString(),
                                                Qt::ElideRight,
                                                previewRect.width()));

    if (badgeLayout.size.isValid()) {
        const QRect badgeRect(badgeX,
                              previewY + (previewHeight - badgeLayout.size.height()) / 2,
                              badgeLayout.size.width(),
                              badgeLayout.size.height());
        drawBadge(painter, badgeRect, badgeLayout, selected);
    }

    painter->restore();
}

QSize MessageListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(0, kItemHeight);
}

MessageListDelegate::BadgeLayout MessageListDelegate::badgeLayoutForItem(const QModelIndex& index,
                                                                         bool selected) const
{
    const int unreadCount = index.data(MessageListModel::UnreadCountRole).toInt();
    const bool doNotDisturb = index.data(MessageListModel::DoNotDisturbRole).toBool();

    BadgeLayout layout;
    if (unreadCount <= 0 && !doNotDisturb) {
        return layout;
    }

    if (unreadCount <= 0 && doNotDisturb) {
        layout.drawIcon = true;
        layout.size = QSize(kBadgeHeight, kBadgeHeight);
        return layout;
    }

    layout.text = unreadCount < 100 ? QString::number(unreadCount) : QStringLiteral("99+");
    QFont badgeFont = QApplication::font();
    badgeFont.setBold(true);
    badgeFont.setPixelSize(11);
    const QFontMetrics metrics(badgeFont);
    const int textWidth = metrics.horizontalAdvance(layout.text);
    const int width = qMax(kBadgeHeight, textWidth + kBadgeHorizontalPadding * 2);
    layout.size = QSize(width, kBadgeHeight);
    layout.backgroundColor = doNotDisturb ? QColor(0xcc, 0xcc, 0xcc) : QColor(0xf7, 0x4c, 0x30);
    layout.textColor = doNotDisturb ? QColor(0xff, 0xfa, 0xfa) : QColor(0xff, 0xff, 0xff);

    if (selected && !doNotDisturb) {
        layout.backgroundColor = QColor(0xff, 0xff, 0xff, 210);
        layout.textColor = QColor(0x00, 0x99, 0xff);
    }

    return layout;
}

void MessageListDelegate::drawBadge(QPainter* painter,
                                    const QRect& rect,
                                    const BadgeLayout& badgeLayout,
                                    bool selected) const
{
    if (badgeLayout.drawIcon) {
        const QString iconPath = selected
                ? QStringLiteral(":/resources/icon/selected_notification.png")
                : QStringLiteral(":/resources/icon/notification.png");
        const QPixmap icon = ImageService::instance().scaled(iconPath,
                                                             rect.size(),
                                                             Qt::KeepAspectRatio,
                                                             painter->device()->devicePixelRatioF());
        painter->drawPixmap(rect, icon);
        return;
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(badgeLayout.backgroundColor);
    if (rect.width() == rect.height()) {
        painter->drawEllipse(rect);
    } else {
        painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);
    }

    QFont badgeFont = QApplication::font();
    badgeFont.setBold(true);
    badgeFont.setPixelSize(11);
    painter->setFont(badgeFont);
    painter->setPen(badgeLayout.textColor);
    painter->drawText(rect, Qt::AlignCenter, badgeLayout.text);
}
