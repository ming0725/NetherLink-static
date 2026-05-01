#pragma once

#include <QLineEdit>
#include <QPainter>

class IconLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    IconLineEdit(QWidget* parent = nullptr);
    ~IconLineEdit() override;

    QString currentText() const;
    void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;
    QLineEdit* getLineEdit() const;
    void setIcon(const QString& iconSource);
    void setDarkIcon(const QString& iconSource);
    void setIconSize(QSize);

protected:
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent*) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent* event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
    void focusInEvent(QFocusEvent* event) Q_DECL_OVERRIDE;
    void focusOutEvent(QFocusEvent* event) Q_DECL_OVERRIDE;

private:
    QString iconSource = QStringLiteral(":/resources/icon/search.png");
    QString darkIconSource = QStringLiteral(":/resources/icon/search_darkmode.png");
    QSize iconSize = QSize(15, 15);
    bool hasFocus = false;
    QRect iconRect;
    QRect clearButtonRect;
    bool clearHovered = false;
    bool clearPressed = false;

    void updateTextMargins();
    void updateControlRects();
    bool showsClearButton() const;
    void updateClearHoverState(const QPoint& pos);
    void focusInnerLineEdit();
    void applyThemePalette();

signals:
    void requestNextFocus();
    void lineEditUnfocused();
    void lineEditFocused();
};
