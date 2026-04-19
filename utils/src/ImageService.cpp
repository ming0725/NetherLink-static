#include "ImageService.h"

#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace {

int imageCostKb(const QImage& image)
{
    return qMax(1, (image.bytesPerLine() * image.height()) / 1024);
}

QString variantKey(const char* mode,
                   const QString& source,
                   const QSize& size,
                   qreal dpr,
                   Qt::AspectRatioMode aspectMode,
                   int radius)
{
    return QString("imgsvc|%1|%2|%3x%4|%5|%6|%7")
            .arg(QString::fromLatin1(mode),
                 source,
                 QString::number(size.width()),
                 QString::number(size.height()),
                 QString::number(dpr, 'f', 2),
                 QString::number(static_cast<int>(aspectMode)),
                 QString::number(radius));
}

QPixmap renderPixmap(const QImage& source,
                     const QSize& logicalSize,
                     qreal dpr,
                     Qt::AspectRatioMode aspectMode,
                     int radius,
                     bool circular)
{
    if (source.isNull() || !logicalSize.isValid()) {
        return {};
    }

    const QSize pixelSize = QSize(qMax(1, qRound(logicalSize.width() * dpr)),
                                  qMax(1, qRound(logicalSize.height() * dpr)));

    QImage canvas(pixelSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform);

    QPainterPath clipPath;
    if (circular) {
        clipPath.addEllipse(QRect(QPoint(0, 0), pixelSize));
    } else if (radius > 0) {
        clipPath.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(pixelSize)),
                                radius * dpr,
                                radius * dpr);
    }
    if (!clipPath.isEmpty()) {
        painter.setClipPath(clipPath);
    }

    const QImage scaled = source.scaled(pixelSize,
                                        aspectMode,
                                        Qt::SmoothTransformation);

    QRect sourceRect(QPoint(0, 0), scaled.size());
    if (aspectMode == Qt::KeepAspectRatioByExpanding) {
        sourceRect.moveLeft(qMax(0, (scaled.width() - pixelSize.width()) / 2));
        sourceRect.moveTop(qMax(0, (scaled.height() - pixelSize.height()) / 2));
        sourceRect.setSize(pixelSize);
    }

    painter.drawImage(QRect(QPoint(0, 0), pixelSize), scaled, sourceRect);

    QPixmap pixmap = QPixmap::fromImage(canvas);
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

} // namespace

ImageService& ImageService::instance()
{
    static ImageService service;
    return service;
}

ImageService::ImageService()
    : m_originalCache(64 * 1024)
{
    if (QPixmapCache::cacheLimit() < 65536) {
        QPixmapCache::setCacheLimit(65536);
    }
}

QImage ImageService::originalImage(const QString& source) const
{
    if (source.isEmpty()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    if (QImage* cached = m_originalCache.object(source)) {
        return *cached;
    }

    QImageReader reader(source);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }

    m_originalCache.insert(source, new QImage(image), imageCostKb(image));
    return image;
}

QPixmap ImageService::pixmap(const QString& source) const
{
    const QImage image = originalImage(source);
    return image.isNull() ? QPixmap() : QPixmap::fromImage(image);
}

QSize ImageService::sourceSize(const QString& source) const
{
    return originalImage(source).size();
}

QPixmap ImageService::transformed(const QString& key,
                                  const QString& source,
                                  const QSize& targetSize,
                                  qreal devicePixelRatio,
                                  Qt::AspectRatioMode aspectMode,
                                  int radius,
                                  bool circular) const
{
    QPixmap pixmap;
    if (QPixmapCache::find(key, &pixmap)) {
        return pixmap;
    }

    const QImage image = originalImage(source);
    if (image.isNull()) {
        return {};
    }

    pixmap = renderPixmap(image,
                          targetSize,
                          devicePixelRatio,
                          aspectMode,
                          radius,
                          circular);
    if (!pixmap.isNull()) {
        QPixmapCache::insert(key, pixmap);
    }
    return pixmap;
}

QPixmap ImageService::scaled(const QString& source,
                             const QSize& targetSize,
                             Qt::AspectRatioMode aspectMode,
                             qreal devicePixelRatio) const
{
    if (targetSize.isEmpty()) {
        return {};
    }

    return transformed(variantKey("scaled",
                                  source,
                                  targetSize,
                                  devicePixelRatio,
                                  aspectMode,
                                  0),
                       source,
                       targetSize,
                       devicePixelRatio,
                       aspectMode,
                       0,
                       false);
}

QPixmap ImageService::centerCrop(const QString& source,
                                 const QSize& targetSize,
                                 int radius,
                                 qreal devicePixelRatio) const
{
    if (targetSize.isEmpty()) {
        return {};
    }

    return transformed(variantKey("crop",
                                  source,
                                  targetSize,
                                  devicePixelRatio,
                                  Qt::KeepAspectRatioByExpanding,
                                  radius),
                       source,
                       targetSize,
                       devicePixelRatio,
                       Qt::KeepAspectRatioByExpanding,
                       radius,
                       false);
}

QPixmap ImageService::circularAvatar(const QString& source,
                                     int size,
                                     qreal devicePixelRatio) const
{
    if (size <= 0) {
        return {};
    }

    const QSize targetSize(size, size);
    return transformed(variantKey("avatar",
                                  source,
                                  targetSize,
                                  devicePixelRatio,
                                  Qt::KeepAspectRatioByExpanding,
                                  size / 2),
                       source,
                       targetSize,
                       devicePixelRatio,
                       Qt::KeepAspectRatioByExpanding,
                       size / 2,
                       true);
}

void ImageService::invalidateSource(const QString& source)
{
    if (source.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_originalCache.remove(source);
}
