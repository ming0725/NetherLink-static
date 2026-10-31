#include "NotificationDelegateRenderer.h"

#include <QPainter>

#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"

namespace {

constexpr qreal kButtonBorderWidth = 1.5;
constexpr int kPendingDotSize = 4;
constexpr int kPendingDotGap = 5;

} // namespace

namespace NotificationDelegateRenderer {

QString formatTime(const QDateTime& time)
{
    if (!time.isValid()) {
        return {};
    }
    return time.date() == QDate::currentDate()
            ? time.toString(QStringLiteral("HH:mm"))
            : time.toString(QStringLiteral("yyyy/MM/dd"));
}

QRect cardRect(const QRect& optionRect, const CardMetrics& metrics)
{
    return QRect(optionRect.left() + metrics.cardMarginH,
                 optionRect.top() + metrics.cardMarginV,
                 optionRect.width() - 2 * metrics.cardMarginH,
                 optionRect.height() - 2 * metrics.cardMarginV);
}

QRect avatarRect(const QRect& card, const CardMetrics& metrics)
{
    const int y = card.top() + (card.height() - metrics.avatarSize) / 2;
    return QRect(card.left() + metrics.cardPaddingH,
                 y,
                 metrics.avatarSize,
                 metrics.avatarSize);
}

QRect contentBounds(const QRect& card, const CardMetrics& metrics)
{
    const int left = card.left() + metrics.cardPaddingH
                     + metrics.avatarSize
                     + metrics.avatarToContentGap;
    return QRect(left,
                 card.top() + metrics.cardPaddingV,
                 card.right() - metrics.cardPaddingH - left,
                 card.height() - 2 * metrics.cardPaddingV);
}

QRect acceptButtonRect(const QRect& card, int y, const CardMetrics& metrics)
{
    const int right = card.right() - metrics.cardPaddingH;
    return QRect(right - metrics.buttonWidth * 2 - metrics.buttonGap,
                 y,
                 metrics.buttonWidth,
                 metrics.buttonHeight);
}

QRect rejectButtonRect(const QRect& card, int y, const CardMetrics& metrics)
{
    const int right = card.right() - metrics.cardPaddingH;
    return QRect(right - metrics.buttonWidth,
                 y,
                 metrics.buttonWidth,
                 metrics.buttonHeight);
}

QRect actionRect(const QRect& acceptRect, const QRect& rejectRect)
{
    return QRect(acceptRect.left(),
                 acceptRect.top(),
                 rejectRect.right() - acceptRect.left() + 1,
                 acceptRect.height());
}

void drawCard(QPainter* painter, const QRect& card, const CardMetrics& metrics)
{
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().color(ThemeColor::PanelBackground));
    painter->drawRoundedRect(card, metrics.cardRadius, metrics.cardRadius);
}

void drawAvatar(QPainter* painter,
                const QRect& rect,
                const QString& avatarPath,
                int avatarSize)
{
    const qreal dpr = painter->device()->devicePixelRatioF();
    const QPixmap pixmap = ImageService::instance().circularAvatarPreview(avatarPath, avatarSize, dpr);
    if (pixmap.isNull()) {
        MediaPlaceholderRenderer::drawAvatar(painter, rect);
        return;
    }
    painter->drawPixmap(rect, pixmap);
}

void drawPendingDots(QPainter* painter, const QRect& rect, const QColor& color)
{
    const int totalWidth = kPendingDotSize * 3 + kPendingDotGap * 2;
    const int startX = rect.left() + (rect.width() - totalWidth) / 2;
    const int y = rect.top() + (rect.height() - kPendingDotSize) / 2;
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    for (int i = 0; i < 3; ++i) {
        painter->drawRect(QRect(startX + i * (kPendingDotSize + kPendingDotGap),
                                y,
                                kPendingDotSize,
                                kPendingDotSize));
    }
}

void drawActionButtons(QPainter* painter,
                       const QRect& acceptRect,
                       const QRect& rejectRect,
                       int hoveredButton,
                       const QFont& font,
                       const CardMetrics& metrics)
{
    painter->setFont(font);

    QColor acceptBackground = ThemeManager::instance().color(ThemeColor::Accent);
    if (hoveredButton == 0) {
        acceptBackground = ThemeManager::instance().color(ThemeColor::AccentHover);
    }
    painter->setBrush(acceptBackground);
    painter->setPen(QPen(acceptBackground, kButtonBorderWidth));
    painter->drawRoundedRect(acceptRect, metrics.buttonRadius, metrics.buttonRadius);
    painter->setPen(ThemeManager::textColorOn(acceptBackground));
    painter->drawText(acceptRect, Qt::AlignCenter, QStringLiteral("同意"));

    const QColor rejectBorder = ThemeManager::instance().color(ThemeColor::DangerText);
    QColor rejectBackground = Qt::transparent;
    if (hoveredButton == 1) {
        rejectBackground = ThemeManager::instance().color(ThemeColor::DangerControlHover);
    }
    painter->setBrush(rejectBackground);
    painter->setPen(QPen(rejectBorder, kButtonBorderWidth));
    painter->drawRoundedRect(rejectRect, metrics.buttonRadius, metrics.buttonRadius);
    painter->setPen(rejectBorder);
    painter->drawText(rejectRect, Qt::AlignCenter, QStringLiteral("拒绝"));
}

} // namespace NotificationDelegateRenderer
