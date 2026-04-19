#include "DefaultPage.h"
#include <QPainter>

DefaultPage::DefaultPage(QWidget *parent)
        : QWidget(parent),
          m_pixmap(":/resources/icon/icon.png"),
          m_displaySize(128, 128)  // 默认显示区域大小
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void DefaultPage::setImageSize(const QSize size)
{
    m_displaySize = size;
    update(); // 重绘
}

void DefaultPage::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0xF2F2F2));
    //painter.fillRect(rect(), QColor(255, 255, 255, 140));
    if (!m_pixmap.isNull()) {
        // 缩放图像到固定大小
        QPixmap scaled = m_pixmap.scaled(m_displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        // 居中位置
        int x = (width()  - scaled.width())  / 2;
        int y = (height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
    }
}