#include "PaintedLabel.h"

#include <QPainter>
#include <QPaintEvent>

PaintedLabel::PaintedLabel(QWidget* parent)
    : QLabel(parent)
{
    setAutoFillBackground(false);
}

PaintedLabel::PaintedLabel(const QString& text, QWidget* parent)
    : QLabel(text, parent)
{
    setAutoFillBackground(false);
}

QColor PaintedLabel::textColor() const
{
    return m_textColor;
}

void PaintedLabel::setTextColor(const QColor& color)
{
    if (m_textColor == color) {
        return;
    }

    m_textColor = color;
    update();
}

void PaintedLabel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setFont(font());
    painter.setPen(m_textColor);
    int flags = alignment();
    if (wordWrap()) {
        flags |= Qt::TextWordWrap;
    } else {
        flags |= Qt::TextSingleLine;
    }
    painter.drawText(rect(), flags, text());
}
