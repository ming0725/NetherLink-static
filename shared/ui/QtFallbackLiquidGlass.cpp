#include "QtFallbackLiquidGlass.h"

#include "shared/theme/ThemeManager.h"
#include "shared/ui/FastGaussianBlur.h"

#include <QEvent>
#include <QFile>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScopedValueRollback>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVector2D>
#include <QVector4D>
#include <QWidget>
#include <QtMath>

namespace {

constexpr qreal kLiquidGlassRenderScale = 0.82;
constexpr qreal kLiquidGlassBorderWidth = 1.0;
constexpr qreal kLiquidGlassLightBorderWidth = 1.0;
constexpr float kLiquidGlassBlurStep = 1.7f;
constexpr int kLiquidGlassRefreshDelayMs = 33;
constexpr int kLiquidGlassQtBlurRadius = 18;
constexpr int kLiquidGlassPassiveRefreshDelayMs = 100;
constexpr int kLiquidGlassMaxPassiveRefreshDelayMs = 1000;

QRectF liquidGlassInsetRect(const QRectF& panelRect, qreal inset)
{
    return panelRect.adjusted(inset, inset, -inset, -inset);
}

qreal liquidGlassInsetRadius(qreal cornerRadius, qreal inset)
{
    return qMax<qreal>(0.0, cornerRadius - inset);
}

QPainterPath liquidGlassInsetPath(const QRectF& panelRect, qreal cornerRadius, qreal inset)
{
    QPainterPath path;
    path.addRoundedRect(liquidGlassInsetRect(panelRect, inset),
                        liquidGlassInsetRadius(cornerRadius, inset),
                        liquidGlassInsetRadius(cornerRadius, inset));
    return path;
}

qreal liquidGlassBorderWidthForMode(bool darkMode)
{
    return darkMode ? kLiquidGlassBorderWidth : kLiquidGlassLightBorderWidth;
}

qreal liquidGlassContentInset(qreal borderWidth, bool darkMode)
{
    return darkMode ? borderWidth / 2.0 : borderWidth;
}

QString loadShaderSource(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QRectF physicalShapeRectFor(const QRectF& logicalShapeRect,
                            const QSize& renderSize,
                            qreal devicePixelRatio)
{
    const qreal pixelScale = devicePixelRatio * kLiquidGlassRenderScale;
    QRectF shapeRect(logicalShapeRect.x() * pixelScale,
                     logicalShapeRect.y() * pixelScale,
                     logicalShapeRect.width() * pixelScale,
                     logicalShapeRect.height() * pixelScale);
    const QRectF imageRect(0.0, 0.0, renderSize.width(), renderSize.height());
    shapeRect = shapeRect.intersected(imageRect);
    if (!shapeRect.isValid() || shapeRect.isEmpty()) {
        return imageRect;
    }
    return shapeRect;
}

quint64 liquidGlassSourceSignature(const QImage& image)
{
    if (image.isNull()) {
        return 0;
    }

    constexpr quint64 kFnvOffset = 1469598103934665603ULL;
    constexpr quint64 kFnvPrime = 1099511628211ULL;
    quint64 hash = kFnvOffset;

    const auto mix = [&hash](quint64 value) {
        for (int shift = 0; shift < 64; shift += 8) {
            hash ^= (value >> shift) & 0xffU;
            hash *= kFnvPrime;
        }
    };

    mix(static_cast<quint64>(image.width()));
    mix(static_cast<quint64>(image.height()));
    mix(static_cast<quint64>(image.bytesPerLine()));
    mix(static_cast<quint64>(image.format()));
    mix(static_cast<quint64>(qRound64(image.devicePixelRatio() * 1000.0)));

    for (int y = 0; y < image.height(); ++y) {
        const uchar* line = image.constScanLine(y);
        for (int x = 0; x < image.bytesPerLine(); ++x) {
            hash ^= line[x];
            hash *= kFnvPrime;
        }
    }
    return hash;
}

} // namespace

QtFallbackLiquidGlassRenderer::~QtFallbackLiquidGlassRenderer()
{
    release();
}

bool QtFallbackLiquidGlassRenderer::isSupported()
{
    static const bool supported = [] {
        QtFallbackLiquidGlassRenderer renderer;
        return renderer.ensureReady();
    }();
    return supported;
}

void QtFallbackLiquidGlassRenderer::setShape(const QRectF& rect, qreal cornerRadius)
{
    m_shapeRect = rect;
    m_cornerRadius = qMax<qreal>(0.0, cornerRadius);
}

QImage QtFallbackLiquidGlassRenderer::render(const QImage& source,
                                             qreal devicePixelRatio,
                                             bool dark)
{
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return {};
    }
    if (!ensureReady() || !m_context || !m_surface || !m_context->makeCurrent(m_surface)) {
        return {};
    }

