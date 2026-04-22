#pragma once

#include <QStringList>

class QWidget;

namespace MacPostBarBridge {
// Test switch:
// 1 = LiquidGlass bar
// 2 = NativeBlur bar
// 3 = Qt fallback bar
constexpr int kPostBarTestMode = 1;

enum class Appearance {
    Unsupported,
    NativeBlur,
    LiquidGlass
};

Appearance appearance();
bool isSupported();
void syncBar(QWidget* widget,
             const QStringList& labels,
             int selectedIndex,
             bool animateSelection = false,
             double opacity = 1.0);
void clearBar(QWidget* widget);
}
