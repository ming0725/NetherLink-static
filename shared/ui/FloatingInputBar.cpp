#include "FloatingInputBar.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/TransparentTextEdit.h"

#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QScrollBar>
#include <QScopedValueRollback>
#include <QSurfaceFormat>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTextOption>
#include <QTimer>
#include <QVector2D>
#include <QtMath>

#ifdef Q_OS_MACOS
#include "platform/macos/MacFloatingInputBarBridge_p.h"
#endif

namespace {

constexpr int kQtMode3ToolbarY = 12;
constexpr int kQtMode3ToolbarInputGap = 6;
constexpr int kQtMode3InputTextTopPadding = 1;
constexpr int kQtMode3InputTextHorizontalPadding = 5;
constexpr int kQtMode3InputTextBottomPadding = 5;
constexpr int kQtMode3InputRightPadding = 5;
constexpr int kQtMode3InputBottomMargin = 10;
constexpr int kQtMode3InputMaxHeight = 120;
constexpr qreal kLiquidGlassRenderScale = 0.82;
constexpr qreal kLiquidGlassBorderWidth = 1.0;
constexpr qreal kLiquidGlassLightBorderWidth = 1.0;
constexpr float kLiquidGlassBlurStep = 1.7f;
constexpr int kLiquidGlassRefreshDelayMs = 24;
constexpr int kLiquidGlassQtBlurRadius = 18;
constexpr auto kSystemFloatingBarsSuppressedProperty = "systemFloatingBarsSuppressed";
constexpr auto kNormalIconProperty = "normalIcon";
constexpr auto kHoveredIconProperty = "hoveredIcon";

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

} // namespace

class QtFallbackLiquidGlassRenderer
{
public:
    ~QtFallbackLiquidGlassRenderer()
    {
        release();
    }

    QImage render(const QImage& source, qreal devicePixelRatio, bool dark, qreal cornerRadius)
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
                const bool darkMode = dark;
                const qreal borderWidth = liquidGlassBorderWidthForMode(darkMode);
                const qreal contentInset = liquidGlassContentInset(borderWidth, darkMode);
                renderGlassPass(verticalFbo.texture(),
                                horizontalFbo,
                                QVector2D(static_cast<float>(renderSize.width()),
                                          static_cast<float>(renderSize.height())),
                                static_cast<float>(liquidGlassInsetRadius(cornerRadius, contentInset)
                                                   * kLiquidGlassRenderScale
                                                   * devicePixelRatio),
                                static_cast<float>(contentInset
                                                   * kLiquidGlassRenderScale
                                                   * devicePixelRatio),
                                dark ? 1.0f : 0.0f);

                // The shader pipeline already keeps the sampled Qt image in widget coordinates.
                output = horizontalFbo.toImage(false).convertToFormat(QImage::Format_ARGB32_Premultiplied);
                output.setDevicePixelRatio(devicePixelRatio * kLiquidGlassRenderScale);
            }
            m_gl->glDeleteTextures(1, &sourceTexture);
        }
        m_context->doneCurrent();
        return output;
    }