    QImage uploadImage = source.convertToFormat(QImage::Format_RGBA8888);
    uploadImage.setDevicePixelRatio(1.0);
    const QSize renderSize = uploadImage.size();
    const QRectF logicalShapeRect = m_shapeRect.isValid()
            ? m_shapeRect
            : QRectF(0.0,
                     0.0,
                     renderSize.width() / (devicePixelRatio * kLiquidGlassRenderScale),
                     renderSize.height() / (devicePixelRatio * kLiquidGlassRenderScale));
    const QRectF physicalShapeRect = physicalShapeRectFor(logicalShapeRect,
                                                          renderSize,
                                                          devicePixelRatio);

    GLuint sourceTexture = uploadTexture(uploadImage);
    if (sourceTexture == 0) {
        m_context->doneCurrent();
        return {};
    }

    QImage output;
    {
        QOpenGLFramebufferObjectFormat fboFormat;
        fboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);
        fboFormat.setInternalTextureFormat(GL_RGBA);
        QOpenGLFramebufferObject horizontalFbo(renderSize, fboFormat);
        QOpenGLFramebufferObject verticalFbo(renderSize, fboFormat);
        if (horizontalFbo.isValid() && verticalFbo.isValid()) {
            renderBlurPass(sourceTexture,
                           horizontalFbo,
                           QVector2D(kLiquidGlassBlurStep / qMax(1, renderSize.width()), 0.0f));
            renderBlurPass(horizontalFbo.texture(),
                           verticalFbo,
                           QVector2D(0.0f, kLiquidGlassBlurStep / qMax(1, renderSize.height())));
            const qreal borderWidth = liquidGlassBorderWidthForMode(dark);
            const qreal contentInset = liquidGlassContentInset(borderWidth, dark);
            const qreal pixelScale = devicePixelRatio * kLiquidGlassRenderScale;
            renderGlassPass(verticalFbo.texture(),
                            horizontalFbo,
                            QVector2D(static_cast<float>(renderSize.width()),
                                      static_cast<float>(renderSize.height())),
                            physicalShapeRect,
                            static_cast<float>(liquidGlassInsetRadius(m_cornerRadius,
                                                                       contentInset) * pixelScale),
                            static_cast<float>(contentInset * pixelScale),
                            dark ? 1.0f : 0.0f);

            output = horizontalFbo.toImage(false).convertToFormat(QImage::Format_ARGB32_Premultiplied);
            output.setDevicePixelRatio(devicePixelRatio * kLiquidGlassRenderScale);
        }
        m_gl->glDeleteTextures(1, &sourceTexture);
    }
    m_context->doneCurrent();
    return output;
}

