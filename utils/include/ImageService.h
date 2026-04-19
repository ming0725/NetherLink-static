#pragma once

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QPixmap>
#include <QString>

class ImageService {
public:
    static ImageService& instance();

    QPixmap pixmap(const QString& source) const;
    QSize sourceSize(const QString& source) const;

    QPixmap scaled(const QString& source,
                   const QSize& targetSize,
                   Qt::AspectRatioMode aspectMode = Qt::KeepAspectRatio,
                   qreal devicePixelRatio = 1.0) const;

    QPixmap centerCrop(const QString& source,
                       const QSize& targetSize,
                       int radius = 0,
                       qreal devicePixelRatio = 1.0) const;

    QPixmap circularAvatar(const QString& source,
                           int size,
                           qreal devicePixelRatio = 1.0) const;

    void invalidateSource(const QString& source);

private:
    ImageService();
    ImageService(const ImageService&) = delete;
    ImageService& operator=(const ImageService&) = delete;

    QImage originalImage(const QString& source) const;
    QPixmap transformed(const QString& key,
                        const QString& source,
                        const QSize& targetSize,
                        qreal devicePixelRatio,
                        Qt::AspectRatioMode aspectMode,
                        int radius,
                        bool circular) const;

    mutable QMutex m_mutex;
    mutable QCache<QString, QImage> m_originalCache;
};