private:
    bool ensureReady()
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

        const QString vertexSource = loadShaderSource(QStringLiteral(":/resources/shaders/floating_input_liquid_glass.vert"));
        const QString blurFragmentSource = loadShaderSource(QStringLiteral(":/resources/shaders/floating_input_blur.frag"));
        const QString glassFragmentSource = loadShaderSource(QStringLiteral(":/resources/shaders/floating_input_liquid_glass.frag"));
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

    void release()
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

    void markFailed()
    {
        m_failed = true;
        release();
    }

    std::unique_ptr<QOpenGLShaderProgram> makeProgram(const QString& vertexSource,
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

    GLuint uploadTexture(const QImage& image)
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

    void drawQuad(QOpenGLShaderProgram& program)
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

    void bindTexture(GLuint texture, GLenum textureUnit = GL_TEXTURE0)
    {
        m_gl->glActiveTexture(textureUnit);
        m_gl->glBindTexture(GL_TEXTURE_2D, texture);
    }

    void prepareTarget(QOpenGLFramebufferObject& target)
    {
        target.bind();
        m_gl->glViewport(0, 0, target.size().width(), target.size().height());
        m_gl->glDisable(GL_BLEND);
        m_gl->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        m_gl->glClear(GL_COLOR_BUFFER_BIT);
    }

    void renderBlurPass(GLuint sourceTexture,
                        QOpenGLFramebufferObject& target,
                        const QVector2D& texelStep)
    {
        prepareTarget(target);
        m_blurProgram->bind();
        bindTexture(sourceTexture);
        m_blurProgram->setUniformValue("u_texture", 0);
        m_blurProgram->setUniformValue("u_texelStep", texelStep);
        drawQuad(*m_blurProgram);
        m_blurProgram->release();
        m_gl->glBindTexture(GL_TEXTURE_2D, 0);
        target.release();
    }

    void renderGlassPass(GLuint blurredTexture,
                         QOpenGLFramebufferObject& target,
                         const QVector2D& size,
                         float radius,
                         float shapeInset,
                         float dark)
    {
        prepareTarget(target);
        m_glassProgram->bind();
        bindTexture(blurredTexture, GL_TEXTURE0);
        m_glassProgram->setUniformValue("u_blurTexture", 0);
        m_glassProgram->setUniformValue("u_size", size);
        m_glassProgram->setUniformValue("u_radius", radius);
        m_glassProgram->setUniformValue("u_shapeInset", shapeInset);
        m_glassProgram->setUniformValue("u_dark", dark);
        drawQuad(*m_glassProgram);
        m_glassProgram->release();
        m_gl->glActiveTexture(GL_TEXTURE0);
        m_gl->glBindTexture(GL_TEXTURE_2D, 0);
        target.release();
    }

    QOffscreenSurface* m_surface = nullptr;
    QOpenGLContext* m_context = nullptr;
    QOpenGLFunctions* m_gl = nullptr;
    std::unique_ptr<QOpenGLShaderProgram> m_blurProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_glassProgram;
    bool m_ready = false;
    bool m_failed = false;
};

FloatingInputBar::FloatingInputBar(QWidget *parent)
        : QWidget(parent)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);
    setFocusPolicy(Qt::StrongFocus);

#ifdef Q_OS_MACOS
    const auto nativeAppearance = MacFloatingInputBarBridge::appearance();
    m_usesNativeGlass = nativeAppearance != MacFloatingInputBarBridge::Appearance::Unsupported;
    m_usesNativeInput = m_usesNativeGlass;
#endif

    if (!m_usesNativeInput) {
        initQtFallbackUi();
    }
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &FloatingInputBar::applyTheme);

    updateFloatingPanelShadow();
}

FloatingInputBar::~FloatingInputBar()
{
    releaseQtFallbackLiquidGlassResources(false);
#ifdef Q_OS_MACOS
    if (m_usesNativeGlass) {
        MacFloatingInputBarBridge::clearInputBar(this);
    }
#endif
}

void FloatingInputBar::focusInput()
{
    if (m_inputEdit) {
        m_inputEdit->setFocus();
        m_inputEdit->moveCursor(QTextCursor::End);
        return;
    }

#ifdef Q_OS_MACOS
    if (m_usesNativeInput) {
        MacFloatingInputBarBridge::focusInputBar(this);
    }
#endif
}

bool FloatingInputBar::usesQtFallbackLiquidGlass() const
{
    return shouldUseQtFallbackLiquidGlass();
}

void FloatingInputBar::setLiquidGlassSourceWidget(QWidget* widget)
{
    if (m_liquidGlassSourceWidget == widget) {
        return;
    }

    if (m_liquidGlassSourceWidget && m_liquidGlassSourceFilterInstalled) {
        m_liquidGlassSourceWidget->removeEventFilter(this);
    }
    m_liquidGlassSourceFilterInstalled = false;
    m_liquidGlassSourceWidget = widget;
    m_liquidGlassBackground = QImage();
    updateQtFallbackLiquidGlassState();
}

