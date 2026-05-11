#pragma once

#include <QWidget>

class QAbstractButton;
class QMouseEvent;
class TransparentTextEdit;

class AiChatFloatingInputBar : public QWidget
{
    Q_OBJECT

public:
    explicit AiChatFloatingInputBar(QWidget* parent = nullptr);

    void focusInput();
    void setStreaming(bool streaming);

signals:
    void sendText(const QString& text);
    void stopStreamingRequested();
    void inputFocused();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyTheme();
    void onActionButtonClicked();
    void sendCurrentText();
    void updateActionButtonIcon();
    void updateInputGeometry();

    TransparentTextEdit* m_inputEdit = nullptr;
    QAbstractButton* m_actionButton = nullptr;
    bool m_streaming = false;

    static constexpr int kCornerRadius = 20;
    static constexpr int kInputLeftMargin = 10;
    static constexpr int kInputRightPadding = 54;
    static constexpr int kInputTopMargin = 18;
    static constexpr int kInputBottomMargin = 16;
    static constexpr int kInputTextTopPadding = 1;
    static constexpr int kInputTextHorizontalPadding = 5;
    static constexpr int kInputTextBottomPadding = 5;
    static constexpr int kActionButtonSize = 34;
    static constexpr int kActionButtonRightMargin = 14;
    static constexpr int kActionButtonBottomMargin = 14;
};
