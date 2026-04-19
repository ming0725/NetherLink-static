#include "ScrollAreaNoWheel.h"
#include <QWheelEvent>

ScrollAreaNoWheel::ScrollAreaNoWheel(QWidget *parent)
    : QScrollArea(parent)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ScrollAreaNoWheel::wheelEvent(QWheelEvent *event) {
    event->ignore();
}
