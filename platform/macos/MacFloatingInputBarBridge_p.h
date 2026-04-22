#pragma once

#include <QString>

class QWidget;

namespace MacFloatingInputBarBridge {
// Test switch:
// 1 = prefer LiquidGlass, then NativeBlur, then Qt fallback
// 2 = prefer NativeBlur, then Qt fallback
// 3 = force Qt fallback
constexpr int kFloatingInputBarTestMode = 1;

enum class Appearance {
    Unsupported,
    NativeBlur,
    LiquidGlass
};

Appearance appearance();
bool isSupported();
void syncGlass(QWidget* widget, double opacity = 1.0);
void syncInputBar(QWidget* widget,
                  const QString& placeholderText = QString(),
                  double opacity = 1.0);
void focusInputBar(QWidget* widget);
void clearInputBar(QWidget* widget);
}
