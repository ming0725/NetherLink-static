#include "FriendListDelegate.h"
#include "shared/services/AppFonts.h"

#include <QApplication>
#include <QPainter>

#include "shared/services/ImageService.h"
#include "shared/ui/renderers/ContactListDelegateRenderer.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"
#include "features/friend/model/FriendListModel.h"
#include "shared/types/User.h"
#include "shared/theme/ThemeManager.h"

extern const int kContactGroupArrowYOffset = 1;

namespace {

const int kNoticeArrowYOffset = 2;

QFont nameFont()
{
    return AppFonts::applicationPixelSizedFont(14);
}

QFontMetrics nameMetrics()
{
    return AppFonts::applicationPixelSizedMetrics(14);
}

void drawFriendDisplayName(QPainter* painter,
                           const QRect& rect,
                           const QString& remark,
                           const QString& nickName,
                           const QString& displayName,
                           const QColor& primaryText,
                           const QColor& tertiaryText,
                           const QColor& selectedText,
                           const QColor& selectedSecondaryText,
                           bool selected)
{
    const QColor primaryColor = selected ? selectedText : primaryText;
    const QColor secondaryColor = selected ? selectedSecondaryText : tertiaryText;

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

QFont subtitleFont()
{
    return AppFonts::applicationPixelSizedFont(12);
}

QFontMetrics subtitleMetrics()
{
    return AppFonts::applicationPixelSizedMetrics(12);
}

QFont groupFont()
{
    return AppFonts::applicationPixelWeightedFont(12, QFont::Medium);
}

QFontMetrics groupMetrics()
{
    return AppFonts::applicationPixelWeightedMetrics(12, QFont::Medium);
}

QFont groupCountFont()
{
    return AppFonts::applicationPixelWeightedFont(11, QFont::Medium);
}

QFontMetrics groupCountMetrics()
{
    return AppFonts::applicationPixelWeightedMetrics(11, QFont::Medium);
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
    AppFonts::configurePainterForText(*painter);
    painter->setClipRect(option.rect);

    const ThemePaintCache& theme = themePaintCache();
    const bool isNotice = index.data(FriendListModel::IsNoticeRole).toBool();
    if (isNotice) {
        const bool noticeSelected = index.data(FriendListModel::NoticeSelectedRole).toBool();
        const bool hovered = option.state & QStyle::State_MouseOver;
        ContactListDelegateRenderer::drawNoticeRow(
            painter,
            option.rect,
            index.data(FriendListModel::DisplayNameRole).toString(),
            index.data(FriendListModel::NoticeUnreadCountRole).toInt(),
            noticeSelected,
            hovered,
            kGroupCountRightPadding,
            kNoticeArrowYOffset,
            groupFont(),
            groupMetrics(),
            ContactListDelegateRenderer::currentPaintColors());
        painter->restore();
        return;
    }

    const bool isGroup = index.data(FriendListModel::IsGroupRole).toBool();
    if (isGroup) {
        const bool hovered = option.state & QStyle::State_MouseOver;
        ContactListDelegateRenderer::drawSectionHeaderRow(
            painter,
            option.rect,
            index.data(FriendListModel::GroupNameRole).toString(),
            QString::number(index.data(FriendListModel::GroupFriendCountRole).toInt()),
            index.data(FriendListModel::GroupProgressRole).toReal(),
            hovered,
            kLeftPadding,
            kGroupCountRightPadding,
            kGroupArrowSize,
            kContactGroupArrowYOffset,
            groupFont(),
            groupMetrics(),
            groupCountFont(),
            groupCountMetrics(),
            ContactListDelegateRenderer::currentPaintColors());
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
        MediaPlaceholderRenderer::drawAvatar(painter, avatarRect);
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
                          theme.selectedText,
                          theme.selectedSecondaryText,
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
    painter->setPen(selected ? theme.selectedText : theme.tertiaryText);
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
    m_themePaintCache.selectedText = ThemeManager::textColorOn(m_themePaintCache.listSelected);
    m_themePaintCache.selectedSecondaryText = ThemeManager::textColorOn(m_themePaintCache.listSelected, 165);
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
