#pragma once

#include <QColor>

class QWidget;

namespace MacWindowBridge {
struct WindowMetrics {
    int topInset = 38;
    int leadingInset = 60;
};

WindowMetrics configureWindow(QWidget* widget, const QColor& tintColor, bool compactTrafficLights);
bool startWindowDrag(QWidget* widget);
bool performWindowZoom(QWidget* widget);
}
