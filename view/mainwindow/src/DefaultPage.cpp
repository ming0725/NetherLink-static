#include "DefaultPage.h"
#include "ImageService.h"
#include <QPainter>

DefaultPage::DefaultPage(QWidget *parent)
        : QWidget(parent),
          m_imageSource(":/resources/icon/icon.png"),
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
    if (!m_imageSource.isEmpty()) {
        QPixmap scaled = ImageService::instance().scaled(m_imageSource,
                                                         m_displaySize,
                                                         Qt::KeepAspectRatio,
                                                         painter.device()->devicePixelRatioF());
        if (scaled.isNull()) {
            return;
        }
        // 居中位置
        int x = (width()  - scaled.width())  / 2;
        int y = (height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
    }
}
