#pragma once

#include <QTextEdit>
#include <QWidget>

class QMouseEvent;

class AiChatFloatingInputBar : public QWidget
{
    Q_OBJECT

public:
    explicit AiChatFloatingInputBar(QWidget* parent = nullptr);

    void focusInput();

signals:
    void sendText(const QString& text);
    void inputFocused();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyTheme();
    void sendCurrentText();
    void updateInputGeometry();

    QTextEdit* m_inputEdit = nullptr;

    static constexpr int kCornerRadius = 20;
    static constexpr int kInputLeftMargin = 10;
    static constexpr int kInputRightPadding = 5;
    static constexpr int kInputTopMargin = 18;
    static constexpr int kInputBottomMargin = 16;
    static constexpr int kInputTextTopPadding = 1;
    static constexpr int kInputTextHorizontalPadding = 5;
    static constexpr int kInputTextBottomPadding = 5;
};