bool QtFallbackLiquidGlassRenderer::ensureReady()
{
    if (m_ready) {
        return true;
    }
    if (m_failed) {
        return false;
    }

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    format.setSamples(0);
    if (format.renderableType() == QSurfaceFormat::DefaultRenderableType) {
        format.setRenderableType(QSurfaceFormat::OpenGL);
    }

    m_surface = new QOffscreenSurface;
    m_surface->setFormat(format);
    m_surface->create();
    if (!m_surface->isValid()) {
        markFailed();
        return false;
    }

    m_context = new QOpenGLContext;
    m_context->setFormat(m_surface->format());
    if (!m_context->create() || !m_context->makeCurrent(m_surface)) {
        markFailed();
        return false;
    }

    m_gl = m_context->functions();
    if (!m_gl) {
        markFailed();
        return false;
    }
    m_gl->initializeOpenGLFunctions();

    const QString vertexSource = loadShaderSource(QStringLiteral(":/resources/shaders/qt_fallback_liquid_glass.vert"));
    const QString blurFragmentSource = loadShaderSource(QStringLiteral(":/resources/shaders/qt_fallback_liquid_glass_blur.frag"));
    const QString glassFragmentSource = loadShaderSource(QStringLiteral(":/resources/shaders/qt_fallback_liquid_glass.frag"));
    if (vertexSource.isEmpty() || blurFragmentSource.isEmpty() || glassFragmentSource.isEmpty()) {
        markFailed();
        return false;
    }

    m_blurProgram = makeProgram(vertexSource, blurFragmentSource);
    m_glassProgram = makeProgram(vertexSource, glassFragmentSource);
    if (!m_blurProgram || !m_glassProgram) {
        markFailed();
        return false;
    }

    m_gl->glDisable(GL_DEPTH_TEST);
    m_gl->glDisable(GL_STENCIL_TEST);
    m_gl->glDisable(GL_BLEND);
    m_ready = true;
    m_context->doneCurrent();
    return true;
}

void QtFallbackLiquidGlassRenderer::release()
{
    if (m_context && m_surface && m_context->makeCurrent(m_surface)) {
        m_blurProgram.reset();
        m_glassProgram.reset();
        m_context->doneCurrent();
    } else {
        m_blurProgram.reset();
        m_glassProgram.reset();
    }
    delete m_context;
    delete m_surface;
    m_context = nullptr;
    m_surface = nullptr;
    m_gl = nullptr;
    m_ready = false;
}

void QtFallbackLiquidGlassRenderer::markFailed()
{
    m_failed = true;
    release();
}

std::unique_ptr<QOpenGLShaderProgram> QtFallbackLiquidGlassRenderer::makeProgram(
        const QString& vertexSource,
        const QString& fragmentSource)
{
    auto program = std::make_unique<QOpenGLShaderProgram>();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource)) {
        return {};
    }
    if (!program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource)) {
        return {};
    }
    if (!program->link()) {
        return {};
    }
    return program;
}

unsigned int QtFallbackLiquidGlassRenderer::uploadTexture(const QImage& image)
{
    GLuint texture = 0;
    m_gl->glGenTextures(1, &texture);
    if (texture == 0) {
        return 0;
    }
    m_gl->glBindTexture(GL_TEXTURE_2D, texture);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_gl->glTexImage2D(GL_TEXTURE_2D,
                       0,
                       GL_RGBA,
                       image.width(),
                       image.height(),
                       0,
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       image.constBits());
    m_gl->glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void QtFallbackLiquidGlassRenderer::drawQuad(QOpenGLShaderProgram& program)
{
    static const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };

    const int positionLocation = program.attributeLocation("a_position");
    const int texCoordLocation = program.attributeLocation("a_texCoord");
    if (positionLocation < 0 || texCoordLocation < 0) {
        return;
    }
    program.enableAttributeArray(positionLocation);
    program.enableAttributeArray(texCoordLocation);
    program.setAttributeArray(positionLocation,
                              GL_FLOAT,
                              vertices,
                              2,
                              4 * static_cast<int>(sizeof(GLfloat)));
    program.setAttributeArray(texCoordLocation,
                              GL_FLOAT,
                              vertices + 2,
                              2,
                              4 * static_cast<int>(sizeof(GLfloat)));
    m_gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    program.disableAttributeArray(texCoordLocation);
    program.disableAttributeArray(positionLocation);
}

void QtFallbackLiquidGlassRenderer::bindTexture(unsigned int texture, unsigned int textureUnit)
{
    m_gl->glActiveTexture(textureUnit);
    m_gl->glBindTexture(GL_TEXTURE_2D, texture);
}

