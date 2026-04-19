#pragma once
#include <QScrollArea>

class ScrollAreaNoWheel : public QScrollArea {
    Q_OBJECT
public:
    explicit ScrollAreaNoWheel(QWidget *parent = nullptr);
protected:
    void wheelEvent(QWheelEvent *event) override;
};
