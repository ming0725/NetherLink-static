#ifndef FLOATINGINPUTBAR_H
#define FLOATINGINPUTBAR_H

#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPropertyAnimation>
#include "customtooltip.h"

class FloatingInputBar : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingInputBar(QWidget *parent = nullptr);
    ~FloatingInputBar();
signals:
    void sendImage(const QString &path);
    void sendText(const QString &text);
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    void initUI();
    void updateLabelIcon(QLabel *label, const QString &normalIcon, 
                        const QString &hoveredIcon, const QSize &size);
    void sendCurrentMessage();
    void handleLabelHover(QLabel *label, const QString &normalIcon,
                         const QString &hoveredIcon, const QSize &size);
    void showTooltip(QLabel *label, const QString &text);
    void hideTooltip();
private:
    QTextEdit *m_inputEdit;
    QLabel *m_emojiLabel;
    QLabel *m_imageLabel;
    QLabel *m_screenshotLabel;
    QLabel *m_historyLabel;
    QLabel *m_sendLabel;
    CustomTooltip *m_tooltip;

    static const int CORNER_RADIUS = 20;
    static const int BUTTON_SIZE = 24;
    static const int SEND_BUTTON_SIZE = 32;
    static const int BUTTON_SPACING = 10;
    static const int VERTICAL_MARGIN = 10;
    static const int ICON_SIZE = 24;
    static const int TOOLBAR_LEFT_MARGIN = 24;  // 工具栏左边距
    static const int INPUT_LEFT_MARGIN = 10;    // 输入框左边距
    static const int RIGHT_MARGIN = 20;         // 右边距
    static const int TOOLBAR_INPUT_SPACING = 0; // 工具栏和输入框之间的间距
    static const QString RESOURCE_PATH;
};

#endif // FLOATINGINPUTBAR_H 
