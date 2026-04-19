#pragma once

#include <QColor>

class QWidget;

namespace MacWindowEffects {
void configureFramelessWindow(QWidget* widget);
void applyBackdrop(QWidget* widget, const QColor& tintColor);
}
