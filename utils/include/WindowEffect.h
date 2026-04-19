#pragma once

#include <QtCore/QtGlobal>
#include <QtGui/QColor>

class QWidget;

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#ifdef Q_OS_WIN
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
#endif

class WindowEffect {
public:
    WindowEffect();

    /// Enables acrylic blur behind the window.
    /// \param widget     target top-level widget
    /// \param color      Qt color (AARRGGBB) – will be used directly
    /// \param enableShadow if true, turns on default shadow flags
    /// \param animId     custom animation ID (usually 0)
    void setAcrylicEffect(QWidget* widget,
                          const QColor& color = QColor(0,0,0,0x00),
                          bool enableShadow    = true,
                          quint32 animId       = 0);

    /// Enables the classic Win10 “blur behind” (Aero)
    void setAeroEffect(QWidget* widget);

    /// Call from mouse-down to allow dragging a frameless window
    void moveWindow(QWidget* widget);

private:
#ifdef Q_OS_WIN
    PFN_SetWindowCompositionAttribute m_pSetWindowCompAttr;
#endif
};