void QtFallbackLiquidGlassRenderer::prepareTarget(QOpenGLFramebufferObject& target)
{
    target.bind();
    m_gl->glViewport(0, 0, target.size().width(), target.size().height());
    m_gl->glDisable(GL_BLEND);
    m_gl->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    m_gl->glClear(GL_COLOR_BUFFER_BIT);
}

void QtFallbackLiquidGlassRenderer::renderBlurPass(unsigned int sourceTexture,
                                                   QOpenGLFramebufferObject& target,
                                                   const QVector2D& texelStep)
{
    prepareTarget(target);
    m_blurProgram->bind();
    bindTexture(sourceTexture, GL_TEXTURE0);
    m_blurProgram->setUniformValue("u_texture", 0);
    m_blurProgram->setUniformValue("u_texelStep", texelStep);
    drawQuad(*m_blurProgram);
    m_blurProgram->release();
    m_gl->glBindTexture(GL_TEXTURE_2D, 0);
    target.release();
}

void QtFallbackLiquidGlassRenderer::renderGlassPass(unsigned int blurredTexture,
                                                    QOpenGLFramebufferObject& target,
                                                    const QVector2D& size,
                                                    const QRectF& physicalShapeRect,
                                                    float radius,
                                                    float shapeInset,
                                                    float dark)
{
    prepareTarget(target);
    m_glassProgram->bind();
    bindTexture(blurredTexture, GL_TEXTURE0);
    m_glassProgram->setUniformValue("u_blurTexture", 0);
    m_glassProgram->setUniformValue("u_size", size);
    m_glassProgram->setUniformValue("u_shapeRect",
                                    QVector4D(static_cast<float>(physicalShapeRect.x()),
                                              static_cast<float>(physicalShapeRect.y()),
                                              static_cast<float>(physicalShapeRect.width()),
                                              static_cast<float>(physicalShapeRect.height())));
    m_glassProgram->setUniformValue("u_radius", radius);
    m_glassProgram->setUniformValue("u_shapeInset", shapeInset);
    m_glassProgram->setUniformValue("u_dark", dark);
    drawQuad(*m_glassProgram);
    m_glassProgram->release();
    m_gl->glActiveTexture(GL_TEXTURE0);
    m_gl->glBindTexture(GL_TEXTURE_2D, 0);
    target.release();
}

QtFallbackLiquidGlassController::QtFallbackLiquidGlassController(QWidget* targetWidget)
    : QObject(targetWidget)
    , m_targetWidget(targetWidget)
{
}

QtFallbackLiquidGlassController::~QtFallbackLiquidGlassController()
{
    release(false);
}

void QtFallbackLiquidGlassController::setSourceWidget(QWidget* widget)
{
    if (m_sourceWidget == widget) {
        return;
    }

    if (m_sourceWidget && m_sourceFilterInstalled) {
        m_sourceWidget->removeEventFilter(this);
    }
    m_sourceFilterInstalled = false;
    m_sourceWidget = widget;
    m_background = QImage();
    resetRefreshCache();
    if (m_enabled) {
        installSourceEventFilter();
        scheduleUpdate(0);
    }
    if (m_targetWidget) {
        m_targetWidget->update();
    }
}

void QtFallbackLiquidGlassController::setEffectMode(EffectMode mode)
{
    if (m_effectMode == mode) {
        return;
    }

    m_effectMode = mode;
    m_background = QImage();
    resetRefreshCache();
    if (m_effectMode == EffectMode::GaussianBlur) {
        m_renderer.reset();
    }
    scheduleUpdate(0);
    if (m_targetWidget) {
        m_targetWidget->update();
    }
}

void QtFallbackLiquidGlassController::setEnabled(bool enabled, bool updateWidget)
{
    if (m_enabled == enabled) {
        if (m_enabled) {
            installSourceEventFilter();
            scheduleUpdate(0);
        }
        return;
    }

    m_enabled = enabled;
    if (!m_enabled) {
        release(updateWidget);
        return;
    }

    installSourceEventFilter();
    scheduleUpdate(0);
    if (updateWidget && m_targetWidget) {
        m_targetWidget->update();
    }
}

