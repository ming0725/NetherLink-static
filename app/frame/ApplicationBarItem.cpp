#include "ApplicationBarItem.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QtMath>

namespace {

QSize logicalPixmapSize(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return {};
    }

    const qreal dpr = pixmap.devicePixelRatio();
    return QSize(qRound(pixmap.width() / dpr),
                 qRound(pixmap.height() / dpr));
}

QString invertedVariantKey(const QString& source, const QSize& size, qreal dpr)
{
    return QStringLiteral("appbar-inverted|%1|%2x%3|%4")
            .arg(source,
                 QString::number(size.width()),
                 QString::number(size.height()),
                 QString::number(dpr, 'f', 2));
}

QPixmap invertedPixmap(const QString& key, const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return pixmap;
    }

    QPixmap cached;
    if (QPixmapCache::find(key, &cached)) {
        return cached;
    }

    QImage image = pixmap.toImage();
    image.invertPixels(QImage::InvertRgb);

    QPixmap inverted = QPixmap::fromImage(image);
    inverted.setDevicePixelRatio(pixmap.devicePixelRatio());
    QPixmapCache::insert(key, inverted);
    return inverted;
}

}

ApplicationBarItem::ApplicationBarItem(const QString& normalSource,
                                       const QString& selectedSource,
                                       QObject* parent)
    : QObject(parent)
    , selectedSource(selectedSource)
    , normalSource(normalSource)
{
    rippleAnim = new QVariantAnimation(this);
    rippleAnim->setDuration(700);
    rippleAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(rippleAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        rippleRadius = value.toReal();
        emit updateRequested();
    });
    connect(rippleAnim, &QVariantAnimation::finished, this, [this]() {
        if (selected) {
            rippleRadius = maxRippleRadius();
            emit updateRequested();
        }
    });
}

ApplicationBarItem::ApplicationBarItem(const QString& normalSource, QObject* parent)
    : ApplicationBarItem(normalSource, normalSource, parent)
{
}

void ApplicationBarItem::setPixmapScale(qreal scale) {
    pixmapScale = qBound<qreal>(0.0, scale, 1.0);
    emit updateRequested();
}

void ApplicationBarItem::setDarkModeInversionEnabled(bool enabled)
{
    if (darkModeInversionEnabled == enabled) {
        return;
    }

    darkModeInversionEnabled = enabled;
    emit updateRequested();
}

void ApplicationBarItem::setSelected(bool select)
{
    if (selected == select) {
        return;
    }

    selected = select;

    if (selected) {
        const qreal maxR = maxRippleRadius();
        rippleAnim->stop();
        rippleAnim->setStartValue(0.0);
        rippleAnim->setEndValue(maxR);
        rippleAnim->start();
    } else {
        rippleAnim->stop();
        rippleRadius = 0.0;
        emit updateRequested();
    }
}

bool ApplicationBarItem::isSelected() const
{
    return selected;
}

void ApplicationBarItem::setHovered(bool hover)
{
    if (hovered == hover) {
        return;
    }

    hovered = hover;
    emit updateRequested();
}

bool ApplicationBarItem::isHovered() const
{
    return hovered;
}

void ApplicationBarItem::setRect(const QRect& rect)
{
    if (itemRect == rect) {
        return;
    }

    itemRect = rect;
    if (selected && rippleAnim->state() != QAbstractAnimation::Running) {
        rippleRadius = maxRippleRadius();
    }
    emit updateRequested();
}

QRect ApplicationBarItem::rect() const
{
    return itemRect;
}

int ApplicationBarItem::y() const
{
    return itemRect.y();
}

bool ApplicationBarItem::contains(const QPoint& pos) const
{
    return itemRect.contains(pos);
}

void ApplicationBarItem::paint(QPainter& painter) const
{
    if (itemRect.isEmpty()) {
        return;
    }

    // 1) hover/selected 背景圆角
    if (hovered || selected) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(ThemeManager::instance().color(ThemeColor::AppBarItemBackground));
        painter.drawRoundedRect(itemRect, 10, 10);
        painter.restore();
    }

    // 2) 正常状态下的图标（按 pixmapScale 缩放并居中）
    QSizeF tgtF = QSizeF(itemRect.size()) * pixmapScale;
    const QSize targetSize = tgtF.toSize();
    const qreal dpr = painter.device()->devicePixelRatioF();
    QPixmap normalScaled = ImageService::instance().scaled(normalSource,
                                                           targetSize,
                                                           Qt::KeepAspectRatio,
                                                           dpr);
    if (ThemeManager::instance().isDark() && darkModeInversionEnabled && !selected) {
        normalScaled = invertedPixmap(invertedVariantKey(normalSource, targetSize, dpr),
                                      normalScaled);
    }
    const QSize normalLogicalSize = logicalPixmapSize(normalScaled);
    int x0 = itemRect.x() + qFloor((itemRect.width()  - normalLogicalSize.width())  / 2.0);
    int y0 = itemRect.y() + qFloor((itemRect.height() - normalLogicalSize.height()) / 2.0);
    painter.drawPixmap(QRect(x0, y0,
                             normalLogicalSize.width(),
                             normalLogicalSize.height()),
                       normalScaled);

    // 3) 如果已选中且正在 ripple 动画中，则“揭露”selectedPixmap
    if (selected && rippleRadius > 0.0) {
        painter.save();
        // 从 item 左下角扩散，揭露选中态图标。
        QPainterPath clipPath;
        clipPath.addEllipse(
                QRectF(
                        itemRect.left() - rippleRadius,
                        itemRect.bottom() + 1 - rippleRadius,
                        rippleRadius * 2,
                        rippleRadius * 2
                )
        );
        painter.setClipPath(clipPath);
        // 在剪裁区域内绘制被选中状态的图标
        QPixmap selScaled = ImageService::instance().scaled(selectedSource,
                                                            targetSize,
                                                            Qt::KeepAspectRatio,
                                                            dpr);
        const QSize selectedLogicalSize = logicalPixmapSize(selScaled);
        const int sx = itemRect.x() + qFloor((itemRect.width()  - selectedLogicalSize.width())  / 2.0);
        const int sy = itemRect.y() + qFloor((itemRect.height() - selectedLogicalSize.height()) / 2.0);
        painter.drawPixmap(QRect(sx, sy,
                                 selectedLogicalSize.width(),
                                 selectedLogicalSize.height()),
                           selScaled);
        painter.restore();
    }
}

qreal ApplicationBarItem::maxRippleRadius() const
{
    return qSqrt(itemRect.width() * itemRect.width() +
                 itemRect.height() * itemRect.height());
}
