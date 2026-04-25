#include "FriendListDelegate.h"

#include <QApplication>
#include <QPainter>

#include "shared/services/ImageService.h"
#include "features/friend/model/FriendListModel.h"
#include "shared/types/User.h"

extern const int kContactGroupArrowYOffset = 1;

namespace {

const int kNoticeArrowYOffset = 2;

const QFont& nameFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(14);
        return value;
    }();
    return font;
}

const QFontMetrics& nameMetrics()
{
    static const QFontMetrics metrics(nameFont());
    return metrics;
}

void drawFriendDisplayName(QPainter* painter,
                           const QRect& rect,
                           const QModelIndex& index,
                           bool selected)
{
    const QString remark = index.data(FriendListModel::RemarkRole).toString();
    const QString nickName = index.data(FriendListModel::NickNameRole).toString();
    const QString displayName = index.data(FriendListModel::DisplayNameRole).toString();
    const QColor primaryColor = selected ? Qt::white : Qt::black;
    const QColor secondaryColor = selected ? QColor(0xff, 0xff, 0xff, 165)
                                           : QColor(0x00, 0x00, 0x00, 120);

    painter->setFont(nameFont());
    if (remark.isEmpty() || nickName.isEmpty()) {
        painter->setPen(primaryColor);
        painter->drawText(rect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          nameMetrics().elidedText(displayName, Qt::ElideRight, rect.width()));
        return;
    }

    const QString suffix = QStringLiteral("(%1)").arg(nickName);
    const int gap = 2;
    const QString elidedRemark = nameMetrics().elidedText(remark, Qt::ElideRight, rect.width());
    const int usedRemarkWidth = nameMetrics().horizontalAdvance(elidedRemark);

    painter->setPen(primaryColor);
    painter->drawText(QRect(rect.left(), rect.top(), qMin(usedRemarkWidth, rect.width()), rect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      elidedRemark);

    if (rect.width() > usedRemarkWidth + gap) {
        painter->setPen(secondaryColor);
        painter->drawText(QRect(rect.left() + usedRemarkWidth + gap,
                                rect.top(),
                                qMax(0, rect.width() - usedRemarkWidth - gap),
                                rect.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          nameMetrics().elidedText(suffix,
                                                   Qt::ElideRight,
                                                   qMax(0, rect.width() - usedRemarkWidth - gap)));
    }
}

const QFont& subtitleFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(12);
        return value;
    }();
    return font;
}

const QFontMetrics& subtitleMetrics()
{
    static const QFontMetrics metrics(subtitleFont());
    return metrics;
}

const QFont& groupFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(12);
        value.setWeight(QFont::Medium);
        return value;
    }();
    return font;
}

const QFontMetrics& groupMetrics()
{
    static const QFontMetrics metrics(groupFont());
    return metrics;
}

const QFont& groupCountFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(11);
        value.setWeight(QFont::Medium);
        return value;
    }();
    return font;
}

const QFontMetrics& groupCountMetrics()
{
    static const QFontMetrics metrics(groupCountFont());
    return metrics;
}

} // namespace

