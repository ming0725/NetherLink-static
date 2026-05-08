#pragma once

#include <QElapsedTimer>
#include <QEasingCurve>
#include <QMargins>
#include <QVector>
#include <QWidget>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QEnterEvent;
#endif
class QGraphicsOpacityEffect;
class QMouseEvent;
class QParallelAnimationGroup;
class QPropertyAnimation;
class QTimer;

class GlobalNotification final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal closeButtonOpacity READ closeButtonOpacity WRITE setCloseButtonOpacity)

public:
    enum class Type {
        Success,
        Failure
    };

    explicit GlobalNotification(QWidget* attachedWidget,
                                Type type,
                                const QString& message,
                                QWidget* parent = nullptr);
    ~GlobalNotification() override;

    static void showSuccess(QWidget* attachedWidget, const QString& message);
    static void showFailure(QWidget* attachedWidget, const QString& message);
    static void showNotification(QWidget* attachedWidget, Type type, const QString& message);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* event) override;
#else
    void enterEvent(QEvent* event) override;
#endif
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void start();
    QRect closeButtonRect() const;
    QRect stackGeometry(int depth) const;
    QRect expandedGeometry(int depth) const;
    QRect hiddenGeometry(int depth) const;
    qreal stackOpacity(int depth) const;
    qreal closeButtonOpacity() const;
    void setCloseButtonOpacity(qreal opacity);
    QParallelAnimationGroup* animateTo(const QRect& targetGeometry,
                                       qreal targetOpacity,
                                       int duration,
                                       QEasingCurve::Type easingCurve);
    void animateCloseButton(qreal targetOpacity);
    void stopLayoutAnimation();
    void animateToStackDepth(int depth, bool animated);
    void dismiss(bool slideToHidden);
    void dismissTo(const QRect& targetGeometry, int duration);
    void startDismissTimer(int duration);
    void pauseDismissTimer();
    void resumeDismissTimer();
    void restartDismissTimer();
    void updateForParentResize();
    void drawNinePatch(QPainter& painter, const QPixmap& pixmap, const QRect& targetRect) const;
    static QVector<GlobalNotification*> activeNotifications(QWidget* parent);
    static void relayoutNotifications(QWidget* parent, bool animated);
    static bool isExpanded(QWidget* parent);
    static bool containsGlobalPos(QWidget* parent, const QPoint& globalPos);
    static bool containsCursor(QWidget* parent);
    static void expandNotifications(QWidget* parent);
    static void collapseNotifications(QWidget* parent);
    static void clearNotifications(QWidget* parent);
    static void startExpandedDismissTimer(QWidget* parent);
    static void pauseExpandedDismissTimer(QWidget* parent);
    static void restartExpandedDismissTimer(QWidget* parent);
    static void stopExpandedDismissTimer(QWidget* parent);
    static void pauseDismissTimers(QWidget* parent);
    static void resumeDismissTimers(QWidget* parent);
    static void restartDismissTimers(QWidget* parent);

    Type m_type;
    QString m_message;
    QWidget* m_attachedWidget = nullptr;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QParallelAnimationGroup* m_layoutAnimation = nullptr;
    QPropertyAnimation* m_closeButtonAnimation = nullptr;
    QTimer* m_dismissTimer = nullptr;
    QElapsedTimer m_dismissElapsed;
    int m_remainingDismissMs = 0;
    QMargins m_sourceMargins;
    quint64 m_sequence = 0;
    qreal m_closeButtonOpacity = 0.0;
    bool m_dismissTimerRunning = false;
    bool m_dismissing = false;
};
