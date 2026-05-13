#include "PostOverlay.h"

#include "shared/theme/ThemeManager.h"

#include <QPainter>

PostOverlay::PostOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void PostOverlay::setOverlayOpacity(qreal opacity)
{
    const qreal bounded = qBound(0.0, opacity, 1.0);
    if (!qFuzzyCompare(m_opacity, bounded)) {
        m_opacity = bounded;
        update();
    }
}

void PostOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    QColor color = ThemeManager::instance().color(ThemeColor::PostOverlay);
    color.setAlphaF(color.alphaF() * m_opacity);
    painter.fillRect(rect(), color);
}
