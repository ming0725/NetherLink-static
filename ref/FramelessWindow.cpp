// FramelessWindow.cpp
#include "FramelessWindow.h"
#include "WindowEffect.h"
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QRegion>
#include <QTimer>
#include <QWindow>

FramelessWindow::FramelessWindow(QWidget* parent)
        : QWidget(parent)
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

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
    if (QApplication::instance()) {
        QApplication::instance()->installEventFilter(this);
    }
#endif
}

FramelessWindow::~FramelessWindow()
{
#ifndef Q_OS_WIN
    if (QApplication::instance()) {
        QApplication::instance()->removeEventFilter(this);
    }
#endif
}

void FramelessWindow::setTitleBar(QWidget* titleBar)
{
    if (m_titleBar) {
        m_titleBar->removeEventFilter(this);
    }

    m_titleBar = titleBar;

    if (m_titleBar) {
        m_titleBar->installEventFilter(this);
        m_titleBar->setMouseTracking(true);
    }
}

bool FramelessWindow::nativeEvent(const QByteArray& eventType, void* message, qint64* result)
{
#ifdef Q_OS_WIN
    MSG* msg = reinterpret_cast<MSG*>(message);
    switch (msg->message) {
        case WM_NCCALCSIZE:
            *result = 0;
            return true;

        case WM_NCLBUTTONDBLCLK:
            if (msg->wParam == HTCAPTION) {
                toggleMaximizedState();
                *result = 0;
                return true;
            }
            break;

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
                if (isTitleBarDraggableArea(m_titleBar, local)) {
                    *result = HTCAPTION;
                    return true;
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

bool FramelessWindow::event(QEvent* event)
{
#ifndef Q_OS_WIN
    if (event->type() == QEvent::Resize && !isWindowMaximized() && !isWindowFullScreen()) {
        QPainterPath path;
        path.addRoundedRect(rect(), 10, 10);
        setMask(QRegion(path.toFillPolygon().toPolygon()));
    }
#endif

    if (event->type() == QEvent::WindowStateChange) {
        bool maximized = isWindowMaximized() || isWindowFullScreen();
        if (maximized != m_isMaximized) {
            setMaximizedVisualState(maximized);
        } else {
            scheduleWindowVisualRefresh();
        }
    }
    return QWidget::event(event);
}

bool FramelessWindow::eventFilter(QObject* watched, QEvent* event)
{
    QWidget* watchedWidget = qobject_cast<QWidget*>(watched);

    if (event->type() == QEvent::MouseButtonDblClick) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton
            && isTitleBarDraggableArea(watchedWidget, mouseEvent->pos())) {
            toggleMaximizedState();
            return true;
        }
    }

#ifndef Q_OS_WIN
    if (watchedWidget && (watchedWidget == this || isAncestorOf(watchedWidget))) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint localPos = watchedWidget == this
                    ? mouseEvent->pos()
                    : watchedWidget->mapTo(this, mouseEvent->pos());
            updateResizeCursor(resizeEdgesAt(localPos));
            break;
        }
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() != Qt::LeftButton || isWindowMaximized()) {
                break;
            }

            const QPoint localPos = watchedWidget == this
                    ? mouseEvent->pos()
                    : watchedWidget->mapTo(this, mouseEvent->pos());
            const Qt::Edges edges = resizeEdgesAt(localPos);
            if (edges && windowHandle()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                if (windowHandle()->startSystemResize(edges)) {
                    return true;
                }
#endif
            }

            if (isTitleBarDraggableArea(watchedWidget, mouseEvent->pos()) && windowHandle()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                if (windowHandle()->startSystemMove()) {
                    return true;
                }
#endif
            }
            break;
        }
        case QEvent::Leave:
            if (m_cursorOverResizeEdge) {
                unsetCursor();
                m_cursorOverResizeEdge = false;
            }
            break;
        default:
            break;
        }
    }
#endif

    return QWidget::eventFilter(watched, event);
}

bool FramelessWindow::isTitleBarDraggableArea(QWidget* watchedWidget, const QPoint& pos) const
{
    if (!m_titleBar || watchedWidget != m_titleBar || !m_titleBar->rect().contains(pos)) {
        return false;
    }

    return m_titleBar->childAt(pos) == nullptr;
}

