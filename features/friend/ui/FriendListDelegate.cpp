#include "FriendListDelegate.h"

#include <QApplication>
#include <QPainter>

#include "shared/services/ImageService.h"
#include "shared/ui/BadgeRenderer.h"
#include "features/friend/model/FriendListModel.h"
#include "shared/types/User.h"
#include "shared/theme/ThemeManager.h"

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
                           const QString& remark,
                           const QString& nickName,
                           const QString& displayName,
                           const QColor& primaryText,
                           const QColor& tertiaryText,
                           bool selected)
{
    const QColor primaryColor = selected ? ThemeManager::instance().color(ThemeColor::TextOnAccent) : primaryText;
    const QColor secondaryColor = selected ? ThemeManager::instance().color(ThemeColor::SecondaryTextOnAccent)
                                           : tertiaryText;

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

QString cachedStatusText(UserStatus userStatus)
{
    static const QString online = QStringLiteral("在线");
    static const QString offline = QStringLiteral("离线");
    static const QString flying = QStringLiteral("飞行模式");
    static const QString mining = QStringLiteral("挖矿中");

    if (userStatus == Online) {
        return online;
    }
    if (userStatus == Offline) {
        return offline;
    }
    if (userStatus == Flying) {
        return flying;
    }
    return mining;
}

QString cachedStatusIconPath(UserStatus userStatus)
{
    static const QString online = QStringLiteral(":/resources/icon/online.png");
    static const QString offline = QStringLiteral(":/resources/icon/offline.png");
    static const QString flying = QStringLiteral(":/resources/icon/flying.png");
    static const QString mining = QStringLiteral(":/resources/icon/mining.png");

    if (userStatus == Online) {
        return online;
    }
    if (userStatus == Offline) {
        return offline;
    }
    if (userStatus == Flying) {
        return flying;
    }
    return mining;
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
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        m_themePaintCache.valid = false;
    });
}

