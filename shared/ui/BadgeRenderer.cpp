#include "BadgeRenderer.h"

#include <QPainter>

#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"

namespace BadgeRenderer {

BadgeLayout layoutForUnreadCount(int unreadCount, bool doNotDisturb,
                                  bool selected, bool isDark)
{
    BadgeLayout layout;
    if (unreadCount <= 0) {
        if (doNotDisturb) {
            layout.drawIcon = true;
            layout.size = QSize(kBadgeHeight, kBadgeHeight);
        }
        return layout;
    }

    layout.text = unreadCount < 100 ? QString::number(unreadCount)
                                    : QStringLiteral("99+");
    const int textWidth = badgeMetrics().horizontalAdvance(layout.text);
    const int width = qMax(kBadgeHeight, textWidth + kBadgeHorizontalPadding * 2);
    layout.size = QSize(width, kBadgeHeight);

    Q_UNUSED(isDark);
    layout.backgroundColor = doNotDisturb
            ? ThemeManager::instance().color(ThemeColor::BadgeMutedBackground)
            : ThemeManager::instance().color(ThemeColor::BadgeUnreadBackground);
    layout.textColor = doNotDisturb
            ? ThemeManager::instance().color(ThemeColor::BadgeMutedText)
            : ThemeManager::instance().color(ThemeColor::BadgeUnreadText);

    if (selected && !doNotDisturb) {
        layout.backgroundColor = ThemeManager::instance().color(ThemeColor::BadgeSelectedBackground);
        layout.textColor = ThemeManager::instance().color(ThemeColor::Accent);
    }

    return layout;
}

void drawBadge(QPainter* painter, const QRect& rect,
                const BadgeLayout& layout, bool selected)
{
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing |
                            QPainter::TextAntialiasing |
                            QPainter::SmoothPixmapTransform,
                            true);

    if (layout.drawIcon) {
        const QString iconPath = selected
                ? QStringLiteral(":/resources/icon/selected_notification.png")
                : QStringLiteral(":/resources/icon/notification.png");
        const QPixmap icon = ImageService::instance().scaled(iconPath,
                                                             rect.size(),
                                                             Qt::KeepAspectRatio,
                                                             painter->device()->devicePixelRatioF());
        painter->drawPixmap(rect, icon);
        painter->restore();
        return;
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(layout.backgroundColor);
    const QRectF badgeRect = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
    if (qFuzzyCompare(badgeRect.width(), badgeRect.height())) {
        painter->drawEllipse(badgeRect);
    } else {
        painter->drawRoundedRect(badgeRect, badgeRect.height() / 2.0,
                                 badgeRect.height() / 2.0);
    }

    painter->setFont(badgeFont());
    painter->setPen(layout.textColor);
    painter->drawText(rect, Qt::AlignCenter, layout.text);
    painter->restore();
}

} // namespace BadgeRenderer
