#include "ApplicationBarItem.h"
#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>

ApplicationBarItem::ApplicationBarItem(QPixmap normal, QPixmap selected, QWidget* parent)
    : QWidget(parent)
    , normalPixmap(normal)
    , selectedPixmap(selected)
{
    rippleAnim = new QVariantAnimation(this);
    rippleAnim->setDuration(700);
    rippleAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(rippleAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        rippleRadius = value.toReal();
        update();
    });
}

ApplicationBarItem::ApplicationBarItem(QPixmap normal, QWidget* parent)
    : ApplicationBarItem(normal, normal, parent)
{
}

void ApplicationBarItem::setPixmapScale(qreal scale) {
    pixmapScale = qBound<qreal>(0.0, scale, 1.0);
    update();
}

void ApplicationBarItem::enterEvent(QEnterEvent*)
{
    hoverd = true;
    update();
}

void ApplicationBarItem::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHints(
            QPainter::Antialiasing |
            QPainter::TextAntialiasing |
            QPainter::SmoothPixmapTransform);

    // 1) hover/selected 背景圆角
    if (hoverd || selected) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0xc8, 0xcf, 0xcd, 60));
        painter.drawRoundedRect(rect(), 10, 10);
        painter.restore();
    }

    // 2) 正常状态下的图标（按 pixmapScale 缩放并居中）
    const QPixmap& base = normalPixmap;
    QSizeF tgtF = QSizeF(width(), height()) * pixmapScale;
    QPixmap normalScaled = base.scaled(
            tgtF.toSize(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
    int x0 = qFloor((width()  - normalScaled.width())  / 2.0);
    int y0 = qFloor((height() - normalScaled.height()) / 2.0);
    painter.drawPixmap(x0, y0, normalScaled);

    // 3) 如果已选中且正在 ripple 动画中，则“揭露”selectedPixmap
    if (selected && rippleRadius > 0.0) {
        painter.save();
        // 剪裁一个圆形区域，圆心在 (0, height())，半径为 rippleRadius
        QPainterPath clipPath;
        clipPath.addEllipse(
                QRectF(
                        -rippleRadius,
                        height() - rippleRadius,
                        rippleRadius * 2,
                        rippleRadius * 2
                )
        );
        painter.setClipPath(clipPath);
        // 在剪裁区域内绘制被选中状态的图标
        QPixmap selScaled = selectedPixmap.scaled(
                tgtF.toSize(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
        painter.drawPixmap(x0, y0, selScaled);
        painter.restore();
    }
}


void ApplicationBarItem::mousePressEvent(QMouseEvent* event)
{
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
        emit itemClicked(this);
    }
}

void ApplicationBarItem::leaveEvent(QEvent*)
{
    hoverd = false;
    update();
}

void ApplicationBarItem::setSelected(bool select)
{
    if (selected == select)
        return;
    selected = select;

    if (selected) {
        qreal maxR = qSqrt(width()*width() + height()*height());
        rippleAnim->stop();
        rippleAnim->setStartValue(0.0);
        rippleAnim->setEndValue(maxR);
        rippleAnim->start();
    } else {
        rippleAnim->stop();
        rippleRadius = 0.0;
    }
    update();
}

bool ApplicationBarItem::isSelected() {
    return selected;
}