bool QtFallbackLiquidGlassController::isEnabled() const
{
    return m_enabled;
}

void QtFallbackLiquidGlassController::setShape(const QRectF& rect, qreal cornerRadius)
{
    const QRectF normalized = rect.normalized();
    const qreal radius = qMax<qreal>(0.0, cornerRadius);
    if (m_shapeRect == normalized && qFuzzyCompare(m_cornerRadius, radius)) {
        return;
    }

    m_shapeRect = normalized;
    m_cornerRadius = radius;
    m_background = QImage();
    resetRefreshCache();
    if (m_renderer) {
        m_renderer->setShape(m_shapeRect, m_cornerRadius);
    }
    scheduleUpdate(0);
    if (m_targetWidget) {
        m_targetWidget->update();
    }
}

void QtFallbackLiquidGlassController::scheduleUpdate(int delayMs)
{
    scheduleUpdateInternal(delayMs, true);
}

void QtFallbackLiquidGlassController::scheduleUpdateInternal(int delayMs, bool force)
{
    if (!m_enabled || !m_targetWidget || !m_targetWidget->isVisible() || !m_sourceWidget) {
        return;
    }

    installSourceEventFilter();

    if (!m_updateTimer) {
        m_updateTimer = new QTimer(this);
        m_updateTimer->setSingleShot(true);
        connect(m_updateTimer, &QTimer::timeout,
                this, &QtFallbackLiquidGlassController::updateBackground);
    }

    int clampedDelay = qMax(0, delayMs);
    if (!force && !m_background.isNull()) {
        clampedDelay = qMax(clampedDelay, passiveRefreshDelayMs());
    }
    m_pendingUpdateForced = m_pendingUpdateForced || force;
    if (m_updateTimer->isActive()
        && m_updateTimer->remainingTime() <= clampedDelay) {
        return;
    }
    m_updateTimer->start(clampedDelay);
}

void QtFallbackLiquidGlassController::release(bool updateWidget)
{
    stopUpdateTimer();
    if (m_sourceWidget && m_sourceFilterInstalled) {
        m_sourceWidget->removeEventFilter(this);
    }
    m_sourceFilterInstalled = false;
    m_renderer.reset();
    m_fastGaussianBlur.reset();
    if (!m_background.isNull()) {
        m_background = QImage();
    }
    resetRefreshCache();
    if (updateWidget && m_targetWidget) {
        m_targetWidget->update();
    }
}

bool QtFallbackLiquidGlassController::handleHostEvent(QEvent* event)
{
    if (!m_enabled || !event) {
        return false;
    }

    switch (event->type()) {
    case QEvent::Show:
        installSourceEventFilter();
        scheduleUpdate(0);
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ParentChange:
    case QEvent::ZOrderChange:
        scheduleUpdate(0);
        break;
    case QEvent::Hide:
        release();
        break;
    default:
        break;
    }
    return false;
}