FriendListDelegate::FriendListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void FriendListDelegate::paint(QPainter* painter,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    painter->save();
    painter->setClipRect(option.rect);

    const bool isNotice = index.data(FriendListModel::IsNoticeRole).toBool();
    if (isNotice) {
        const bool hovered = option.state & QStyle::State_MouseOver;
        if (hovered) {
            const QRect hoverRect = option.rect.adjusted(6, 3, -6, -3);
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(QColor(0xe9, 0xe9, 0xe9));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(hoverRect, 6, 6);
        }

        painter->setFont(groupFont());
        painter->setPen(QColor(0x11, 0x11, 0x11));
        painter->drawText(option.rect.adjusted(20, 0, -28, 0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          groupMetrics().elidedText(index.data(FriendListModel::DisplayNameRole).toString(),
                                                     Qt::ElideRight,
                                                     option.rect.width() - 48));

        const int centerX = option.rect.right() - kGroupCountRightPadding;
        const int centerY = option.rect.center().y() + kNoticeArrowYOffset;
        QPen arrowPen(QColor(0x66, 0x66, 0x66), 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(arrowPen);
        painter->drawLine(QPointF(centerX - 2.0, centerY - 4.0), QPointF(centerX + 2.0, centerY));
        painter->drawLine(QPointF(centerX + 2.0, centerY), QPointF(centerX - 2.0, centerY + 4.0));
        painter->restore();
        return;
    }

    const bool isGroup = index.data(FriendListModel::IsGroupRole).toBool();
    if (isGroup) {
        const bool hovered = option.state & QStyle::State_MouseOver;
        if (hovered) {
            const QRect hoverRect = option.rect.adjusted(6, 3, -6, -3);
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(QColor(0xee, 0xee, 0xee));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(hoverRect, 6, 6);
        }

        const qreal progress = index.data(FriendListModel::GroupProgressRole).toReal();
        const QRect arrowRect(option.rect.left() + kLeftPadding,
                              option.rect.top() + (option.rect.height() - kGroupArrowSize) / 2
                                      + kContactGroupArrowYOffset,
                              kGroupArrowSize,
                              kGroupArrowSize);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->translate(arrowRect.center());
        painter->rotate(progress * 90.0);
        QPen arrowPen(QColor(0x78, 0x86, 0x94), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(arrowPen);
        painter->drawLine(QPointF(-2.5, -4.0), QPointF(2.5, 0.0));
        painter->drawLine(QPointF(2.5, 0.0), QPointF(-2.5, 4.0));
        painter->restore();

        const QString title = index.data(FriendListModel::GroupNameRole).toString();
        const QString countText = QString::number(index.data(FriendListModel::GroupFriendCountRole).toInt());
        const int titleLeft = arrowRect.right() + 8;
        const int countWidth = groupCountMetrics().horizontalAdvance(countText);
        const int rightEdge = option.rect.right() - kGroupCountRightPadding;
        const QRect countRect(qMax(titleLeft, rightEdge - countWidth + 1),
                              option.rect.top(),
                              countWidth,
                              option.rect.height());
        const QRect titleRect(titleLeft,
                              option.rect.top(),
                              qMax(0, countRect.left() - titleLeft - 8),
                              option.rect.height());

        painter->setFont(groupFont());
        painter->setPen(QColor(0x38, 0x45, 0x52));
        painter->drawText(titleRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          groupMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));
        painter->setFont(groupCountFont());
        painter->setPen(QColor(0x99, 0xa3, 0xad));
        painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, countText);
        painter->restore();
        return;
    }

    const qreal progress = index.data(FriendListModel::GroupProgressRole).toReal();
    if (option.rect.height() <= 0 || progress <= 0.01) {
        painter->restore();
        return;
    }

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const QColor backgroundColor = selected
            ? QColor(0x00, 0x99, 0xff)
            : (hovered ? QColor(0xf0, 0xf0, 0xf0) : QColor(0xff, 0xff, 0xff));
    painter->fillRect(option.rect, backgroundColor);
    painter->setOpacity(qMin(1.0, progress * 1.25));

    const int contentTop = option.rect.top() - qRound((1.0 - progress) * 10.0);

    const QRect avatarRect(option.rect.left() + kLeftPadding,
                           contentTop + (kItemHeight - kAvatarSize) / 2,
                           kAvatarSize,
                           kAvatarSize);
    const qreal devicePixelRatio = painter->device()->devicePixelRatioF();
    const QString avatarPath = index.data(FriendListModel::AvatarPathRole).toString();
    QPixmap avatar = ImageService::instance().circularAvatar(avatarPath,
                                                             kAvatarSize,
                                                             devicePixelRatio);
    const UserStatus status = static_cast<UserStatus>(index.data(FriendListModel::StatusRole).toInt());
    painter->save();
    if (status == Offline) {
        painter->setOpacity(painter->opacity() * 0.42);
    }
    painter->drawPixmap(avatarRect, avatar);
    painter->restore();

    const int contentLeft = avatarRect.right() + kContentSpacing + 1;
    const int rightEdge = option.rect.right() - kRightPadding;
    const int itemCenterY = contentTop + kItemHeight / 2;

    const int nameHeight = nameMetrics().height();
    const int nameY = itemCenterY - kLineSpacing / 2 - nameHeight;
    const QRect nameRect(contentLeft,
                         nameY,
                         qMax(0, rightEdge - contentLeft),
                         nameHeight);

    drawFriendDisplayName(painter, nameRect, index, selected);

    const QString subtitle = QString("[%1] %2").arg(statusText(status),
                                                    index.data(FriendListModel::SignatureRole).toString());

    const int subtitleHeight = subtitleMetrics().height();
    const int subtitleY = itemCenterY + kLineSpacing / 2 + 1;

    const QRect iconRect(contentLeft,
                         subtitleY + 1,
                         kStatusIconSize + 2,
                         kStatusIconSize + 2);
    const QPixmap statusIcon = ImageService::instance().scaled(statusIconPath(status),
                                                               QSize(kStatusIconSize, kStatusIconSize),
                                                               Qt::KeepAspectRatio,
                                                               devicePixelRatio);
    painter->drawPixmap(iconRect, statusIcon);

    const int subtitleLeft = iconRect.right() + kStatusIconTextGap;
    const QRect subtitleRect(subtitleLeft,
                             subtitleY,
                             qMax(0, rightEdge - subtitleLeft),
                             subtitleHeight);

    painter->setFont(subtitleFont());
    painter->setPen(selected ? Qt::white : Qt::gray);
    painter->drawText(subtitleRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      subtitleMetrics().elidedText(subtitle,
                                                   Qt::ElideRight,
                                                   subtitleRect.width()));

    painter->restore();
}

QSize FriendListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    Q_UNUSED(option);
    const QSize modelHint = index.data(Qt::SizeHintRole).toSize();
    if (modelHint.isValid()) {
        return modelHint;
    }
    return index.data(FriendListModel::IsGroupRole).toBool()
            || index.data(FriendListModel::IsNoticeRole).toBool()
            ? QSize(0, kGroupHeaderHeight)
            : QSize(0, kItemHeight);
}