void FloatingInputBar::scheduleLiquidGlassUpdate(int delayMs)
{
    if (!shouldUseQtFallbackLiquidGlass() || !isVisible() || !m_liquidGlassSourceWidget) {
        return;
    }

    if (!m_liquidGlassUpdateTimer) {
        m_liquidGlassUpdateTimer = new QTimer(this);
        m_liquidGlassUpdateTimer->setSingleShot(true);
        connect(m_liquidGlassUpdateTimer, &QTimer::timeout,
                this, &FloatingInputBar::updateLiquidGlassBackground);
    }

    const int clampedDelay = qMax(0, delayMs);
    if (m_liquidGlassUpdateTimer->isActive()
        && m_liquidGlassUpdateTimer->remainingTime() <= clampedDelay) {
        return;
    }
    m_liquidGlassUpdateTimer->start(clampedDelay);
}

void FloatingInputBar::initQtFallbackUi()
{
    m_emojiLabel = new QLabel(this);
    m_imageLabel = new QLabel(this);
    m_screenshotLabel = new QLabel(this);
    m_historyLabel = new QLabel(this);

    const QSize buttonSize(BUTTON_SIZE, BUTTON_SIZE);
    m_emojiLabel->setFixedSize(buttonSize);
    m_imageLabel->setFixedSize(buttonSize);
    m_screenshotLabel->setFixedSize(buttonSize);
    m_historyLabel->setFixedSize(buttonSize);
    m_emojiLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_screenshotLabel->setAlignment(Qt::AlignCenter);
    m_historyLabel->setAlignment(Qt::AlignCenter);

    const QSize iconSize(ICON_SIZE, ICON_SIZE);
    updateLabelIcon(m_emojiLabel,
                    QStringLiteral(":/resources/icon/skull.png"),
                    QStringLiteral(":/resources/icon/hovered_skull.png"),
                    iconSize);
    updateLabelIcon(m_imageLabel,
                    QStringLiteral(":/resources/icon/painting.png"),
                    QStringLiteral(":/resources/icon/hovered_painting.png"),
                    iconSize);
    updateLabelIcon(m_screenshotLabel,
                    QStringLiteral(":/resources/icon/shears.png"),
                    QStringLiteral(":/resources/icon/hovered_shears.png"),
                    iconSize);
    updateLabelIcon(m_historyLabel,
                    QStringLiteral(":/resources/icon/clock.png"),
                    QStringLiteral(":/resources/icon/hovered_clock.png"),
                    iconSize);

    const QList<QLabel*> toolbarLabels {m_emojiLabel, m_imageLabel, m_screenshotLabel, m_historyLabel};
    for (QLabel* label : toolbarLabels) {
        label->setCursor(Qt::PointingHandCursor);
        label->installEventFilter(this);
    }

    m_inputEdit = new TransparentTextEdit(this);
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setMinimumHeight(60);
    m_inputEdit->setMaximumHeight(120);
    m_inputEdit->setViewportPadding(kQtMode3InputTextHorizontalPadding,
                                    kQtMode3InputTextTopPadding,
                                    kQtMode3InputTextHorizontalPadding,
                                    kQtMode3InputTextBottomPadding);
    m_inputEdit->installEventFilter(this);
    setFocusProxy(m_inputEdit);

    QFont font = m_inputEdit->font();
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    m_inputEdit->setFont(font);

    m_sendLabel = new QLabel(this);
    m_sendLabel->setFixedSize(SEND_BUTTON_SIZE, SEND_BUTTON_SIZE);
    m_sendLabel->setCursor(Qt::PointingHandCursor);
    m_sendLabel->setAlignment(Qt::AlignCenter);
    updateLabelIcon(m_sendLabel,
                    QStringLiteral(":/resources/icon/book_and_pen.png"),
                    QStringLiteral(":/resources/icon/hovered_book_and_pen.png"),
                    QSize(SEND_BUTTON_SIZE, SEND_BUTTON_SIZE));
    m_sendLabel->installEventFilter(this);
    m_sendLabel->raise();
    updateQtFallbackGeometry();

    m_tooltip = new CustomTooltip(this);
    m_tooltip->hide();
    applyTheme();
}

void FloatingInputBar::clearQtFallbackUi()
{
    releaseQtFallbackLiquidGlassResources(false);
    hideTooltip();

    setFocusProxy(nullptr);

    delete m_inputEdit;
    delete m_emojiLabel;
    delete m_imageLabel;
    delete m_screenshotLabel;
    delete m_historyLabel;
    delete m_sendLabel;
    delete m_tooltip;

    m_inputEdit = nullptr;
    m_emojiLabel = nullptr;
    m_imageLabel = nullptr;
    m_screenshotLabel = nullptr;
    m_historyLabel = nullptr;
    m_sendLabel = nullptr;
    m_tooltip = nullptr;
}

