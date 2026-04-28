#pragma once

#include <QPoint>
#include <QtGlobal>

#include <functional>

class QMenu;
class QWidget;

namespace MacStyledActionMenuBridge {
// Test switch:
// 1/2 = native AppKit menu. macOS 26+ uses system Liquid Glass; older macOS uses native blur.
// 3 = force Qt fallback
constexpr int kStyledActionMenuTestMode = 1;

enum class Appearance {
    Unsupported,
    NativeMenu
};

Appearance appearance();
bool isSupported();
bool popupMenu(QWidget* anchorWidget,
               QMenu* menu,
               const QPoint& globalPos,
               const std::function<void()>& closeHandler = {});
}
