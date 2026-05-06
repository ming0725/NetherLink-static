#include "AiChatListDelegate.h"

#include <QApplication>
#include <QCursor>
#include <QPainter>

#include "features/aichat/model/AiChatListModel.h"
#include "shared/theme/ThemeManager.h"

AiChatListDelegate::AiChatListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void AiChatListDelegate::paint(QPainter* painter,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool sectionStart = index.data(AiChatListModel::IsSectionStartRole).toBool();

    if (sectionStart) {
        const QRect sectionRect = headerRect(option, index);
        QFont headerFont = option.font;
        headerFont.setPixelSize(13);
        headerFont.setBold(true);
        painter->setFont(headerFont);
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(sectionRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          index.data(AiChatListModel::SectionTitleRole).toString());
    }

    const QRect bodyRect = itemBodyRect(option, index);
    const QColor backgroundColor = selected
            ? ThemeManager::instance().color(ThemeColor::ListPinned)
            : (hovered ? ThemeManager::instance().color(ThemeColor::ListHover)
                       : ThemeManager::instance().color(ThemeColor::PanelBackground));

    painter->setPen(Qt::NoPen);
    painter->setBrush(backgroundColor);
    painter->drawRoundedRect(bodyRect, kCornerRadius, kCornerRadius);

    const QRect textRect = titleRect(option, index);
    QFont titleFont = option.font;
    titleFont.setPixelSize(14);
    painter->setFont(titleFont);
    painter->setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
    painter->drawText(textRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(titleFont).elidedText(index.data(AiChatListModel::TitleRole).toString(),
                                                         Qt::ElideRight,
                                                         textRect.width()));

    if (hovered || selected) {
        const QRect buttonRect = moreButtonRect(option, index);
        const QRect drawButtonRect = buttonRect.translated(0, 2);
        const QPoint cursorPos = option.widget
                ? option.widget->mapFromGlobal(QCursor::pos())
                : QPoint(-1, -1);
        const bool buttonHovered = buttonRect.contains(cursorPos);
        if (buttonHovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(ThemeManager::instance().color(ThemeColor::Divider));
            painter->drawRoundedRect(drawButtonRect, 5, 5);
        }

        const int dotsWidth = kMoreDotSize * 3 + kMoreDotGap * 2;
        const int startX = drawButtonRect.center().x() - dotsWidth / 2;
        const int y = drawButtonRect.center().y() - kMoreDotSize / 2;
        painter->setPen(Qt::NoPen);
        painter->setBrush(buttonHovered
                          ? ThemeManager::instance().color(ThemeColor::PrimaryText)
                          : ThemeManager::instance().color(ThemeColor::SecondaryText));
        for (int dot = 0; dot < 3; ++dot) {
            painter->drawRect(startX + dot * (kMoreDotSize + kMoreDotGap),
                              y,
                              kMoreDotSize,
                              kMoreDotSize);
        }
    }

    painter->restore();
}

QSize AiChatListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    Q_UNUSED(option);
    const bool sectionStart = index.data(AiChatListModel::IsSectionStartRole).toBool();
    const int height = kItemHeight + (sectionStart ? kSectionHeaderHeight + kSectionGap : 0);
    return QSize(0, height);
}

QRect AiChatListDelegate::moreButtonRect(const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const
{
    const QRect bodyRect = itemBodyRect(option, index);
    return QRect(bodyRect.right() - kTextRightPadding - kMoreButtonSize,
                 bodyRect.center().y() - kMoreButtonSize / 2,
                 kMoreButtonSize,
                 kMoreButtonSize);
}

int AiChatListDelegate::stickyHeaderHeight() const
{
    return kSectionHeaderHeight;
}

QRect AiChatListDelegate::itemBodyRect(const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
    const bool sectionStart = index.data(AiChatListModel::IsSectionStartRole).toBool();
    const int top = option.rect.top() + (sectionStart ? kSectionHeaderHeight + kSectionGap : 0);
    return QRect(option.rect.left() + kHorizontalMargin,
                 top,
                 qMax(0, option.rect.width() - kHorizontalMargin * 2),
                 kItemHeight);
}

QRect AiChatListDelegate::headerRect(const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const
{
    Q_UNUSED(index);
    return QRect(option.rect.left() + 12,
                 option.rect.top(),
                 qMax(0, option.rect.width() - 24),
                 kSectionHeaderHeight);
}

QRect AiChatListDelegate::titleRect(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    const QRect bodyRect = itemBodyRect(option, index);
    const QRect moreRect = moreButtonRect(option, index);
    return QRect(bodyRect.left() + kTextLeftPadding,
                 bodyRect.top(),
                 qMax(0, moreRect.left() - bodyRect.left() - kTextLeftPadding - 4),
                 bodyRect.height());
}