void QtFallbackLiquidGlassController::paint(QPainter& painter)
{
    if (!m_targetWidget) {
        return;
    }

    const bool darkMode = ThemeManager::instance().isDark();
    const bool gaussianBlur = m_effectMode == EffectMode::GaussianBlur;
    const qreal borderWidth = gaussianBlur ? 1.0 : liquidGlassBorderWidthForMode(darkMode);
    const QRectF shapeRect = m_shapeRect.isValid() ? m_shapeRect : targetRect();
    const QPainterPath borderPath = liquidGlassInsetPath(shapeRect,
                                                         m_cornerRadius,
                                                         borderWidth / 2.0);
    const QPainterPath contentPath = gaussianBlur
            ? borderPath
            : (darkMode
            ? borderPath
            : liquidGlassInsetPath(shapeRect,
                                   m_cornerRadius,
                                   liquidGlassContentInset(borderWidth, darkMode)));

    if (!m_background.isNull()) {
        painter.save();
        painter.setClipPath(contentPath);
        painter.drawImage(targetRect(), m_background);
        painter.restore();
    } else {
        QColor fallback = ThemeManager::instance().color(ThemeColor::PanelBackground);
        fallback.setAlpha(gaussianBlur ? (darkMode ? 96 : 76) : (darkMode ? 118 : 72));
        painter.fillPath(contentPath, fallback);
        scheduleUpdate(kLiquidGlassRefreshDelayMs);
    }

    QColor tint = gaussianBlur
            ? (darkMode ? QColor(24, 26, 30, 62) : QColor(255, 255, 255, 58))
            : (darkMode ? QColor(24, 26, 30, 68) : QColor(255, 255, 255, 60));
    painter.fillPath(contentPath, tint);

    if (darkMode && !gaussianBlur) {
        QLinearGradient sheen(shapeRect.bottomLeft(), shapeRect.topLeft());
        sheen.setColorAt(0.0, QColor(255, 255, 255, 16));
        sheen.setColorAt(0.42, QColor(255, 255, 255, 4));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.fillPath(contentPath, sheen);
    }

    QPen border(gaussianBlur
                ? (darkMode ? QColor(255, 255, 255, 40) : QColor(18, 22, 28, 54))
                : (darkMode
                ? QColor(255, 255, 255, 50)
                : QColor(18, 22, 28, 72)));
    border.setWidthF(borderWidth);
    border.setCapStyle(Qt::RoundCap);
    border.setJoinStyle(Qt::RoundJoin);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(borderPath);
}

int QtFallbackLiquidGlassController::refreshDelayMs()
{
    return kLiquidGlassRefreshDelayMs;
}

bool QtFallbackLiquidGlassController::liquidGlassSupported()
{
    return QtFallbackLiquidGlassRenderer::isSupported();
}

bool QtFallbackLiquidGlassController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_sourceWidget.data()) {
        if (m_captureInProgress) {
            return QObject::eventFilter(watched, event);
        }
        switch (event->type()) {
        case QEvent::Paint:
        case QEvent::Resize:
        case QEvent::Move:
        case QEvent::Show:
        case QEvent::UpdateRequest:
            scheduleUpdateInternal(0, false);
            break;
        case QEvent::Hide:
            stopUpdateTimer();
            m_background = QImage();
            resetRefreshCache();
            if (m_targetWidget) {
                m_targetWidget->update();
            }
            break;
        default:
            break;
        }
    }

    return QObject::eventFilter(watched, event);
}

void QtFallbackLiquidGlassController::installSourceEventFilter()
{
    if (!m_enabled || !m_sourceWidget || m_sourceFilterInstalled) {
        return;
    }
    m_sourceWidget->installEventFilter(this);
    m_sourceFilterInstalled = true;
}

void QtFallbackLiquidGlassController::stopUpdateTimer()
{
    if (!m_updateTimer) {
        return;
    }
    m_updateTimer->stop();
    delete m_updateTimer;
    m_updateTimer = nullptr;
}

void QtFallbackLiquidGlassController::resetRefreshCache()
{
    m_lastSourceSignature = 0;
    m_unchangedPassiveRefreshCount = 0;
    m_hasSourceSignature = false;
    m_pendingUpdateForced = false;
}

int QtFallbackLiquidGlassController::passiveRefreshDelayMs() const
{
    if (m_unchangedPassiveRefreshCount <= 0) {
        return kLiquidGlassRefreshDelayMs;
    }
    if (m_unchangedPassiveRefreshCount == 1) {
        return kLiquidGlassPassiveRefreshDelayMs;
    }
    if (m_unchangedPassiveRefreshCount == 2) {
        return kLiquidGlassPassiveRefreshDelayMs * 2;
    }
    return kLiquidGlassMaxPassiveRefreshDelayMs;
}

