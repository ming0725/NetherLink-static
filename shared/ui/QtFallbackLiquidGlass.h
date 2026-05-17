#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QRectF>

#include <memory>

class QEvent;
class QOffscreenSurface;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QOpenGLFunctions;
class QOpenGLShaderProgram;
class QPainter;
class QString;
class QTimer;
class QVector2D;
class QWidget;

class QtFallbackLiquidGlassRenderer
{
public:
    ~QtFallbackLiquidGlassRenderer();

    static bool isSupported();

    void setShape(const QRectF& rect, qreal cornerRadius);
    QImage render(const QImage& source, qreal devicePixelRatio, bool dark);
    void release();

private:
    bool ensureReady();
    void markFailed();
    std::unique_ptr<QOpenGLShaderProgram> makeProgram(const QString& vertexSource,
                                                      const QString& fragmentSource);
    unsigned int uploadTexture(const QImage& image);
    void drawQuad(QOpenGLShaderProgram& program);
    void bindTexture(unsigned int texture, unsigned int textureUnit);
    void prepareTarget(QOpenGLFramebufferObject& target);
    void renderBlurPass(unsigned int sourceTexture,
                        QOpenGLFramebufferObject& target,
                        const QVector2D& texelStep);
    void renderGlassPass(unsigned int blurredTexture,
                         QOpenGLFramebufferObject& target,
                         const QVector2D& size,
                         const QRectF& physicalShapeRect,
                         float radius,
                         float shapeInset,
                         float dark);

    QOffscreenSurface* m_surface = nullptr;
    QOpenGLContext* m_context = nullptr;
    QOpenGLFunctions* m_gl = nullptr;
    std::unique_ptr<QOpenGLShaderProgram> m_blurProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_glassProgram;
    QRectF m_shapeRect;
    qreal m_cornerRadius = 0.0;
    bool m_ready = false;
    bool m_failed = false;
};

class QtFallbackLiquidGlassController : public QObject
{
public:
    enum class EffectMode {
        LiquidGlass,
        GaussianBlur
    };

    explicit QtFallbackLiquidGlassController(QWidget* targetWidget);
    ~QtFallbackLiquidGlassController() override;

    void setSourceWidget(QWidget* widget);
    void setEffectMode(EffectMode mode);
    void setEnabled(bool enabled, bool updateWidget = true);
    bool isEnabled() const;
    void setShape(const QRectF& rect, qreal cornerRadius);
    void scheduleUpdate(int delayMs = 0);
    void scheduleInteractiveUpdate();
    void release(bool updateWidget = true);
    bool handleHostEvent(QEvent* event);
    void paint(QPainter& painter);

    static int refreshDelayMs();
    static bool liquidGlassSupported();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void scheduleUpdateInternal(int delayMs, bool force);
    void installSourceEventFilter();
    void stopUpdateTimer();
    void resetRefreshCache();
    int passiveRefreshDelayMs() const;
    void updateBackground();
    QImage captureSource(qreal devicePixelRatio) const;
    QImage renderBackground(const QImage& source, qreal devicePixelRatio);
    QImage renderBackgroundWithFastBlur(const QImage& source, qreal devicePixelRatio);
    void startGaussianBlurTask(const QImage& source, qreal devicePixelRatio);
    void finishGaussianBlurTask(quint64 generation, const QImage& background);
    QRectF targetRect() const;

    QPointer<QWidget> m_targetWidget;
    QPointer<QWidget> m_sourceWidget;
    QTimer* m_updateTimer = nullptr;
    QImage m_background;
    std::unique_ptr<QtFallbackLiquidGlassRenderer> m_renderer;
    QRectF m_shapeRect;
    qreal m_cornerRadius = 0.0;
    EffectMode m_effectMode = EffectMode::LiquidGlass;
    quint64 m_gaussianBlurGeneration = 0;
    quint64 m_lastSourceSignature = 0;
    int m_unchangedPassiveRefreshCount = 0;
    bool m_enabled = false;
    bool m_sourceFilterInstalled = false;
    bool m_captureInProgress = false;
    bool m_hasSourceSignature = false;
    bool m_pendingUpdateForced = false;
    bool m_gaussianBlurInFlight = false;
    bool m_gaussianBlurUpdatePending = false;
};
