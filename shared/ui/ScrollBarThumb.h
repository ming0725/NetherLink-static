#pragma once
#include <QWidget>

class ScrollBarThumb : public QWidget {
    Q_OBJECT
public:
    explicit ScrollBarThumb(QWidget *parent = nullptr);
    void setColor(const QColor &newColor);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QColor color;
};
