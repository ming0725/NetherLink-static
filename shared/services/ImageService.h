#pragma once

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

class ImageService : public QObject {
    Q_OBJECT

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

    QPixmap previewCrop(const QString& source,
                        const QSize& targetSize,
                        int radius = 0,
                        qreal devicePixelRatio = 1.0) const;

    void requestPreviewWarmup(const QString& source,
                              const QSize& targetSize,
                              qreal devicePixelRatio = 1.0);

    QPixmap circularAvatar(const QString& source,
                           int size,
                           qreal devicePixelRatio = 1.0) const;

    QPixmap circularAvatarPreview(const QString& source,
                                  int size,
                                  qreal devicePixelRatio = 1.0) const;

    void invalidateSource(const QString& source);

signals:
    void previewReady();

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
    mutable QCache<QString, QImage> m_previewCache;
    mutable QSet<QString> m_pendingPreviewLoads;
};
