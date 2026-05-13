#include "CustomTooltip.h"
#include "shared/theme/ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QVBoxLayout>

CustomTooltip::CustomTooltip(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(PADDING, PADDING, PADDING, PADDING);

    m_label = new QLabel(this);
    QFont labelFont = m_label->font();
    labelFont.setPixelSize(12);
    m_label->setFont(labelFont);
    QPalette labelPalette = m_label->palette();
    labelPalette.setColor(QPalette::WindowText, ThemeManager::instance().color(ThemeColor::TooltipText));
    m_label->setPalette(labelPalette);
    layout->addWidget(m_label);
}

void CustomTooltip::setText(const QString &text)
{
    m_label->setText(text);
    adjustSize();
}

void CustomTooltip::showTooltip(const QPoint &pos)
{
    // 向上偏移，避免与图标重叠
    move(pos.x(), pos.y() - height() - 5);
    show();
}

void CustomTooltip::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), CORNER_RADIUS, CORNER_RADIUS);

    // 半透明白色背景
    painter.fillPath(path, ThemeManager::instance().color(ThemeColor::TooltipBackground));
    
} 
