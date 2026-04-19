// FramelessWindow.cpp
#include "FramelessWindow.h"
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QShowEvent>
#include <QtGlobal>
#include <QWindow>

#ifdef Q_OS_MACOS
#include "MacWindowEffects_p.h"
#endif

namespace {
QPoint eventGlobalPos(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}
}

FramelessWindow::FramelessWindow(QWidget* parent)
        : QWidget(parent)
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);

#ifdef Q_OS_WIN
    HWND hwnd = HWND(winId());
    LONG style = ::GetWindowLong(hwnd, GWL_STYLE);
    ::SetWindowLong(hwnd, GWL_STYLE,
                    style | WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX);

    UINT preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd,
                          DWMWA_WINDOW_CORNER_PREFERENCE,
                          &preference, sizeof(preference));
#else
    if (qApp) {
        qApp->installEventFilter(this);
        m_appEventFilterInstalled = true;
    }
#endif
}

FramelessWindow::~FramelessWindow()
{
#ifndef Q_OS_WIN
    if (m_appEventFilterInstalled && qApp) {
        qApp->removeEventFilter(this);
    }
#endif
}

void FramelessWindow::setTitleBar(QWidget* titleBar)
{
    if (m_titleBar == titleBar) {
        return;
    }

#ifndef Q_OS_WIN
    if (m_titleBar) {
        m_titleBar->removeEventFilter(this);
    }
#endif

    m_titleBar = titleBar;

#ifndef Q_OS_WIN
    if (m_titleBar) {
        m_titleBar->installEventFilter(this);
    }
#endif
}

bool FramelessWindow::nativeEvent(const QByteArray& eventType, void* message, qint64* result)
{
#ifdef Q_OS_WIN
    MSG* msg = reinterpret_cast<MSG*>(message);
    switch (msg->message) {
        case WM_NCCALCSIZE:
            *result = 0;
            return true;

        case WM_NCHITTEST: {
            const LONG gx = GET_X_LPARAM(msg->lParam);
            const LONG gy = GET_Y_LPARAM(msg->lParam);
            QPoint globalPos(gx, gy);

            int hit = adjustResizeWindow(globalPos);
            if (hit) {
                *result = hit;
                return true;
            }

            // 标题栏拖拽检测
            if (m_titleBar) {
                double dpr = devicePixelRatioF();
                QPoint local = m_titleBar->mapFromGlobal(QPoint(gx / dpr, gy / dpr));
                if (m_titleBar->rect().contains(local)) {
                    QWidget* child = m_titleBar->childAt(local);
                    if (!child) {
                        *result = HTCAPTION;
                        return true;
                    }
                }
            }
            return false;
        }
        default:
            return QWidget::nativeEvent(eventType, message, result);
    }
#else
    return QWidget::nativeEvent(eventType, message, result);
#endif
}

bool FramelessWindow::eventFilter(QObject* watched, QEvent* event)
{
#ifndef Q_OS_WIN
    if (!isManagedWidget(watched)) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && !isMaximized() && !isFullScreen()) {
            if (QWindow* handle = windowHandle()) {
                const Qt::Edges edges = adjustResizeWindow(eventGlobalPos(mouseEvent));
                if (edges.testAnyFlags(Qt::TopEdge | Qt::BottomEdge | Qt::LeftEdge | Qt::RightEdge)
                    && handle->startSystemResize(edges)) {
                    event->accept();
                    return true;
                }
            }
        }
    }

    if (watched == m_titleBar) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton
                && isTitleBarDraggableArea(eventGlobalPos(mouseEvent))) {
                isMaximized() ? showNormal() : showMaximized();
                event->accept();
                return true;
            }
        }

        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton
                && isTitleBarDraggableArea(eventGlobalPos(mouseEvent))) {
                if (QWindow* handle = windowHandle(); handle && handle->startSystemMove()) {
                    event->accept();
                    return true;
                }
            }
        }
    }
#else
    Q_UNUSED(watched);
    Q_UNUSED(event);
#endif

    return QWidget::eventFilter(watched, event);
}

bool FramelessWindow::event(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        bool maximized = isMaximized();
        if (maximized != m_isMaximized) {
            m_isMaximized = maximized;
            if (maximized) {
                setContentsMargins(0, 0, 0, 0);
            } else {
                setContentsMargins(8, 8, 8, 8);
            }
        }
    }
    return QWidget::event(event);
}

void FramelessWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

#ifdef Q_OS_MACOS
    if (!m_macWindowConfigured) {
        MacWindowEffects::configureFramelessWindow(this);
        m_macWindowConfigured = true;
    }
#endif
}

#ifdef Q_OS_WIN
int FramelessWindow::adjustResizeWindow(const QPoint& pos)
{
    int result = 0;

    RECT winrect;
    GetWindowRect(HWND(this->winId()), &winrect);

    int mouse_x = pos.x();
    int mouse_y = pos.y();

    bool resizeWidth = this->minimumWidth() != this->maximumWidth();
    bool resizeHieght = this->minimumHeight() != this->maximumHeight();

    if (resizeWidth) {
        if (mouse_x > winrect.left && mouse_x < winrect.left + RESIZE_WINDOW_WIDTH)
            result = HTLEFT;
        if (mouse_x < winrect.right && mouse_x >= winrect.right - RESIZE_WINDOW_WIDTH)
            result = HTRIGHT;
    }
    if (resizeHieght) {
        if (mouse_y < winrect.top + RESIZE_WINDOW_WIDTH && mouse_y >= winrect.top)
            result = HTTOP;

        if (mouse_y <= winrect.bottom && mouse_y > winrect.bottom - RESIZE_WINDOW_WIDTH)
            result = HTBOTTOM;
    }
    if (resizeWidth && resizeHieght) {
        // topleft corner
        if (mouse_x >= winrect.left && mouse_x < winrect.left + RESIZE_WINDOW_WIDTH && mouse_y >= winrect.top && mouse_y < winrect.top + RESIZE_WINDOW_WIDTH) {
            result = HTTOPLEFT;
        }
        // topRight corner
        if (mouse_x <= winrect.right && mouse_x > winrect.right - RESIZE_WINDOW_WIDTH && mouse_y >= winrect.top && mouse_y < winrect.top + RESIZE_WINDOW_WIDTH)
            result = HTTOPRIGHT;
        // leftBottom  corner
        if (mouse_x >= winrect.left && mouse_x < winrect.left + RESIZE_WINDOW_WIDTH && mouse_y <= winrect.bottom && mouse_y > winrect.bottom - RESIZE_WINDOW_WIDTH)
            result = HTBOTTOMLEFT;
        // rightbottom  corner
        if (mouse_x <= winrect.right && mouse_x > winrect.right - RESIZE_WINDOW_WIDTH && mouse_y <= winrect.bottom && mouse_y > winrect.bottom - RESIZE_WINDOW_WIDTH)
            result = HTBOTTOMRIGHT;
    }
    return result;
}
#else
Qt::Edges FramelessWindow::adjustResizeWindow(const QPoint& pos) const
{
    Qt::Edges edges;
    const QRect rect = frameGeometry();

    const bool resizeWidth = minimumWidth() != maximumWidth();
    const bool resizeHeight = minimumHeight() != maximumHeight();

    if (resizeWidth) {
        if (pos.x() >= rect.left() && pos.x() < rect.left() + RESIZE_WINDOW_WIDTH) {
            edges |= Qt::LeftEdge;
        } else if (pos.x() <= rect.right() && pos.x() > rect.right() - RESIZE_WINDOW_WIDTH) {
            edges |= Qt::RightEdge;
        }
    }

    if (resizeHeight) {
        if (pos.y() >= rect.top() && pos.y() < rect.top() + RESIZE_WINDOW_WIDTH) {
            edges |= Qt::TopEdge;
        } else if (pos.y() <= rect.bottom() && pos.y() > rect.bottom() - RESIZE_WINDOW_WIDTH) {
            edges |= Qt::BottomEdge;
        }
    }

    return edges;
}

bool FramelessWindow::isTitleBarDraggableArea(const QPoint& globalPos) const
{
    if (!m_titleBar) {
        return false;
    }

    const QPoint localPos = m_titleBar->mapFromGlobal(globalPos);
    if (!m_titleBar->rect().contains(localPos)) {
        return false;
    }

    return m_titleBar->childAt(localPos) == nullptr;
}

bool FramelessWindow::isManagedWidget(const QObject* watched) const
{
    const QWidget* widget = qobject_cast<const QWidget*>(watched);
    if (!widget) {
        return false;
    }

    return widget == this || isAncestorOf(const_cast<QWidget*>(widget));
}
#endif
