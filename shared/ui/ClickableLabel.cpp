// RoundedLabel.cpp
#include "ClickableLabel.h"
#include <QPainter>
#include <QPainterPath>

ClickableLabel::ClickableLabel(QWidget* parent)
        : QLabel(parent)
        , radius(-1)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

void ClickableLabel::mousePressEvent(QMouseEvent* evt)  {
    if (evt->button() == Qt::LeftButton)
        emit clicked();
    QLabel::mousePressEvent(evt);
}
void ClickableLabel::enterEvent(QEnterEvent* evt)  {
    emit hovered(true);
    QLabel::enterEvent(evt);
}
void ClickableLabel::leaveEvent(QEvent* evt)  {
    emit hovered(false);
    QLabel::leaveEvent(evt);
}

void ClickableLabel::setRadius(int r) {
    radius = r;
    update();
}

void ClickableLabel::paintEvent(QPaintEvent* ev) {
    auto pix = pixmap();
    if (!pix.isNull() && radius > 0) {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        QRect r = rect().adjusted(1, 1, -1, -1); // 留出边框宽度
        QPainterPath path;
        path.addRoundedRect(r, radius, radius);
        p.setClipPath(path);
        p.drawPixmap(r, pix);
        p.setPen(QPen(QColor(50, 50, 50, 80), 3)); // 设置描边颜色和宽度
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    } else {
        QLabel::paintEvent(ev);
    }
}


void ClickableLabel::setRoundedPixmap(const QPixmap& pixmap, int r) {
    QLabel::setPixmap(pixmap);
    setRadius(r);
}

int ClickableLabel::getRadius() {
    return radius;
}
