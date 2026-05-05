#include "GroupListDelegate.h"

#include <QApplication>
#include <QPainter>

#include "features/friend/model/GroupListModel.h"
#include "shared/services/ImageService.h"
#include "shared/ui/BadgeRenderer.h"
#include "shared/theme/ThemeManager.h"

extern const int kContactGroupArrowYOffset;

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

void drawGroupDisplayName(QPainter* painter,
                          const QRect& rect,
                          const QModelIndex& index,
                          bool selected)
{
    const QString remark = index.data(GroupListModel::RemarkRole).toString();
    const QString groupName = index.data(GroupListModel::GroupNameRole).toString();
    const QString displayName = index.data(GroupListModel::DisplayNameRole).toString();
    const QColor primaryColor = selected ? Qt::white : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor secondaryColor = selected ? QColor(0xff, 0xff, 0xff, 165)
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

const QFont& memberFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(12);
        return value;
    }();
    return font;
}

const QFontMetrics& memberMetrics()
{
    static const QFontMetrics metrics(memberFont());
    return metrics;
}

const QFont& categoryFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(12);
        value.setWeight(QFont::Medium);
        return value;
    }();
    return font;
}

const QFontMetrics& categoryMetrics()
{
    static const QFontMetrics metrics(categoryFont());
    return metrics;
}

const QFont& categoryCountFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(11);
        value.setWeight(QFont::Medium);
        return value;
    }();
    return font;
}

const QFontMetrics& categoryCountMetrics()
{
    static const QFontMetrics metrics(categoryCountFont());
    return metrics;
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
    painter->setClipRect(option.rect);

    const bool isNotice = index.data(GroupListModel::IsNoticeRole).toBool();
    if (isNotice) {
        const bool noticeSelected = index.data(GroupListModel::NoticeSelectedRole).toBool();
        painter->fillRect(option.rect, ThemeManager::instance().color(ThemeColor::PanelBackground));

        const bool hovered = option.state & QStyle::State_MouseOver;
        const QRect overlayRect = option.rect.adjusted(6, 3, -6, -3);
        if (noticeSelected) {
            QColor hoverColor = ThemeManager::instance().color(ThemeColor::ListHover);
            const int delta = ThemeManager::instance().isDark() ? 8 : 12;
            QColor selColor(qMax(0, hoverColor.red() - delta),
                            qMax(0, hoverColor.green() - delta),
                            qMax(0, hoverColor.blue() - delta));
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(selColor);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(overlayRect, 6, 6);
        } else if (hovered) {
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(ThemeManager::instance().color(ThemeColor::ListHover));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(overlayRect, 6, 6);
        }

        const int centerX = option.rect.right() - kCategoryCountRightPadding;
        const int centerY = option.rect.center().y() + kNoticeArrowYOffset;
        const int unreadCount = index.data(GroupListModel::NoticeUnreadCountRole).toInt();
        const BadgeLayout badgeLayout = BadgeRenderer::layoutForUnreadCount(
            unreadCount, false, noticeSelected, ThemeManager::instance().isDark());
        int rightLimit = centerX - 12;
        if (badgeLayout.size.isValid()) {
            const int badgeX = rightLimit - badgeLayout.size.width();
            const QRect badgeRect(badgeX,
                                  centerY - badgeLayout.size.height() / 2,
                                  badgeLayout.size.width(),
                                  badgeLayout.size.height());
            BadgeRenderer::drawBadge(painter, badgeRect, badgeLayout, noticeSelected);
            rightLimit = badgeX - 6;
        }

        painter->setFont(categoryFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
        const QRect textRect(20, option.rect.top(),
                             qMax(0, rightLimit - 20), option.rect.height());
        painter->drawText(textRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          categoryMetrics().elidedText(index.data(GroupListModel::DisplayNameRole).toString(),
                                                       Qt::ElideRight,
                                                       textRect.width()));

        QPen arrowPen(ThemeManager::instance().color(ThemeColor::SecondaryText),
                      1.3,
                      Qt::SolidLine,
                      Qt::RoundCap,
                      Qt::RoundJoin);
        painter->setPen(arrowPen);
        painter->drawLine(QPointF(centerX - 2.0, centerY - 4.0), QPointF(centerX + 2.0, centerY));
        painter->drawLine(QPointF(centerX + 2.0, centerY), QPointF(centerX - 2.0, centerY + 4.0));
        painter->restore();
        return;
    }

    const bool isCategory = index.data(GroupListModel::IsCategoryRole).toBool();
    if (isCategory) {
        painter->fillRect(option.rect, ThemeManager::instance().color(ThemeColor::PanelBackground));

        const bool hovered = option.state & QStyle::State_MouseOver;
        if (hovered) {
            const QRect hoverRect = option.rect.adjusted(6, 3, -6, -3);
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(ThemeManager::instance().color(ThemeColor::ListHover));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(hoverRect, 6, 6);
        }

        const qreal progress = index.data(GroupListModel::CategoryProgressRole).toReal();
        const QRect arrowRect(option.rect.left() + kLeftPadding,
                              option.rect.top() + (option.rect.height() - kCategoryArrowSize) / 2
                                      + kContactGroupArrowYOffset,
                              kCategoryArrowSize,
                              kCategoryArrowSize);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->translate(arrowRect.center());
        painter->rotate(progress * 90.0);
        QPen arrowPen(ThemeManager::instance().color(ThemeColor::TertiaryText),
                      1.5,
                      Qt::SolidLine,
                      Qt::RoundCap,
                      Qt::RoundJoin);
        painter->setPen(arrowPen);
        painter->drawLine(QPointF(-2.5, -4.0), QPointF(2.5, 0.0));
        painter->drawLine(QPointF(2.5, 0.0), QPointF(-2.5, 4.0));
        painter->restore();

        const QString title = index.data(GroupListModel::CategoryNameRole).toString();
        const QString countText = QString::number(index.data(GroupListModel::CategoryGroupCountRole).toInt());
        const int titleLeft = arrowRect.right() + 8;
        const int countWidth = categoryCountMetrics().horizontalAdvance(countText);
        const int rightEdge = option.rect.right() - kCategoryCountRightPadding;
        const QRect countRect(qMax(titleLeft, rightEdge - countWidth + 1),
                              option.rect.top(),
                              countWidth,
                              option.rect.height());
        const QRect titleRect(titleLeft,
                              option.rect.top(),
                              qMax(0, countRect.left() - titleLeft - 8),
                              option.rect.height());

        painter->setFont(categoryFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(titleRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          categoryMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));
        painter->setFont(categoryCountFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
        painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, countText);
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
        painter->setPen(Qt::NoPen);
        painter->setBrush(ThemeManager::instance().color(ThemeColor::ImagePlaceholder));
        painter->drawEllipse(avatarRect);
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

    drawGroupDisplayName(painter, nameRect, index, selected);

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
    painter->setPen(selected ? Qt::white : ThemeManager::instance().color(ThemeColor::TertiaryText));
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
