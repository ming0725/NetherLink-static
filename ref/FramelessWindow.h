#pragma once

#include <QWidget>

class QPoint;

#ifdef Q_OS_WIN
#pragma comment(lib, "dwmapi.lib")
#include <dwmapi.h>
#include <WinUser.h>
#include <windowsx.h>
#endif

constexpr int RESIZE_WINDOW_WIDTH = 8;

#ifdef Q_OS_WIN
#ifndef _MSC_VER
// 非 MSVC 下，保持原先写法（static/internal linkage）
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
    void toggleMaximizedState();
    void toggleFullScreenState();
    bool isWindowMaximized() const;
    bool isWindowFullScreen() const;
protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qint64* result) override;
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void windowVisualStateChanged(bool maximized);
private:
    bool isTitleBarDraggableArea(QWidget* watchedWidget, const QPoint& pos) const;
    void setMaximizedVisualState(bool maximized);
    void refreshWindowVisuals();
    void scheduleWindowVisualRefresh();

#ifdef Q_OS_WIN
    int adjustResizeWindow(const QPoint& pos);
#else
    Qt::Edges resizeEdgesAt(const QPoint& pos) const;
    void updateResizeCursor(Qt::Edges edges);
#endif

    QWidget* m_titleBar = nullptr;
    bool m_isMaximized = false;
#ifndef Q_OS_WIN
    bool m_cursorOverResizeEdge = false;
#endif
};
