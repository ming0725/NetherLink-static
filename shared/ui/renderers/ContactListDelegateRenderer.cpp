#include "ContactListDelegateRenderer.h"

#include <QPainter>

#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/renderers/BadgeRenderer.h"

namespace {

constexpr int kOverlayRadius = 6;
constexpr int kArrowBadgeGap = 12;
constexpr int kBadgeTextGap = 6;
constexpr int kTitleLeft = 20;

QRect overlayRectFor(const QRect& rect)
{
    return rect.adjusted(6, 3, -6, -3);
}

} // namespace

namespace ContactListDelegateRenderer {

PaintColors currentPaintColors()
{
    PaintColors colors;
    colors.dark = ThemeManager::instance().isDark();
    colors.panelBackground = ThemeManager::instance().color(ThemeColor::PanelBackground);
    colors.listHover = ThemeManager::instance().color(ThemeColor::ListHover);
    colors.listSelected = ThemeManager::instance().color(ThemeColor::ListSelected);
    colors.noticeSelectedBackground = ThemeManager::instance().color(ThemeColor::ListNoticeSelected);
    colors.primaryText = ThemeManager::instance().color(ThemeColor::PrimaryText);
    colors.secondaryText = ThemeManager::instance().color(ThemeColor::SecondaryText);
    colors.tertiaryText = ThemeManager::instance().color(ThemeColor::TertiaryText);
    colors.selectedText = ThemeManager::textColorOn(colors.listSelected);
    colors.selectedSecondaryText = ThemeManager::textColorOn(colors.listSelected, 165);
    colors.imagePlaceholder = ThemeManager::instance().color(ThemeColor::ImagePlaceholder);
    return colors;
}

void drawHoverOrSelectedOverlay(QPainter* painter,
                                const QRect& rect,
                                bool selected,
                                bool hovered,
                                const PaintColors& colors)
{
    if (!selected && !hovered) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(*painter);
    painter->setBrush(selected ? colors.noticeSelectedBackground : colors.listHover);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(overlayRectFor(rect), kOverlayRadius, kOverlayRadius);
    painter->restore();
}

void drawRightChevron(QPainter* painter,
                      const QPointF& center,
                      const QColor& color,
                      qreal penWidth)
{
    QPen arrowPen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(arrowPen);
    painter->drawLine(QPointF(center.x() - 2.0, center.y() - 4.0),
                      QPointF(center.x() + 2.0, center.y()));
    painter->drawLine(QPointF(center.x() + 2.0, center.y()),
                      QPointF(center.x() - 2.0, center.y() + 4.0));
}

void drawDisclosureArrow(QPainter* painter,
                         const QRect& rect,
                         qreal progress,
                         const QColor& color)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(*painter);
    painter->translate(rect.center());
    painter->rotate(progress * 90.0);
    QPen arrowPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(arrowPen);
    painter->drawLine(QPointF(-2.5, -4.0), QPointF(2.5, 0.0));
    painter->drawLine(QPointF(2.5, 0.0), QPointF(-2.5, 4.0));
    painter->restore();
}

void drawNoticeRow(QPainter* painter,
                   const QRect& rect,
                   const QString& title,
                   int unreadCount,
                   bool selected,
                   bool hovered,
                   int rightPadding,
                   int arrowCenterYOffset,
                   const QFont& titleFont,
                   const QFontMetrics& titleMetrics,
                   const PaintColors& colors)
{
    painter->fillRect(rect, colors.panelBackground);
    drawHoverOrSelectedOverlay(painter, rect, selected, hovered, colors);

    const int arrowCenterX = rect.right() - rightPadding;
    const int arrowCenterY = rect.center().y() + arrowCenterYOffset;
    int rightLimit = arrowCenterX - kArrowBadgeGap;

    const BadgeLayout badgeLayout = selected
        ? BadgeLayout{}
        : BadgeRenderer::layoutForUnreadCount(unreadCount, false, selected, colors.dark);
    if (badgeLayout.size.isValid()) {
        const int badgeX = rightLimit - badgeLayout.size.width();
        const QRect badgeRect(badgeX,
                              arrowCenterY - badgeLayout.size.height() / 2,
                              badgeLayout.size.width(),
                              badgeLayout.size.height());
        BadgeRenderer::drawBadge(painter, badgeRect, badgeLayout, selected);
        rightLimit = badgeX - kBadgeTextGap;
    }

    painter->setFont(titleFont);
    painter->setPen(colors.primaryText);
    const QRect textRect(kTitleLeft,
                         rect.top(),
                         qMax(0, rightLimit - kTitleLeft),
                         rect.height());
    painter->drawText(textRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      titleMetrics.elidedText(title, Qt::ElideRight, textRect.width()));

    drawRightChevron(painter,
                     QPointF(arrowCenterX, arrowCenterY),
                     colors.secondaryText);
}

void drawSectionHeaderRow(QPainter* painter,
                          const QRect& rect,
                          const QString& title,
                          const QString& countText,
                          qreal progress,
                          bool hovered,
                          int leftPadding,
                          int rightPadding,
                          int arrowSize,
                          int arrowYOffset,
                          const QFont& titleFont,
                          const QFontMetrics& titleMetrics,
                          const QFont& countFont,
                          const QFontMetrics& countMetrics,
                          const PaintColors& colors)
{
    painter->fillRect(rect, colors.panelBackground);
    drawHoverOrSelectedOverlay(painter, rect, false, hovered, colors);

    const QRect arrowRect(rect.left() + leftPadding,
                          rect.top() + (rect.height() - arrowSize) / 2 + arrowYOffset,
                          arrowSize,
                          arrowSize);
    drawDisclosureArrow(painter, arrowRect, progress, colors.tertiaryText);

    const int titleLeft = arrowRect.right() + 8;
    const int countWidth = countMetrics.horizontalAdvance(countText);
    const int rightEdge = rect.right() - rightPadding;
    const QRect countRect(qMax(titleLeft, rightEdge - countWidth + 1),
                          rect.top(),
                          countWidth,
                          rect.height());
    const QRect titleRect(titleLeft,
                          rect.top(),
                          qMax(0, countRect.left() - titleLeft - 8),
                          rect.height());

    painter->setFont(titleFont);
    painter->setPen(colors.secondaryText);
    painter->drawText(titleRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      titleMetrics.elidedText(title, Qt::ElideRight, titleRect.width()));
    painter->setFont(countFont);
    painter->setPen(colors.tertiaryText);
    painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, countText);
}

} // namespace ContactListDelegateRenderer
