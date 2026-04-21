#pragma once
#include <QLabel>
#include <QEnterEvent>
#include <QEvent>

class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget* parent = nullptr);
    void setRadius(int r);
    void setRoundedPixmap(const QPixmap&, int r = -1);
    int getRadius();
signals:
    void clicked();
    void hovered(bool entering);
protected:
    void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
    void enterEvent(QEnterEvent*) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent*) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
private:
    int radius = -1;
};