void FloatingInputBar::refreshPlatformAppearance()
{
#ifdef Q_OS_MACOS
    const bool shouldUseNative = MacFloatingInputBarBridge::appearance()
            != MacFloatingInputBarBridge::Appearance::Unsupported;
    const bool systemSuppressed = property(kSystemFloatingBarsSuppressedProperty).toBool();

    if (m_usesNativeGlass && !shouldUseNative) {
        MacFloatingInputBarBridge::clearInputBar(this);
        m_usesNativeGlass = false;
        m_usesNativeInput = false;
        if (!m_inputEdit) {
            initQtFallbackUi();
        }
        updateQtFallbackGeometry();
        updateQtFallbackLiquidGlassState();
        updateGeometry();
        update();
        return;
    }

    if (!m_usesNativeGlass && shouldUseNative && systemSuppressed) {
        updateQtFallbackGeometry();
        updateQtFallbackLiquidGlassState();
        update();
        return;
    }

    if (!m_usesNativeGlass && shouldUseNative) {
        releaseQtFallbackLiquidGlassResources(false);
        clearQtFallbackUi();
        m_usesNativeGlass = true;
        m_usesNativeInput = true;
        syncPlatformInput();
        updateGeometry();
        update();
        return;
    }

    if (m_usesNativeGlass) {
        syncPlatformInput();
    }
#endif

    updateQtFallbackGeometry();
    updateQtFallbackLiquidGlassState();
    update();
}

void FloatingInputBar::applyTheme()
{
#ifdef Q_OS_MACOS
    if (m_usesNativeGlass) {
        syncPlatformInput();
    }
#endif

    if (m_inputEdit) {
        QPalette inputPalette = m_inputEdit->palette();
        inputPalette.setColor(QPalette::Base, Qt::transparent);
        inputPalette.setColor(QPalette::Window, Qt::transparent);
        inputPalette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
        inputPalette.setColor(QPalette::PlaceholderText,
                              ThemeManager::instance().color(ThemeColor::PlaceholderText));
        inputPalette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::Accent));
        inputPalette.setColor(QPalette::HighlightedText, ThemeManager::instance().color(ThemeColor::TextOnAccent));
        m_inputEdit->setPalette(inputPalette);
        m_inputEdit->update();
    }

    updateQtFallbackLiquidGlassState();
    update();
}

void FloatingInputBar::updateFloatingPanelShadow()
{
    if (shouldUseQtFallbackLiquidGlass()) {
        if (graphicsEffect()) {
            setGraphicsEffect(nullptr);
        }
        return;
    }

    auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect());
    if (!shadow) {
        shadow = new QGraphicsDropShadowEffect(this);
        setGraphicsEffect(shadow);
    }

    shadow->setBlurRadius(30);
    shadow->setOffset(0, 0);
    shadow->setColor(ThemeManager::instance().color(ThemeColor::FloatingPanelShadow));
}

void FloatingInputBar::updateLabelIcon(QLabel *label,
                                       const QString &normalIcon,
                                       const QString &hoveredIcon,
                                       const QSize &size)
{
    if (!label) {
        return;
    }

    const qreal dpr = label->devicePixelRatioF();
    label->setProperty(kNormalIconProperty, normalIcon);
    label->setProperty(kHoveredIconProperty, hoveredIcon);
    label->setProperty("iconSize", size);
    label->setPixmap(ImageService::instance().scaled(normalIcon,
                                                     size,
                                                     Qt::KeepAspectRatio,
                                                     dpr));
}

void FloatingInputBar::showTooltip(QLabel *label, const QString &text)
{
    if (!m_tooltip || !label) {
        return;
    }

    m_tooltip->setText(text);
    const QPoint labelPos = label->mapToGlobal(QPoint(0, 0));
    const QPoint tooltipPos(labelPos.x() + label->width() / 2 - m_tooltip->width() / 2,
                            labelPos.y());
    m_tooltip->showTooltip(tooltipPos);
}

