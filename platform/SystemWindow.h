#pragma once

#include <QColor>
#include <QWidget>
#include <QtCore/QtGlobal>

constexpr int SYSTEM_WINDOW_RESIZE_BORDER = 8;

#ifdef Q_OS_WIN
#pragma comment(lib, "dwmapi.lib")
#include <Windows.h>
#include <dwmapi.h>
#include <WinUser.h>
#include <windowsx.h>

#ifndef _MSC_VER
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
enum DWM_WINDOW_CORNER_PREFERENCE {
    DWMWCP_DEFAULT = 0,
    DWMWCP_DONOTROUND = 1,
    DWMWCP_ROUND = 2,
    DWMWCP_ROUNDSMALL = 3
};
constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
#endif
#endif

typedef BOOL (WINAPI *PFN_SetWindowCompositionAttribute)(
        HWND hWnd,
        struct WINDOWCOMPOSITIONATTRIBDATA* data
);

enum WINDOW_COMPOSITION_ATTRIB : DWORD {
    WCA_ACCENT_POLICY = 19
};

enum ACCENT_STATE : DWORD {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_INVALID_STATE = 5
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOW_COMPOSITION_ATTRIB Attribute;
    PVOID Data;
    ULONG SizeOfData;
};
#endif

class SystemWindow : public QWidget {
    Q_OBJECT
public:
    explicit SystemWindow(QWidget* parent = nullptr);
    ~SystemWindow() override;

    void setDragTitleBar(QWidget* titleBar);
    void setBackdropColor(const QColor& color);
    void setCompactTrafficLightsEnabled(bool enabled);

    int topInset() const { return m_topInset; }
    int leadingInset() const { return m_leadingInset; }
    bool usesSystemTitleButtons() const;

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qint64* result) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void ensurePlatformChrome();
    void refreshPlatformMetrics();
    void updateWindowMargins();

#ifdef Q_OS_WIN
    int hitTestResize(const QPoint& globalPos) const;
    bool isDragRegion(const QPoint& globalPos) const;
    void applyWindowsBackdrop();
#else
    Qt::Edges hitTestResize(const QPoint& globalPos) const;
    bool isDragRegion(const QPoint& globalPos) const;
    bool isManagedWidget(const QObject* watched) const;
#endif

    QWidget* m_titleBar = nullptr;
    QColor m_backdropColor = QColor(248, 248, 252, 92);
    int m_topInset = 0;
    int m_leadingInset = 0;
    bool m_platformChromeReady = false;
    bool m_isMaximized = false;
    bool m_compactTrafficLightsEnabled = false;

#ifdef Q_OS_WIN
    PFN_SetWindowCompositionAttribute m_setWindowCompositionAttribute = nullptr;
#elif !defined(Q_OS_MACOS)
    bool m_appEventFilterInstalled = false;
#endif
};
