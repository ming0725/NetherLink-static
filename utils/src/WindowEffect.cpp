#include "WindowEffect.h"
#include <QWidget>
#include <QWindow>

#ifdef Q_OS_WIN
#include <QtCore/QDebug>
#include <Windows.h>
#include <winuser.h>
#endif

#ifdef Q_OS_MACOS
#include "MacWindowEffects_p.h"
#endif

WindowEffect::WindowEffect()
#ifdef Q_OS_WIN
        : m_pSetWindowCompAttr(nullptr)
#endif
{
#ifdef Q_OS_WIN
    // Dynamically load the (undocumented) API
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        m_pSetWindowCompAttr = reinterpret_cast<PFN_SetWindowCompositionAttribute>(
                GetProcAddress(hUser, "SetWindowCompositionAttribute"));
    }

    if (!m_pSetWindowCompAttr) {
        qWarning() << "Failed to load SetWindowCompositionAttribute";
    }
#endif
}

void WindowEffect::setAcrylicEffect(QWidget* widget,
                                    const QColor& color,
                                    bool enableShadow,
                                    quint32 animId)
{
#ifdef Q_OS_WIN
    if (!widget || !m_pSetWindowCompAttr)
        return;

    HWND hWnd = reinterpret_cast<HWND>(widget->winId());
    if (!m_pSetWindowCompAttr)
        return;

    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    // Set shadow flags if requested (0x20|0x40|0x80|0x100) matches Python code
    policy.AccentFlags = enableShadow ? (0x20|0x40|0x80|0x100) : 0;
    // Use Qt’s ARGB directly (0xAARRGGBB)
    policy.GradientColor = static_cast<DWORD>(color.rgba());
    policy.AnimationId   = animId;

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute  = WCA_ACCENT_POLICY;
    data.Data       = &policy;
    data.SizeOfData = sizeof(policy);

    m_pSetWindowCompAttr(hWnd, &data);
#elif defined(Q_OS_MACOS)
    Q_UNUSED(enableShadow);
    Q_UNUSED(animId);
    MacWindowEffects::applyBackdrop(widget, color);
#else
    Q_UNUSED(widget);
    Q_UNUSED(color);
    Q_UNUSED(enableShadow);
    Q_UNUSED(animId);
#endif
}

void WindowEffect::setAeroEffect(QWidget* widget)
{
#ifdef Q_OS_WIN
    if (!widget || !m_pSetWindowCompAttr)
        return;

    HWND hWnd = reinterpret_cast<HWND>(widget->winId());
    if (!m_pSetWindowCompAttr)
        return;

    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_BLURBEHIND;
    // other fields zero

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute  = WCA_ACCENT_POLICY;
    data.Data       = &policy;
    data.SizeOfData = sizeof(policy);

    m_pSetWindowCompAttr(hWnd, &data);
#elif defined(Q_OS_MACOS)
    MacWindowEffects::applyBackdrop(widget, QColor(248, 248, 252, 92));
#else
    Q_UNUSED(widget);
#endif
}

void WindowEffect::moveWindow(QWidget* widget)
{
#ifdef Q_OS_WIN
    if (!widget)
        return;

    HWND hWnd = reinterpret_cast<HWND>(widget->winId());
    // Release mouse capture & send WM_SYSCOMMAND to drag the title bar
    ReleaseCapture();
    SendMessageW(hWnd, WM_SYSCOMMAND,
                 SC_MOVE | HTCAPTION,
                 0);
#else
    if (!widget)
        return;

    if (QWindow* handle = widget->windowHandle()) {
        handle->startSystemMove();
    }
#endif
}