void FloatingInputBar::hideTooltip()
{
    if (m_tooltip) {
        m_tooltip->hide();
    }
}

bool FloatingInputBar::submitNativeText(const QString& text)
{
    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty()) {
        return false;
    }

    emit sendText(trimmedText);
    return true;
}

void FloatingInputBar::triggerNativeHelloShortcut()
{
    sendHelloWorld();
}

void FloatingInputBar::triggerNativeImageShortcut()
{
    chooseAndSendImage();
}

bool FloatingInputBar::event(QEvent *event)
{
    const bool handled = QWidget::event(event);

#ifdef Q_OS_MACOS
    if (m_usesNativeGlass) {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::WinIdChange:
        case QEvent::ParentChange:
        case QEvent::ZOrderChange:
            syncPlatformInput();
            break;
        case QEvent::Hide:
            MacFloatingInputBarBridge::clearInputBar(this);
            break;
        default:
            break;
        }
        return handled;
    }
#endif

    switch (event->type()) {
    case QEvent::Show:
        updateQtFallbackLiquidGlassState();
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ParentChange:
    case QEvent::ZOrderChange:
        scheduleLiquidGlassUpdate(kLiquidGlassRefreshDelayMs);
        break;
    case QEvent::Hide:
        releaseQtFallbackLiquidGlassResources();
        break;
    default:
        break;
    }

    return handled;
}

void FloatingInputBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (m_usesNativeGlass) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(m_visualOpacity);
    const QRectF panelRect(rect());

    if (shouldUseQtFallbackLiquidGlass()) {
        paintQtFallbackLiquidGlass(painter, panelRect);
        return;
    }

    QPainterPath path;
    path.addRoundedRect(panelRect, CORNER_RADIUS, CORNER_RADIUS);
    QColor background = ThemeManager::instance().color(ThemeColor::PanelBackground);
    background.setAlpha(ThemeManager::instance().isDark() ? 190 : 205);
    painter.fillPath(path, background);
}

void FloatingInputBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateQtFallbackGeometry();
    scheduleLiquidGlassUpdate(0);
}

bool FloatingInputBar::shouldUseQtFallbackLiquidGlass() const
{
    return !m_usesNativeGlass
            && ThemeManager::instance().qtFallbackLiquidGlassEnabled();
}

void FloatingInputBar::updateQtFallbackLiquidGlassState()
{
    updateFloatingPanelShadow();

    if (!shouldUseQtFallbackLiquidGlass() || !isVisible()) {
        releaseQtFallbackLiquidGlassResources();
        return;
    }

    if (m_liquidGlassSourceWidget && !m_liquidGlassSourceFilterInstalled) {
        m_liquidGlassSourceWidget->installEventFilter(this);
        m_liquidGlassSourceFilterInstalled = true;
    }
    scheduleLiquidGlassUpdate(0);
}

void FloatingInputBar::releaseQtFallbackLiquidGlassResources(bool updateWidget)
{
    if (m_liquidGlassUpdateTimer) {
        m_liquidGlassUpdateTimer->stop();
        delete m_liquidGlassUpdateTimer;
        m_liquidGlassUpdateTimer = nullptr;
    }
    if (m_liquidGlassSourceWidget && m_liquidGlassSourceFilterInstalled) {
        m_liquidGlassSourceWidget->removeEventFilter(this);
    }
    m_liquidGlassSourceFilterInstalled = false;
    m_liquidGlassRenderer.reset();
    if (!m_liquidGlassBackground.isNull()) {
        m_liquidGlassBackground = QImage();
    }
    if (updateWidget) {
        update();
    }
}

