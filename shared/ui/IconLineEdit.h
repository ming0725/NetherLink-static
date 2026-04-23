#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QWidget>

class IconLineEdit : public QWidget
{
    Q_OBJECT
public:
    IconLineEdit(QWidget* parent = nullptr);
    ~IconLineEdit();

    QString currentText() const;
    void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;
    QLineEdit* getLineEdit() const;
    void setIcon(const QString& iconSource);
    void setIconSize(QSize);

protected:
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent*) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent* event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent* event) Q_DECL_OVERRIDE;

private:
    QLineEdit* lineEdit = Q_NULLPTR;
    QLabel* iconLabel = Q_NULLPTR;
    QString iconSource = QStringLiteral(":/resources/icon/search.png");
    bool hasFocus = false;
    QRect clearButtonRect;
    bool clearHovered = false;
    bool clearPressed = false;

    void refreshIcons();
    bool showsClearButton() const;
    void updateClearHoverState(const QPoint& pos);
    void focusInnerLineEdit();

signals:
    void requestNextFocus();
    void lineEditUnfocused();
    void lineEditFocused();
};