void FramelessWindow::toggleMaximizedState()
{
#ifdef Q_OS_WIN
    if (isWindowMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
#elif defined(Q_OS_MACOS)
    WindowEffect effect;
    effect.performWindowZoom(NativeWindowHandle(winId()));
    setMaximizedVisualState(effect.isWindowZoomed(NativeWindowHandle(winId())));
#else
    if (isWindowMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
#endif
}

void FramelessWindow::toggleFullScreenState()
{
#ifdef Q_OS_MACOS
    WindowEffect effect;
    const bool willEnterFullScreen = !effect.isWindowFullScreen(NativeWindowHandle(winId()));
    effect.toggleWindowFullScreen(NativeWindowHandle(winId()));
    setMaximizedVisualState(willEnterFullScreen);
    scheduleWindowVisualRefresh();
#else
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
#endif
}

bool FramelessWindow::isWindowMaximized() const
{
#ifdef Q_OS_WIN
    return isMaximized();
#elif defined(Q_OS_MACOS)
    WindowEffect effect;
    return effect.isWindowZoomed(NativeWindowHandle(winId()));
#else
    return isMaximized();
#endif
}

bool FramelessWindow::isWindowFullScreen() const
{
#ifdef Q_OS_MACOS
    WindowEffect effect;
    return effect.isWindowFullScreen(NativeWindowHandle(winId()));
#else
    return isFullScreen();
#endif
}

void FramelessWindow::setMaximizedVisualState(bool maximized)
{
    m_isMaximized = maximized;
    if (maximized) {
        setContentsMargins(0, 0, 0, 0);
#ifndef Q_OS_WIN
        clearMask();
#endif
    } else {
        setContentsMargins(8, 8, 8, 8);
#ifndef Q_OS_WIN
        QPainterPath path;
        path.addRoundedRect(rect(), 10, 10);
        setMask(QRegion(path.toFillPolygon().toPolygon()));
#endif
    }
    scheduleWindowVisualRefresh();
}

void FramelessWindow::windowVisualStateChanged(bool maximized)
{
    Q_UNUSED(maximized);
}

void FramelessWindow::refreshWindowVisuals()
{
#ifndef Q_OS_WIN
    if (m_isMaximized || isWindowFullScreen()) {
        clearMask();
    } else {
        QPainterPath path;
        path.addRoundedRect(rect(), 10, 10);
        setMask(QRegion(path.toFillPolygon().toPolygon()));
    }
#endif

    updateGeometry();
    update();
    for (QWidget* child : findChildren<QWidget*>()) {
        child->updateGeometry();
        child->update();
    }
    windowVisualStateChanged(m_isMaximized);
}

void FramelessWindow::scheduleWindowVisualRefresh()
{
    refreshWindowVisuals();
    QTimer::singleShot(120, this, &FramelessWindow::refreshWindowVisuals);
    QTimer::singleShot(420, this, &FramelessWindow::refreshWindowVisuals);
    QTimer::singleShot(800, this, &FramelessWindow::refreshWindowVisuals);
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
Qt::Edges FramelessWindow::resizeEdgesAt(const QPoint& pos) const
{
    Qt::Edges edges;
    if (minimumWidth() != maximumWidth()) {
        if (pos.x() >= 0 && pos.x() < RESIZE_WINDOW_WIDTH) {
            edges |= Qt::LeftEdge;
        } else if (pos.x() <= width() && pos.x() > width() - RESIZE_WINDOW_WIDTH) {
            edges |= Qt::RightEdge;
        }
    }

    if (minimumHeight() != maximumHeight()) {
        if (pos.y() >= 0 && pos.y() < RESIZE_WINDOW_WIDTH) {
            edges |= Qt::TopEdge;
        } else if (pos.y() <= height() && pos.y() > height() - RESIZE_WINDOW_WIDTH) {
            edges |= Qt::BottomEdge;
        }
    }

    return edges;
}

void FramelessWindow::updateResizeCursor(Qt::Edges edges)
{
    if (isWindowMaximized()) {
        edges = Qt::Edges();
    }

    if (!edges) {
        if (m_cursorOverResizeEdge) {
            unsetCursor();
            m_cursorOverResizeEdge = false;
        }
        return;
    }

    if ((edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::LeftEdge)) ||
        (edges.testFlag(Qt::BottomEdge) && edges.testFlag(Qt::RightEdge))) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::RightEdge)) ||
               (edges.testFlag(Qt::BottomEdge) && edges.testFlag(Qt::LeftEdge))) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge)) {
        setCursor(Qt::SizeHorCursor);
    } else {
        setCursor(Qt::SizeVerCursor);
    }

    m_cursorOverResizeEdge = true;
}
#endif
