#include "GroupListDelegate.h"
#include "shared/services/AppFonts.h"

#include <QApplication>
#include <QPainter>

#include "features/friend/model/GroupListModel.h"
#include "shared/services/ImageService.h"
#include "shared/ui/renderers/ContactListDelegateRenderer.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"
#include "shared/theme/ThemeManager.h"

extern const int kContactGroupArrowYOffset;

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

void drawGroupDisplayName(QPainter* painter,
                          const QRect& rect,
                          const QModelIndex& index,
                          const QColor& selectedText,
                          const QColor& selectedSecondaryText,
                          bool selected)
{
    const QString remark = index.data(GroupListModel::RemarkRole).toString();
    const QString groupName = index.data(GroupListModel::GroupNameRole).toString();
    const QString displayName = index.data(GroupListModel::DisplayNameRole).toString();
    const QColor primaryColor = selected ? selectedText : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor secondaryColor = selected ? selectedSecondaryText
                                           : ThemeManager::instance().color(ThemeColor::TertiaryText);

    painter->setFont(nameFont());
    if (remark.isEmpty() || groupName.isEmpty()) {
        painter->setPen(primaryColor);
        painter->drawText(rect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          nameMetrics().elidedText(displayName, Qt::ElideRight, rect.width()));
        return;
    }

    const QString suffix = QStringLiteral("(%1)").arg(groupName);
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

QFont memberFont()
{
    return AppFonts::applicationPixelSizedFont(12);
}

QFontMetrics memberMetrics()
{
    return AppFonts::applicationPixelSizedMetrics(12);
}

QFont categoryFont()
{
    return AppFonts::applicationPixelWeightedFont(12, QFont::Medium);
}

QFontMetrics categoryMetrics()
{
    return AppFonts::applicationPixelWeightedMetrics(12, QFont::Medium);
}

QFont categoryCountFont()
{
    return AppFonts::applicationPixelWeightedFont(11, QFont::Medium);
}

QFontMetrics categoryCountMetrics()
{
    return AppFonts::applicationPixelWeightedMetrics(11, QFont::Medium);
}

} // namespace

