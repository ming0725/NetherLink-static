#ifndef FLOATINGINPUTBAR_H
#define FLOATINGINPUTBAR_H

#include <QWidget>
#include <QTextEdit>
#include <QLabel>

#include "shared/ui/CustomTooltip.h"

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
    void syncPlatformInput();
    void applyTheme();
private:
    QTextEdit *m_inputEdit = nullptr;
    QLabel *m_emojiLabel = nullptr;
    QLabel *m_imageLabel = nullptr;
    QLabel *m_screenshotLabel = nullptr;
    QLabel *m_historyLabel = nullptr;
    QLabel *m_sendLabel = nullptr;
    CustomTooltip *m_tooltip = nullptr;
    qreal m_visualOpacity = 1.0;
    bool m_usesNativeGlass = false;
    bool m_usesNativeInput = false;

    static const int CORNER_RADIUS = 20;
    static const int BUTTON_SIZE = 24;
    static const int SEND_BUTTON_SIZE = 32;
    static const int BUTTON_SPACING = 10;
    static const int VERTICAL_MARGIN = 10;
    static const int ICON_SIZE = 24;
    static const int TOOLBAR_LEFT_MARGIN = 24;
    static const int INPUT_LEFT_MARGIN = 10;
    static const int RIGHT_MARGIN = 20;
    static const int TOOLBAR_INPUT_SPACING = 0;
    static const int HORIZONTAL_MARGIN = 22;
    static const int TOP_MARGIN = 18;
    static const int BOTTOM_MARGIN = 16;
};

#endif // FLOATINGINPUTBAR_H 
