#pragma once

#include <QtCore/qglobal.h>
#include <QtGui/QColor>
#include <QtGui/qwindowdefs.h>
#include <QtWidgets/QWidget>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <dwmapi.h>
#include <WinUser.h>

// Un-documented API – load at runtime
typedef BOOL (WINAPI *PFN_SetWindowCompositionAttribute)(
        HWND hWnd,
        struct WINDOWCOMPOSITIONATTRIBDATA* data
);

// Which attribute we’re setting (only need the one)
enum WINDOW_COMPOSITION_ATTRIB : DWORD {
    WCA_ACCENT_POLICY = 19
};

// The blur mode
enum ACCENT_STATE : DWORD {
    ACCENT_DISABLED                      = 0,
    ACCENT_ENABLE_GRADIENT               = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT    = 2,
    ACCENT_ENABLE_BLURBEHIND             = 3,   // Aero
    ACCENT_ENABLE_ACRYLICBLURBEHIND      = 4,   // Acrylic
    ACCENT_INVALID_STATE                 = 5
};

// Data struct we hand to the API
struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD        AccentFlags;
    DWORD        GradientColor;   // 0xAARRGGBB
    DWORD        AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOW_COMPOSITION_ATTRIB Attribute;
    PVOID                     Data;
    ULONG                     SizeOfData;
};

using NativeWindowHandle = HWND;
#else
using NativeWindowHandle = WId;
#endif

class WindowEffect {
public:
    WindowEffect();

    /// Enables acrylic blur behind the window.
    /// \param hWnd       native window handle (HWND on Windows, NSView WId on macOS)
    /// \param color      Qt color (AARRGGBB) – will be used directly
    /// \param enableShadow if true, turns on default shadow flags
    /// \param animId     custom animation ID (usually 0)
    void setAcrylicEffect(NativeWindowHandle hWnd,
                          const QColor& color = QColor(0,0,0,0x00),
                          bool enableShadow    = true,
                          quint32 animId       = 0);

    /// Enables the classic Win10 “blur behind” (Aero)
    void setAeroEffect(NativeWindowHandle hWnd);

    void setMicaEffect(NativeWindowHandle hWnd, bool isDarkMode = false, bool isAlt = false);
    void setTransparentEffect(NativeWindowHandle hWnd);
    void removeBackgroundEffect(NativeWindowHandle hWnd);
    void addShadowEffect(NativeWindowHandle hWnd);
    void removeShadowEffect(NativeWindowHandle hWnd);
    void performWindowZoom(NativeWindowHandle hWnd);
    bool isWindowZoomed(NativeWindowHandle hWnd) const;
    void toggleWindowFullScreen(NativeWindowHandle hWnd);
    bool isWindowFullScreen(NativeWindowHandle hWnd) const;

    /// Call from mouse-down to allow dragging a frameless window
    void moveWindow(NativeWindowHandle hWnd);

private:
#ifdef Q_OS_WIN
    PFN_SetWindowCompositionAttribute m_pSetWindowCompAttr;
#endif
};