QImage FloatingInputBar::captureLiquidGlassSource(qreal devicePixelRatio) const
{
    if (!m_liquidGlassSourceWidget || width() <= 0 || height() <= 0) {
        return {};
    }

    const QSize logicalSize = size();
    const QSize imageSize(qMax(1, qRound(logicalSize.width() * devicePixelRatio)),
                          qMax(1, qRound(logicalSize.height() * devicePixelRatio)));
    QImage source(imageSize, QImage::Format_ARGB32_Premultiplied);
    source.setDevicePixelRatio(devicePixelRatio);
    source.fill(ThemeManager::instance().color(ThemeColor::PageBackground));

    const QPoint sourceTopLeft = m_liquidGlassSourceWidget->mapFromGlobal(mapToGlobal(QPoint(0, 0)));
    const QRect sourceRect(sourceTopLeft, logicalSize);
    const QRect clippedSourceRect = sourceRect.intersected(m_liquidGlassSourceWidget->rect());
    if (clippedSourceRect.isEmpty()) {
        return source;
    }

    QPainter painter(&source);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QPixmap sourcePixmap = m_liquidGlassSourceWidget->grab(clippedSourceRect);
    painter.drawPixmap(clippedSourceRect.topLeft() - sourceTopLeft, sourcePixmap);
    return source;
}

QImage FloatingInputBar::renderLiquidGlassBackground(const QImage& source, qreal devicePixelRatio)
{
    if (source.isNull()) {
        return {};
    }

    if (!m_liquidGlassRenderer) {
        m_liquidGlassRenderer = std::make_unique<QtFallbackLiquidGlassRenderer>();
    }

    QImage rendered = m_liquidGlassRenderer->render(source,
                                                    devicePixelRatio,
                                                    ThemeManager::instance().isDark(),
                                                    CORNER_RADIUS);
    if (!rendered.isNull()) {
        return rendered;
    }

    m_liquidGlassRenderer.reset();
    return renderLiquidGlassBackgroundWithQtBlur(source, devicePixelRatio);
}

QImage FloatingInputBar::renderLiquidGlassBackgroundWithQtBlur(const QImage& source,
                                                              qreal devicePixelRatio) const
{
    QImage blurSource = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    blurSource.setDevicePixelRatio(1.0);

    QGraphicsScene scene;
    scene.setSceneRect(QRectF(QPointF(0, 0), QSizeF(blurSource.size())));
    QGraphicsPixmapItem* item = scene.addPixmap(QPixmap::fromImage(blurSource));
    auto* effect = new QGraphicsBlurEffect;
    effect->setBlurRadius(kLiquidGlassQtBlurRadius * kLiquidGlassRenderScale * devicePixelRatio);
    effect->setBlurHints(QGraphicsBlurEffect::PerformanceHint);
    item->setGraphicsEffect(effect);

    QImage blurred(blurSource.size(), QImage::Format_ARGB32_Premultiplied);
    blurred.fill(Qt::transparent);
    QPainter painter(&blurred);
    scene.render(&painter,
                 QRectF(QPointF(0, 0), QSizeF(blurred.size())),
                 scene.sceneRect());
    painter.end();

    blurred.setDevicePixelRatio(devicePixelRatio * kLiquidGlassRenderScale);
    return blurred;
}

