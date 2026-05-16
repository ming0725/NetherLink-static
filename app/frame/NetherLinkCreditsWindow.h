#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>
#include <QVector>

class QColor;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QRectF;
class QResizeEvent;
class QShowEvent;
class QTimer;

class NetherLinkCreditsWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit NetherLinkCreditsWindow(QWidget* parent = nullptr);

protected:
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    enum class EntryType {
        Logo,
        YellowLine,
        YellowSmallLine,
        Section,
        RoleName,
        Spacer
    };

    struct CreditPerson {
        QString name;
        QString url;
    };

    struct CreditEntry {
        EntryType type;
        QString primary;
        QVector<CreditPerson> people;
        int spacing = 0;
    };

    void buildCredits();
    void prepareResources();
    void releaseResources();
    void rebuildBackgroundTile(qreal devicePixelRatio);
    void rebuildBackgroundStrip(qreal devicePixelRatio);
    void rebuildCreditsPixmap(qreal devicePixelRatio);
    void rebuildVignette(qreal devicePixelRatio);
    void drawBackground(QPainter& painter);
    void drawCredits(QPainter& painter);
    void drawEntry(QPainter& painter, const CreditEntry& entry, qreal y, qreal height);
    void drawRevealOverlay(QPainter& painter) const;
    void drawVignette(QPainter& painter);
    void paintVignetteLayer(QPainter& painter, const QRectF& bounds) const;
    void drawCenteredText(QPainter& painter,
                          const QString& text,
                          qreal y,
                          int pixelSize,
                          const QColor& color,
                          qreal height,
                          bool bold = false) const;
    void drawWrappedCenteredText(QPainter& painter,
                                 const QString& text,
                                 qreal y,
                                 int pixelSize,
                                 const QColor& color,
                                 qreal lineHeight,
                                 bool bold = false) const;
    QVector<QString> wrappedTextLines(const QString& text, int pixelSize, bool bold = false) const;
    qreal wrappedTextHeight(const QString& text, int pixelSize, qreal lineHeight, bool bold = false) const;
    qreal entryHeight(const CreditEntry& entry) const;
    qreal logoHeight() const;
    bool isScrollFinished() const;
    qreal scrollOffset() const;
    qreal totalCreditsHeight() const;
    qreal visibleCreditsHeight() const;

    QTimer* m_frameTimer = nullptr;
    QElapsedTimer m_clock;
    QVector<CreditEntry> m_entries;
    bool m_resourcesPrepared = false;
    QImage m_backgroundImage;
    QPixmap m_backgroundTile;
    QSize m_backgroundTileSize;
    qreal m_backgroundTileDevicePixelRatio = 0.0;
    QPixmap m_backgroundStrip;
    QSize m_backgroundStripSize;
    qreal m_backgroundStripDevicePixelRatio = 0.0;
    QPixmap m_logoPixmap;
    QPixmap m_creditsPixmap;
    QSize m_creditsPixmapSize;
    qreal m_creditsDevicePixelRatio = 0.0;
    qreal m_totalCreditsHeight = 0.0;
    QPixmap m_vignettePixmap;
    QSize m_vignetteSize;
    qreal m_vignetteDevicePixelRatio = 0.0;
};