void QtFallbackLiquidGlassController::updateBackground()
{
    if (!m_enabled || !m_targetWidget || !m_targetWidget->isVisible()) {
        release();
        return;
    }

    const bool forcedUpdate = m_pendingUpdateForced;
    m_pendingUpdateForced = false;
    const qreal devicePixelRatio = qMax<qreal>(m_targetWidget->devicePixelRatioF(), 1.0);
    QScopedValueRollback<bool> captureGuard(m_captureInProgress, true);
    QImage source = captureSource(devicePixelRatio);
    if (source.isNull()) {
        m_background = QImage();
        resetRefreshCache();
        m_targetWidget->update();
        return;
    }

    const quint64 sourceSignature = liquidGlassSourceSignature(source);
    if (!m_background.isNull()
        && m_hasSourceSignature
        && m_lastSourceSignature == sourceSignature) {
        if (!forcedUpdate) {
            ++m_unchangedPassiveRefreshCount;
        }
        return;
    }
    m_lastSourceSignature = sourceSignature;
    m_hasSourceSignature = true;
    m_unchangedPassiveRefreshCount = 0;

    const QSize scaledSize(qMax(1, qRound(source.width() * kLiquidGlassRenderScale)),
                           qMax(1, qRound(source.height() * kLiquidGlassRenderScale)));
    QImage scaledSource = source.scaled(scaledSize,
                                        Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
    scaledSource.setDevicePixelRatio(devicePixelRatio * kLiquidGlassRenderScale);
    m_background = renderBackground(scaledSource, devicePixelRatio);
    m_targetWidget->update();
}

QImage QtFallbackLiquidGlassController::captureSource(qreal devicePixelRatio) const
{
    if (!m_targetWidget || !m_sourceWidget
        || m_targetWidget->width() <= 0 || m_targetWidget->height() <= 0) {
        return {};
    }

    const QSize logicalSize = m_targetWidget->size();
    const QSize imageSize(qMax(1, qRound(logicalSize.width() * devicePixelRatio)),
                          qMax(1, qRound(logicalSize.height() * devicePixelRatio)));
    QImage source(imageSize, QImage::Format_ARGB32_Premultiplied);
    source.setDevicePixelRatio(devicePixelRatio);
    source.fill(ThemeManager::instance().color(ThemeColor::PageBackground));

    const QPoint sourceTopLeft = m_sourceWidget->mapFromGlobal(m_targetWidget->mapToGlobal(QPoint(0, 0)));
    const QRect sourceRect(sourceTopLeft, logicalSize);
    const QRect clippedSourceRect = sourceRect.intersected(m_sourceWidget->rect());
    if (clippedSourceRect.isEmpty()) {
        return source;
    }

    QPainter painter(&source);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QPixmap sourcePixmap = m_sourceWidget->grab(clippedSourceRect);
    painter.drawPixmap(clippedSourceRect.topLeft() - sourceTopLeft, sourcePixmap);
    return source;
}

QImage QtFallbackLiquidGlassController::renderBackground(const QImage& source, qreal devicePixelRatio)
{
    if (source.isNull()) {
        return {};
    }

    if (m_effectMode == EffectMode::GaussianBlur) {
        return renderBackgroundWithFastBlur(source, devicePixelRatio);
    }

    if (!liquidGlassSupported()) {
        return {};
    }

    if (!m_renderer) {
        m_renderer = std::make_unique<QtFallbackLiquidGlassRenderer>();
    }
    m_renderer->setShape(m_shapeRect, m_cornerRadius);

    QImage rendered = m_renderer->render(source,
                                         devicePixelRatio,
                                         ThemeManager::instance().isDark());
    if (!rendered.isNull()) {
        return rendered;
    }

    m_renderer.reset();
    return {};
}

QImage QtFallbackLiquidGlassController::renderBackgroundWithFastBlur(const QImage& source,
                                                                     qreal devicePixelRatio)
{
    if (!m_fastGaussianBlur) {
        m_fastGaussianBlur = std::make_unique<FastGaussianBlur>();
    }

    QImage blurred = m_fastGaussianBlur->blur(
            source,
            kLiquidGlassQtBlurRadius * kLiquidGlassRenderScale * devicePixelRatio);
    if (!blurred.isNull()) {
        blurred.setDevicePixelRatio(devicePixelRatio * kLiquidGlassRenderScale);
    }
    return blurred;
}

QRectF QtFallbackLiquidGlassController::targetRect() const
{
    return m_targetWidget
            ? QRectF(QPointF(0, 0), QSizeF(m_targetWidget->size()))
            : QRectF();
}
