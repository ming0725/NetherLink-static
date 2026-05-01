#include "DefaultPage.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
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
    painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::PageBackground));
    if (!m_imageSource.isEmpty()) {
        QPixmap scaled = ImageService::instance().scaled(m_imageSource,
                                                         m_displaySize,
                                                         Qt::KeepAspectRatio,
                                                         painter.device()->devicePixelRatioF());
        if (scaled.isNull()) {
            return;
        }
        const qreal dpr = scaled.devicePixelRatio();
        const QSize logicalSize(qRound(scaled.width() / dpr),
                                qRound(scaled.height() / dpr));
        const QRect targetRect((width() - logicalSize.width()) / 2,
                               (height() - logicalSize.height()) / 2,
                               logicalSize.width(),
                               logicalSize.height());
        painter.drawPixmap(targetRect, scaled);
    }
}