GroupListDelegate::GroupListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void GroupListDelegate::paint(QPainter* painter,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    painter->save();
    AppFonts::configurePainterForText(*painter);
    painter->setClipRect(option.rect);

    const bool isNotice = index.data(GroupListModel::IsNoticeRole).toBool();
    if (isNotice) {
        const bool noticeSelected = index.data(GroupListModel::NoticeSelectedRole).toBool();
        const bool hovered = option.state & QStyle::State_MouseOver;
        ContactListDelegateRenderer::drawNoticeRow(
            painter,
            option.rect,
            index.data(GroupListModel::DisplayNameRole).toString(),
            index.data(GroupListModel::NoticeUnreadCountRole).toInt(),
            noticeSelected,
            hovered,
            kCategoryCountRightPadding,
            kNoticeArrowYOffset,
            categoryFont(),
            categoryMetrics(),
            ContactListDelegateRenderer::currentPaintColors());
        painter->restore();
        return;
    }

    const bool isCategory = index.data(GroupListModel::IsCategoryRole).toBool();
    if (isCategory) {
        const bool hovered = option.state & QStyle::State_MouseOver;
        ContactListDelegateRenderer::drawSectionHeaderRow(
            painter,
            option.rect,
            index.data(GroupListModel::CategoryNameRole).toString(),
            QString::number(index.data(GroupListModel::CategoryGroupCountRole).toInt()),
            index.data(GroupListModel::CategoryProgressRole).toReal(),
            hovered,
            kLeftPadding,
            kCategoryCountRightPadding,
            kCategoryArrowSize,
            kContactGroupArrowYOffset,
            categoryFont(),
            categoryMetrics(),
            categoryCountFont(),
            categoryCountMetrics(),
            ContactListDelegateRenderer::currentPaintColors());
        painter->restore();
        return;
    }

    const qreal progress = index.data(GroupListModel::CategoryProgressRole).toReal();
    if (option.rect.height() <= 0 || progress <= 0.01) {
        painter->restore();
        return;
    }

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = ((option.state & QStyle::State_MouseOver) &&
                          !index.data(GroupListModel::HoverSuppressedRole).toBool());
    const QColor backgroundColor = selected
            ? ThemeManager::instance().color(ThemeColor::ListSelected)
            : (hovered ? ThemeManager::instance().color(ThemeColor::ListHover)
                       : ThemeManager::instance().color(ThemeColor::PanelBackground));
    painter->fillRect(option.rect, backgroundColor);
    const QColor selectedTextColor = ThemeManager::textColorOn(backgroundColor);
    const QColor selectedSecondaryTextColor = ThemeManager::textColorOn(backgroundColor, 165);
    painter->setOpacity(qMin(1.0, progress * 1.25));

    const int contentTop = option.rect.top() - qRound((1.0 - progress) * 10.0);

    const QRect avatarRect(option.rect.left() + kLeftPadding,
                           contentTop + (kItemHeight - kAvatarSize) / 2,
                           kAvatarSize,
                           kAvatarSize);
    const qreal devicePixelRatio = painter->device()->devicePixelRatioF();
    const QString avatarPath = index.data(GroupListModel::AvatarPathRole).toString();
    const QPixmap avatar = ImageService::instance().circularAvatarPreview(avatarPath,
                                                                          kAvatarSize,
                                                                          devicePixelRatio);
    if (avatar.isNull()) {
        MediaPlaceholderRenderer::drawAvatar(painter, avatarRect);
    } else {
        painter->drawPixmap(avatarRect, avatar);
    }

    const int contentLeft = avatarRect.right() + kContentSpacing + 1;
    const int rightEdge = option.rect.right() - kRightPadding;
    const int itemCenterY = contentTop + kItemHeight / 2;

    const int nameHeight = nameMetrics().height();
    const int nameY = itemCenterY - kLineSpacing / 2 - nameHeight;
    const QRect nameRect(contentLeft,
                         nameY,
                         qMax(0, rightEdge - contentLeft),
                         nameHeight);

    drawGroupDisplayName(painter, nameRect, index, selectedTextColor, selectedSecondaryTextColor, selected);

    const QString memberText = QStringLiteral("%1人").arg(index.data(GroupListModel::MemberCountRole).toInt());
    const int memberHeight = memberMetrics().height();
    const int memberY = itemCenterY + kLineSpacing / 2 + 1;

    const QRect iconRect(contentLeft,
                         memberY + 1,
                         kMemberIconSize + 2,
                         kMemberIconSize + 2);
    const QPixmap memberIcon = ImageService::instance().scaled(QStringLiteral(":/resources/icon/friend_selected.png"),
                                                               QSize(kMemberIconSize, kMemberIconSize),
                                                               Qt::KeepAspectRatio,
                                                               devicePixelRatio);
    painter->drawPixmap(iconRect, memberIcon);

    const int memberLeft = iconRect.right() + kMemberIconTextGap;
    const QRect memberRect(memberLeft,
                           memberY,
                           qMax(0, rightEdge - memberLeft),
                           memberHeight);

    painter->setFont(memberFont());
    painter->setPen(selected ? selectedTextColor : ThemeManager::instance().color(ThemeColor::TertiaryText));
    painter->drawText(memberRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      memberMetrics().elidedText(memberText,
                                                 Qt::ElideRight,
                                                 memberRect.width()));

    painter->restore();
}

QSize GroupListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    Q_UNUSED(option);
    const QSize modelHint = index.data(Qt::SizeHintRole).toSize();
    if (modelHint.isValid()) {
        return modelHint;
    }
    return index.data(GroupListModel::IsCategoryRole).toBool()
            || index.data(GroupListModel::IsNoticeRole).toBool()
            ? QSize(0, kCategoryHeaderHeight)
            : QSize(0, kItemHeight);
}
