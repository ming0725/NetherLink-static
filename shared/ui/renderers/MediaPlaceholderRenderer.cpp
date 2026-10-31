#include "MediaPlaceholderRenderer.h"

#include <QPainter>
#include <QPainterPath>

#include "shared/theme/ThemeManager.h"

namespace {

QColor iconColor()
{
    QColor color = ThemeManager::instance().color(ThemeColor::SecondaryText);
    color.setAlpha(ThemeManager::instance().isDark() ? 118 : 105);
    return color;
}

} // namespace

namespace MediaPlaceholderRenderer {

void drawAvatar(QPainter* painter, const QRect& rect)
{
    if (!painter || rect.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().color(ThemeColor::ImagePlaceholder));
    painter->drawEllipse(rect);

    const qreal unit = qMin(rect.width(), rect.height()) / 100.0;
    const QPointF center = QRectF(rect).center();
    const QColor glyph = iconColor();
    painter->setBrush(glyph);
    painter->drawEllipse(QRectF(center.x() - 17 * unit,
                                rect.top() + 24 * unit,
                                34 * unit,
                                34 * unit));

    QPainterPath shoulders;
    shoulders.addRoundedRect(QRectF(center.x() - 31 * unit,
                                    rect.top() + 61 * unit,
                                    62 * unit,
                                    31 * unit),
                             16 * unit,
                             16 * unit);
    painter->drawPath(shoulders);
    painter->restore();
}

void drawImage(QPainter* painter, const QRect& rect, int radius)
{
    if (!painter || rect.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().color(ThemeColor::ImagePlaceholder));
    painter->drawRoundedRect(rect, radius, radius);

    const QRectF bounds(rect);
    const qreal unit = qMin(bounds.width(), bounds.height()) / 100.0;
    const QColor glyph = iconColor();
    painter->setBrush(glyph);

    const QRectF sun(bounds.left() + 64 * unit,
                     bounds.top() + 22 * unit,
                     14 * unit,
                     14 * unit);
    painter->drawEllipse(sun);

    QPainterPath mountain;
    mountain.moveTo(bounds.left() + 17 * unit, bounds.bottom() - 20 * unit);
    mountain.lineTo(bounds.left() + 38 * unit, bounds.top() + 53 * unit);
    mountain.lineTo(bounds.left() + 51 * unit, bounds.top() + 68 * unit);
    mountain.lineTo(bounds.left() + 62 * unit, bounds.top() + 49 * unit);
    mountain.lineTo(bounds.right() - 15 * unit, bounds.bottom() - 20 * unit);
    mountain.closeSubpath();
    painter->drawPath(mountain);
    painter->restore();
}

} // namespace MediaPlaceholderRenderer
