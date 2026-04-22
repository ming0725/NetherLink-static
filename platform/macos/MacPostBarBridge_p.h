#pragma once

#include <QStringList>

class QWidget;

namespace MacPostBarBridge {
// Test switch:
// 1 = prefer LiquidGlass, then NativeBlur, then Qt fallback
// 2 = prefer NativeBlur, then Qt fallback
// 3 = force Qt fallback
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
