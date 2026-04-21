#include "AiChatListDelegate.h"

#include <QApplication>
#include <QPainter>

#include "AiChatListModel.h"
#include "ImageService.h"

AiChatListDelegate::AiChatListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void AiChatListDelegate::paint(QPainter* painter,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
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
        painter->setPen(QColor(0x66, 0x66, 0x66));
        painter->drawText(sectionRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          index.data(AiChatListModel::SectionTitleRole).toString());
    }

    const QRect bodyRect = itemBodyRect(option, index);
    const QColor backgroundColor = selected
            ? QColor(0xec, 0xec, 0xec)
            : (hovered ? QColor(0xf9, 0xf9, 0xf9) : QColor(0xff, 0xff, 0xff));

    painter->setPen(Qt::NoPen);
    painter->setBrush(backgroundColor);
    painter->drawRoundedRect(bodyRect, kCornerRadius, kCornerRadius);

    const QRect textRect = titleRect(option, index);
    QFont titleFont = option.font;
    titleFont.setPixelSize(14);
    painter->setFont(titleFont);
    painter->setPen(Qt::black);
    painter->drawText(textRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(titleFont).elidedText(index.data(AiChatListModel::TitleRole).toString(),
                                                         Qt::ElideRight,
                                                         textRect.width()));

    if (hovered || selected) {
        const QRect iconRect = moreButtonRect(option, index);
        const QRect drawRect(iconRect.center().x() - kMoreIconSize / 2,
                             iconRect.center().y() - kMoreIconSize / 2,
                             kMoreIconSize,
                             kMoreIconSize);
        const QPixmap icon = ImageService::instance().scaled(QStringLiteral(":/resources/icon/more.png"),
                                                             drawRect.size(),
                                                             Qt::KeepAspectRatio,
                                                             painter->device()->devicePixelRatioF());
        painter->drawPixmap(drawRect, icon);
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