void FloatingInputBar::updateLiquidGlassBackground()
{
    if (!shouldUseQtFallbackLiquidGlass() || !isVisible()) {
        releaseQtFallbackLiquidGlassResources();
        return;
    }

    const qreal devicePixelRatio = qMax<qreal>(devicePixelRatioF(), 1.0);
    QScopedValueRollback<bool> captureGuard(m_liquidGlassCaptureInProgress, true);
    QImage source = captureLiquidGlassSource(devicePixelRatio);
    if (source.isNull()) {
        m_liquidGlassBackground = QImage();
        update();
        return;
    }

    const QSize scaledSize(qMax(1, qRound(source.width() * kLiquidGlassRenderScale)),
                           qMax(1, qRound(source.height() * kLiquidGlassRenderScale)));
    QImage scaledSource = source.scaled(scaledSize,
                                        Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
    scaledSource.setDevicePixelRatio(devicePixelRatio * kLiquidGlassRenderScale);
    m_liquidGlassBackground = renderLiquidGlassBackground(scaledSource, devicePixelRatio);
    update();
}

void FloatingInputBar::paintQtFallbackLiquidGlass(QPainter& painter, const QRectF& panelRect)
{
    const bool darkMode = ThemeManager::instance().isDark();
    const qreal borderWidth = liquidGlassBorderWidthForMode(darkMode);
    const QPainterPath borderPath = liquidGlassInsetPath(panelRect,
                                                         CORNER_RADIUS,
                                                         borderWidth / 2.0);
    const QPainterPath contentPath = darkMode
            ? borderPath
            : liquidGlassInsetPath(panelRect,
                                   CORNER_RADIUS,
                                   liquidGlassContentInset(borderWidth, darkMode));

    if (!m_liquidGlassBackground.isNull()) {
        painter.save();
        painter.setClipPath(contentPath);
        painter.drawImage(panelRect, m_liquidGlassBackground);
        painter.restore();
    } else {
        QColor fallback = ThemeManager::instance().color(ThemeColor::PanelBackground);
        fallback.setAlpha(ThemeManager::instance().isDark() ? 118 : 72);
        painter.fillPath(contentPath, fallback);
        scheduleLiquidGlassUpdate(kLiquidGlassRefreshDelayMs);
    }

    QColor tint = ThemeManager::instance().isDark()
            ? QColor(24, 26, 30, 68)
            : QColor(255, 255, 255, 60);
    painter.fillPath(contentPath, tint);

    if (darkMode) {
        QLinearGradient sheen(panelRect.bottomLeft(), panelRect.topLeft());
        sheen.setColorAt(0.0, QColor(255, 255, 255, 16));
        sheen.setColorAt(0.42, QColor(255, 255, 255, 4));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.fillPath(contentPath, sheen);
    }

    QPen border(darkMode
                ? QColor(255, 255, 255, 50)
                : QColor(18, 22, 28, 72));
    border.setWidthF(borderWidth);
    border.setCapStyle(Qt::RoundCap);
    border.setJoinStyle(Qt::RoundJoin);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(borderPath);
}

void FloatingInputBar::updateQtFallbackGeometry()
{
    if (!m_inputEdit) {
        return;
    }

    const int toolbarY = kQtMode3ToolbarY;
    const int emojiX = TOOLBAR_LEFT_MARGIN;
    const int imageX = emojiX + BUTTON_SIZE + BUTTON_SPACING;
    const int screenshotX = imageX + BUTTON_SIZE + BUTTON_SPACING;
    const int historyCandidateX = width() - RIGHT_MARGIN - 4 - BUTTON_SIZE;
    const int historyX = historyCandidateX > TOOLBAR_LEFT_MARGIN
            ? historyCandidateX
            : TOOLBAR_LEFT_MARGIN;
    const int inputY = toolbarY + BUTTON_SIZE + kQtMode3ToolbarInputGap;
    const int rawInputHeight = height() - inputY - kQtMode3InputBottomMargin;
    const int availableInputHeight = rawInputHeight > 0 ? rawInputHeight : 0;
    const int inputHeight = availableInputHeight < kQtMode3InputMaxHeight
            ? availableInputHeight
            : kQtMode3InputMaxHeight;
    const int rawInputWidth = width() - INPUT_LEFT_MARGIN - kQtMode3InputRightPadding;
    const int inputWidth = rawInputWidth > 0 ? rawInputWidth : 0;

    m_emojiLabel->setGeometry(emojiX, toolbarY, BUTTON_SIZE, BUTTON_SIZE);
    m_imageLabel->setGeometry(imageX, toolbarY, BUTTON_SIZE, BUTTON_SIZE);
    m_screenshotLabel->setGeometry(screenshotX, toolbarY, BUTTON_SIZE, BUTTON_SIZE);
    m_historyLabel->setGeometry(historyX, toolbarY, BUTTON_SIZE, BUTTON_SIZE);
    m_inputEdit->setGeometry(INPUT_LEFT_MARGIN, inputY, inputWidth, inputHeight);
    updateSendButtonPosition();
}

void FloatingInputBar::updateSendButtonPosition()
{
    if (m_sendLabel) {
        m_sendLabel->setGeometry(width() - SEND_BUTTON_SIZE - RIGHT_MARGIN,
                                 height() - SEND_BUTTON_SIZE - VERTICAL_MARGIN,
                                 SEND_BUTTON_SIZE,
                                 SEND_BUTTON_SIZE);
        m_sendLabel->raise();
    }
}

bool FloatingInputBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_liquidGlassSourceWidget.data()) {
        if (m_liquidGlassCaptureInProgress) {
            return QWidget::eventFilter(watched, event);
        }
        switch (event->type()) {
        case QEvent::Paint:
        case QEvent::Resize:
        case QEvent::Move:
        case QEvent::Show:
        case QEvent::UpdateRequest:
            scheduleLiquidGlassUpdate(kLiquidGlassRefreshDelayMs);
            break;
        case QEvent::Hide:
            if (m_liquidGlassUpdateTimer) {
                m_liquidGlassUpdateTimer->stop();
            }
            m_liquidGlassBackground = QImage();
            update();
            break;
        default:
            break;
        }
    }

    if (watched == m_inputEdit) {
        if (event->type() == QEvent::FocusIn) {
            emit inputFocused();
        } else if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_BracketLeft && keyEvent->modifiers() == Qt::NoModifier) {
                sendCurrentMessage(SendMode::Peer);
                return true;
            }
            if (keyEvent->key() == Qt::Key_BracketRight && keyEvent->modifiers() == Qt::NoModifier) {
                sendHelloWorld();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backslash && keyEvent->modifiers() == Qt::NoModifier) {
                chooseAndSendImage();
                return true;
            }
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                && keyEvent->modifiers() == Qt::NoModifier) {
                sendCurrentMessage();
                return true;
            }
        }
    }

    QLabel* label = qobject_cast<QLabel*>(watched);
    if (label) {
        if (event->type() == QEvent::Enter) {
            const QString hoveredIcon = label->property(kHoveredIconProperty).toString();
            const QSize iconSize = label->property("iconSize").toSize();
            label->setPixmap(ImageService::instance().scaled(hoveredIcon,
                                                             iconSize,
                                                             Qt::KeepAspectRatio,
                                                             label->devicePixelRatioF()));

            if (label == m_emojiLabel) {
                showTooltip(label, QStringLiteral("表情"));
            } else if (label == m_imageLabel) {
                showTooltip(label, QStringLiteral("图片"));
            } else if (label == m_screenshotLabel) {
                showTooltip(label, QStringLiteral("截图"));
            } else if (label == m_historyLabel) {
                showTooltip(label, QStringLiteral("历史"));
            } else if (label == m_sendLabel) {
                showTooltip(label, QStringLiteral("发送"));
            }
        } else if (event->type() == QEvent::Leave) {
            const QString normalIcon = label->property(kNormalIconProperty).toString();
            const QSize iconSize = label->property("iconSize").toSize();
            label->setPixmap(ImageService::instance().scaled(normalIcon,
                                                             iconSize,
                                                             Qt::KeepAspectRatio,
                                                             label->devicePixelRatioF()));
            hideTooltip();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void FloatingInputBar::mousePressEvent(QMouseEvent *event)
{
    if (m_sendLabel && m_sendLabel->geometry().contains(event->pos())) {
        sendCurrentMessage();
    } else if (m_imageLabel && m_imageLabel->geometry().contains(event->pos())) {
        chooseAndSendImage();
    }

    focusInput();
    QWidget::mousePressEvent(event);
}


void FloatingInputBar::sendCurrentMessage(SendMode mode)
{
    if (!m_inputEdit) {
        return;
    }

    QString text = m_inputEdit->toPlainText().trimmed();
    if (!text.isEmpty()) {
        if (mode == SendMode::Peer) {
            emit sendTextAsPeer(text);
        } else {
            emit sendText(text);
        }
        m_inputEdit->clear();
    }
}

void FloatingInputBar::chooseAndSendImage()
{
    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          tr("选择图片"),
                                                          desktopPath,
                                                          tr("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (!filePath.isEmpty()) {
        emit sendImage(filePath);
    }
}

void FloatingInputBar::sendHelloWorld()
{
    emit sendText(QStringLiteral("Hello World"));
}

void FloatingInputBar::syncPlatformInput()
{
#ifdef Q_OS_MACOS
    if (m_usesNativeGlass) {
        if (property(kSystemFloatingBarsSuppressedProperty).toBool()) {
            MacFloatingInputBarBridge::clearInputBar(this);
            return;
        }

        if (m_usesNativeInput) {
            MacFloatingInputBarBridge::syncInputBar(this, QString(), m_visualOpacity);
        } else {
            MacFloatingInputBarBridge::syncGlass(this, m_visualOpacity);
        }
    }
#endif
}
