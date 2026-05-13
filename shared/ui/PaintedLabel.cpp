#include "PaintedLabel.h"

#include "shared/theme/ThemeManager.h"

#include <QPaintEvent>
#include <QPalette>

PaintedLabel::PaintedLabel(QWidget* parent)
    : QLabel(parent)
    , m_textColor(ThemeManager::instance().color(ThemeColor::SecondaryText))
{
    setAutoFillBackground(false);
}

PaintedLabel::PaintedLabel(const QString& text, QWidget* parent)
    : QLabel(text, parent)
    , m_textColor(ThemeManager::instance().color(ThemeColor::SecondaryText))
{
    setAutoFillBackground(false);
}

QColor PaintedLabel::textColor() const
{
    return m_textColor;
}

void PaintedLabel::setTextColor(const QColor& color)
{
    const bool changed = m_textColor != color;
    m_textColor = color;

    QPalette labelPalette = palette();
    if (labelPalette.color(QPalette::WindowText) != color ||
        labelPalette.color(QPalette::Text) != color) {
        labelPalette.setColor(QPalette::WindowText, color);
        labelPalette.setColor(QPalette::Text, color);
        setPalette(labelPalette);
    }

    if (!changed) {
        return;
    }

    update();
}

void PaintedLabel::paintEvent(QPaintEvent* event)
{
    QLabel::paintEvent(event);
}
