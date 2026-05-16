#ifndef FLOATINGINPUTBAR_H
#define FLOATINGINPUTBAR_H

#include <QWidget>
#include <QLabel>
#include <QImage>
#include <QPointer>

#include <memory>

#include "shared/ui/CustomTooltip.h"

class QTimer;
class QPainter;
class QRectF;
class QtFallbackLiquidGlassRenderer;
class TransparentTextEdit;

class FloatingInputBar : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingInputBar(QWidget *parent = nullptr);
    ~FloatingInputBar() override;
    void focusInput();
    bool submitNativeText(const QString& text);
    void triggerNativeHelloShortcut();
    void triggerNativeImageShortcut();
    void refreshPlatformAppearance();
    bool usesNativeGlass() const { return m_usesNativeGlass; }
    bool usesQtFallbackLiquidGlass() const;
    void setLiquidGlassSourceWidget(QWidget* widget);
    void scheduleLiquidGlassUpdate(int delayMs = 0);
signals:
    void sendImage(const QString &path);
    void sendText(const QString &text);
    void sendTextAsPeer(const QString &text);
    void inputFocused();
protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    enum class SendMode {
        Self,
        Peer
    };

    void initQtFallbackUi();
    void updateLabelIcon(QLabel *label,
                         const QString &normalIcon,
                         const QString &hoveredIcon,
                         const QSize &size);
    void showTooltip(QLabel *label, const QString &text);
    void hideTooltip();
    void sendCurrentMessage(SendMode mode = SendMode::Self);
    void sendHelloWorld();
    void chooseAndSendImage();
    void clearQtFallbackUi();
    void syncPlatformInput();
    void applyTheme();
    void updateFloatingPanelShadow();
    void updateQtFallbackGeometry();
    void updateSendButtonPosition();
    bool shouldUseQtFallbackLiquidGlass() const;
    void updateQtFallbackLiquidGlassState();
    void releaseQtFallbackLiquidGlassResources(bool updateWidget = true);
    void updateLiquidGlassBackground();
    QImage captureLiquidGlassSource(qreal devicePixelRatio) const;
    QImage renderLiquidGlassBackground(const QImage& source, qreal devicePixelRatio);
    QImage renderLiquidGlassBackgroundWithQtBlur(const QImage& source, qreal devicePixelRatio) const;
    void paintQtFallbackLiquidGlass(QPainter& painter, const QRectF& panelRect);
private:
    TransparentTextEdit *m_inputEdit = nullptr;
    QLabel *m_emojiLabel = nullptr;
    QLabel *m_imageLabel = nullptr;
    QLabel *m_screenshotLabel = nullptr;
    QLabel *m_historyLabel = nullptr;
    QLabel *m_sendLabel = nullptr;
    CustomTooltip *m_tooltip = nullptr;
    QPointer<QWidget> m_liquidGlassSourceWidget;
    QTimer *m_liquidGlassUpdateTimer = nullptr;
    QImage m_liquidGlassBackground;
    std::unique_ptr<QtFallbackLiquidGlassRenderer> m_liquidGlassRenderer;
    qreal m_visualOpacity = 1.0;
    bool m_usesNativeGlass = false;
    bool m_usesNativeInput = false;
    bool m_liquidGlassSourceFilterInstalled = false;
    bool m_liquidGlassCaptureInProgress = false;

    static constexpr int CORNER_RADIUS = 20;
    static constexpr int BUTTON_SIZE = 24;
    static constexpr int SEND_BUTTON_SIZE = 32;
    static constexpr int BUTTON_SPACING = 10;
    static constexpr int VERTICAL_MARGIN = 10;
    static constexpr int ICON_SIZE = 24;
    static constexpr int TOOLBAR_LEFT_MARGIN = 24;
    static constexpr int INPUT_LEFT_MARGIN = 10;
    static constexpr int RIGHT_MARGIN = 20;
    static constexpr int TOOLBAR_INPUT_SPACING = 0;
    static constexpr int HORIZONTAL_MARGIN = 22;
    static constexpr int TOP_MARGIN = 18;
    static constexpr int BOTTOM_MARGIN = 16;
};

#endif // FLOATINGINPUTBAR_H 
