#pragma once

#include <QPoint>
#include <QtGlobal>
#include <QVector>

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

enum class Mode {
    NativeMenu = 1,
    QtFallback = 3
};

Mode mode();
void setMode(Mode mode);
QVector<Mode> supportedModes();
Appearance appearance();
bool isSupported();
bool popupMenu(QWidget* anchorWidget,
               QMenu* menu,
               const QPoint& globalPos,
               const std::function<void()>& closeHandler = {});
}
