#pragma once

#include <QWidget>
#include <QtCore/QtGlobal>

constexpr int RESIZE_WINDOW_WIDTH = 8;

#ifdef Q_OS_WIN
#pragma comment(lib, "dwmapi.lib")
#include <Windows.h>
#include <dwmapi.h>
#include <WinUser.h>
#include <windowsx.h>

#ifndef _MSC_VER
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
enum DWM_WINDOW_CORNER_PREFERENCE {
    DWMWCP_DEFAULT    = 0,
    DWMWCP_DONOTROUND = 1,
    DWMWCP_ROUND      = 2,
    DWMWCP_ROUNDSMALL = 3
};
constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
#endif
#endif
#endif

class FramelessWindow : public QWidget {
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget* parent = nullptr);
    ~FramelessWindow() override;
    void setTitleBar(QWidget* titleBar);
protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qint64* result) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
private:
#ifdef Q_OS_WIN
    int adjustResizeWindow(const QPoint& pos);
#else
    Qt::Edges adjustResizeWindow(const QPoint& pos) const;
    bool isTitleBarDraggableArea(const QPoint& globalPos) const;
    bool isManagedWidget(const QObject* watched) const;
#endif

    QWidget* m_titleBar = nullptr;
    bool m_isMaximized = false;
#ifndef Q_OS_WIN
    bool m_appEventFilterInstalled = false;
    bool m_macWindowConfigured = false;
#endif
};
