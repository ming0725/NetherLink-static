#include "NewMessageNotifier.h"
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QResizeEvent>

NewMessageNotifier::NewMessageNotifier(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(32);
    setAttribute(Qt::WA_TranslucentBackground);
    
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    
    label = new QLabel(this);
    label->setStyleSheet("QLabel { color: #FFFFFF; font-size: 13px; }");
    layout->addWidget(label);
    
    // 初始状态为隐藏
    QWidget::hide();
}

void NewMessageNotifier::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制半透明背景
    QPainterPath path;
    path.addRoundedRect(rect(), 16, 16);
    
    // 设置阴影
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 20));
    painter.drawRoundedRect(rect().adjusted(1, 1, 1, 1), 16, 16);
    
    // 绘制主体
    painter.setBrush(QColor(0, 0, 0, 127));
    painter.drawPath(path);
}

void NewMessageNotifier::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    emit clicked();
}

void NewMessageNotifier::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void NewMessageNotifier::setMessageCount(int count)
{
    updateText(count);
    updateLayout();
}

void NewMessageNotifier::updateText(int count)
{
    label->setText(QString("有 %1 条新消息，点击查看").arg(count));
    adjustSize();
}

void NewMessageNotifier::updateLayout()
{
    if (parentWidget()) {
        // 调整位置，更靠近底部
        int x = (parentWidget()->width() - width()) / 2;
        int y = parentWidget()->height() - height() - 10;
        move(x, y);
    }
} 