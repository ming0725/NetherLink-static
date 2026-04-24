#include "FriendListDelegate.h"

#include <QApplication>
#include <QPainter>

#include "shared/services/ImageService.h"
#include "features/friend/model/FriendListModel.h"
#include "shared/types/User.h"

namespace {

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

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const QColor backgroundColor = selected
            ? QColor(0x00, 0x99, 0xff)
            : (hovered ? QColor(0xf0, 0xf0, 0xf0) : QColor(0xff, 0xff, 0xff));
    painter->fillRect(option.rect, backgroundColor);

    const QRect avatarRect(option.rect.left() + kLeftPadding,
                           option.rect.top() + (kItemHeight - kAvatarSize) / 2,
                           kAvatarSize,
                           kAvatarSize);
    const qreal devicePixelRatio = painter->device()->devicePixelRatioF();
    const QString avatarPath = index.data(FriendListModel::AvatarPathRole).toString();
    const QPixmap avatar = ImageService::instance().circularAvatar(avatarPath,
                                                                   kAvatarSize,
                                                                   devicePixelRatio);
    painter->drawPixmap(avatarRect, avatar);

    const int contentLeft = avatarRect.right() + kContentSpacing + 1;
    const int rightEdge = option.rect.right() - kRightPadding;
    const int itemCenterY = option.rect.center().y();

    const int nameHeight = nameMetrics().height();
    const int nameY = itemCenterY - kLineSpacing / 2 - nameHeight;
    const QRect nameRect(contentLeft,
                         nameY,
                         qMax(0, rightEdge - contentLeft),
                         nameHeight);

    painter->setFont(nameFont());
    painter->setPen(selected ? Qt::white : Qt::black);
    painter->drawText(nameRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics().elidedText(index.data(FriendListModel::DisplayNameRole).toString(),
                                               Qt::ElideRight,
                                               nameRect.width()));

    const UserStatus status = static_cast<UserStatus>(index.data(FriendListModel::StatusRole).toInt());
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
    Q_UNUSED(index);
    return QSize(0, kItemHeight);
}
