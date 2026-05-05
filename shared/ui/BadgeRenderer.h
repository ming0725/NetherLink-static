#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QSize>
#include <QString>

class QPainter;
class QRect;

struct BadgeLayout {
    QSize size;
    QString text;
    bool drawIcon = false;
    QColor backgroundColor;
    QColor textColor;
};

namespace BadgeRenderer {

constexpr int kBadgeHeight = 16;
constexpr int kBadgeHorizontalPadding = 4;

inline const QFont& badgeFont()
{
    static const QFont font = []() {
        QFont f;
        f.setBold(true);
        f.setPixelSize(11);
        return f;
    }();
    return font;
}

inline QFontMetrics badgeMetrics()
{
    return QFontMetrics(badgeFont());
}

// Compute badge layout from unread count and do-not-disturb state.
// Set selected=true for selected item rendering.
// Set isDark=true for dark theme.
BadgeLayout layoutForUnreadCount(int unreadCount, bool doNotDisturb,
                                 bool selected, bool isDark);

// Draw a badge. Handles both icon mode (drawIcon) and text count mode.
void drawBadge(QPainter* painter, const QRect& rect,
               const BadgeLayout& layout, bool selected);

} // namespace BadgeRenderer