void FriendListDelegate::paint(QPainter* painter,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setClipRect(option.rect);

    const ThemePaintCache& theme = themePaintCache();
    const bool isNotice = index.data(FriendListModel::IsNoticeRole).toBool();
    if (isNotice) {
        const bool noticeSelected = index.data(FriendListModel::NoticeSelectedRole).toBool();

        // Background: panel by default
        painter->fillRect(option.rect, theme.panelBackground);

        // Selected or hovered overlay: rounded rect (6px radius, margins 6/3/6/3)
        const bool hovered = option.state & QStyle::State_MouseOver;
        const QRect overlayRect = option.rect.adjusted(6, 3, -6, -3);

        if (noticeSelected) {
            // Darker than hover: darken ListHover by a fixed delta
            const QColor hoverColor = theme.listHover;
            const int delta = theme.dark ? 8 : 12;
            QColor selColor(qMax(0, hoverColor.red() - delta),
                            qMax(0, hoverColor.green() - delta),
                            qMax(0, hoverColor.blue() - delta));
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setRenderHint(QPainter::TextAntialiasing, true);
            painter->setBrush(selColor);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(overlayRect, 6, 6);
        } else if (hovered) {
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setRenderHint(QPainter::TextAntialiasing, true);
            painter->setBrush(theme.listHover);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(overlayRect, 6, 6);
        }

        // Unread badge (left of arrow)
        const int unreadCount = index.data(FriendListModel::NoticeUnreadCountRole).toInt();
        const BadgeLayout badgeLayout = BadgeRenderer::layoutForUnreadCount(
            unreadCount, false, noticeSelected, theme.dark);

        const int arrowCenterX = option.rect.right() - kGroupCountRightPadding;
        const int arrowCenterY = option.rect.center().y() + kNoticeArrowYOffset;

        // Reserve space for badge + gap + arrow
        int rightLimit = arrowCenterX - 12; // gap before arrow
        if (badgeLayout.size.isValid()) {
            const int badgeX = rightLimit - badgeLayout.size.width();
            const int badgeY = arrowCenterY - badgeLayout.size.height() / 2;
            const QRect badgeR(badgeX, badgeY,
                               badgeLayout.size.width(), badgeLayout.size.height());
            BadgeRenderer::drawBadge(painter, badgeR, badgeLayout, noticeSelected);
            rightLimit = badgeX - 6;
        }

        painter->setFont(groupFont());
        painter->setPen(theme.primaryText);
        const QRect textRect(20, option.rect.top(),
                             qMax(0, rightLimit - 20), option.rect.height());
        painter->drawText(textRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          groupMetrics().elidedText(index.data(FriendListModel::DisplayNameRole).toString(),
                                                     Qt::ElideRight,
                                                     textRect.width()));

        QPen arrowPen(theme.secondaryText, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(arrowPen);
        painter->drawLine(QPointF(arrowCenterX - 2.0, arrowCenterY - 4.0),
                          QPointF(arrowCenterX + 2.0, arrowCenterY));
        painter->drawLine(QPointF(arrowCenterX + 2.0, arrowCenterY),
                          QPointF(arrowCenterX - 2.0, arrowCenterY + 4.0));
        painter->restore();
        return;
    }

    const bool isGroup = index.data(FriendListModel::IsGroupRole).toBool();
    if (isGroup) {
        painter->fillRect(option.rect, theme.panelBackground);

        const bool hovered = option.state & QStyle::State_MouseOver;
        if (hovered) {
            const QRect hoverRect = option.rect.adjusted(6, 3, -6, -3);
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setRenderHint(QPainter::TextAntialiasing, true);
            painter->setBrush(theme.listHover);
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
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->translate(arrowRect.center());
        painter->rotate(progress * 90.0);
        QPen arrowPen(theme.tertiaryText,
                      1.5,
                      Qt::SolidLine,
                      Qt::RoundCap,
                      Qt::RoundJoin);
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
        painter->setPen(theme.secondaryText);
        painter->drawText(titleRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          groupMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));
        painter->setFont(groupCountFont());
        painter->setPen(theme.tertiaryText);
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
    const bool hovered = (option.state & QStyle::State_MouseOver) ||
            index.data(FriendListModel::ContextMenuActiveRole).toBool();
    const QColor backgroundColor = selected
            ? theme.listSelected
            : (hovered ? theme.listHover
                       : theme.panelBackground);
    painter->fillRect(option.rect, backgroundColor);
    painter->setOpacity(qMin(1.0, progress * 1.25));

    const int contentTop = option.rect.top() - qRound((1.0 - progress) * 10.0);

    const QRect avatarRect(option.rect.left() + kLeftPadding,
                           contentTop + (kItemHeight - kAvatarSize) / 2,
                           kAvatarSize,
                           kAvatarSize);
    const qreal devicePixelRatio = painter->device()->devicePixelRatioF();
    const FriendPaintCache friendData = friendPaintCache(index);
    const UserStatus status = static_cast<UserStatus>(friendData.status);
    QPixmap avatar = ImageService::instance().circularAvatarPreview(friendData.avatarPath,
                                                                    kAvatarSize,
                                                                    devicePixelRatio);
    if (selected && status == Offline) {
        painter->save();
        painter->setOpacity(1.0);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(theme.panelBackground);
        painter->drawEllipse(avatarRect);
        painter->restore();
    }

    painter->save();
    if (status == Offline) {
        painter->setOpacity(painter->opacity() * 0.42);
    }
    if (avatar.isNull()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(theme.imagePlaceholder);
        painter->drawEllipse(avatarRect);
    } else {
        painter->drawPixmap(avatarRect, avatar);
    }
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

    drawFriendDisplayName(painter,
                          nameRect,
                          friendData.remark,
                          friendData.nickName,
                          friendData.displayName,
                          theme.primaryText,
                          theme.tertiaryText,
                          selected);

    const int subtitleHeight = subtitleMetrics().height();
    const int subtitleY = itemCenterY + kLineSpacing / 2 + 1;

    const QRect iconRect(contentLeft,
                         subtitleY + 1,
                         kStatusIconSize + 2,
                         kStatusIconSize + 2);
    const QPixmap statusIcon = ImageService::instance().scaled(friendData.statusIconPath,
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
    painter->setPen(selected ? ThemeManager::instance().color(ThemeColor::TextOnAccent) : theme.tertiaryText);
    painter->drawText(subtitleRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      subtitleMetrics().elidedText(friendData.subtitle,
                                                   Qt::ElideRight,
                                                   subtitleRect.width()));

    painter->restore();
}

void FriendListDelegate::clearPaintCache()
{
    m_friendPaintCache.clear();
    m_themePaintCache.valid = false;
}

void FriendListDelegate::invalidatePaintCache(const QModelIndex& topLeft,
                                              const QModelIndex& bottomRight,
                                              const QVector<int>& roles)
{
    if (!topLeft.isValid() || !bottomRight.isValid() || topLeft.model() != bottomRight.model()) {
        return;
    }

    if (!roles.isEmpty()) {
        bool affectsFriendPaint = false;
        for (const int role : roles) {
            switch (role) {
            case Qt::DisplayRole:
            case FriendListModel::DisplayNameRole:
            case FriendListModel::AvatarPathRole:
            case FriendListModel::StatusRole:
            case FriendListModel::SignatureRole:
            case FriendListModel::NickNameRole:
            case FriendListModel::RemarkRole:
                affectsFriendPaint = true;
                break;
            default:
                break;
            }
            if (affectsFriendPaint) {
                break;
            }
        }
        if (!affectsFriendPaint) {
            return;
        }
    }

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        const QModelIndex index = topLeft.sibling(row, topLeft.column());
        const QString userId = index.data(FriendListModel::UserIdRole).toString();
        if (!userId.isEmpty()) {
            m_friendPaintCache.remove(userId);
        }
    }
}

const FriendListDelegate::ThemePaintCache& FriendListDelegate::themePaintCache() const
{
    const bool dark = ThemeManager::instance().isDark();
    if (m_themePaintCache.valid && m_themePaintCache.dark == dark) {
        return m_themePaintCache;
    }

    m_themePaintCache.valid = true;
    m_themePaintCache.dark = dark;
    m_themePaintCache.panelBackground = ThemeManager::instance().color(ThemeColor::PanelBackground);
    m_themePaintCache.listHover = ThemeManager::instance().color(ThemeColor::ListHover);
    m_themePaintCache.listSelected = ThemeManager::instance().color(ThemeColor::ListSelected);
    m_themePaintCache.primaryText = ThemeManager::instance().color(ThemeColor::PrimaryText);
    m_themePaintCache.secondaryText = ThemeManager::instance().color(ThemeColor::SecondaryText);
    m_themePaintCache.tertiaryText = ThemeManager::instance().color(ThemeColor::TertiaryText);
    m_themePaintCache.imagePlaceholder = ThemeManager::instance().color(ThemeColor::ImagePlaceholder);
    return m_themePaintCache;
}

FriendListDelegate::FriendPaintCache FriendListDelegate::friendPaintCache(const QModelIndex& index) const
{
    const QString userId = index.data(FriendListModel::UserIdRole).toString();
    if (!userId.isEmpty()) {
        const auto cached = m_friendPaintCache.constFind(userId);
        if (cached != m_friendPaintCache.cend()) {
            return cached.value();
        }
    }

    FriendPaintCache data;
    data.userId = userId;
    data.displayName = index.data(FriendListModel::DisplayNameRole).toString();
    data.avatarPath = index.data(FriendListModel::AvatarPathRole).toString();
    data.status = index.data(FriendListModel::StatusRole).toInt();
    const QString signature = index.data(FriendListModel::SignatureRole).toString();
    data.nickName = index.data(FriendListModel::NickNameRole).toString();
    data.remark = index.data(FriendListModel::RemarkRole).toString();

    const UserStatus status = static_cast<UserStatus>(data.status);
    data.subtitle = QStringLiteral("[%1] %2").arg(cachedStatusText(status), signature);
    data.statusIconPath = cachedStatusIconPath(status);

    if (!data.userId.isEmpty()) {
        m_friendPaintCache.insert(data.userId, data);
    }
    return data;
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
