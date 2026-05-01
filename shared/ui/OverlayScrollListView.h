#pragma once

#include <QListView>
#include <QPropertyAnimation>

#include "shared/theme/ThemeManager.h"

class SmoothScrollBar;

class OverlayScrollListView : public QListView
{
    Q_OBJECT
    Q_PROPERTY(int animatedScrollValue READ animatedScrollValue WRITE setAnimatedScrollValue)

public:
    explicit OverlayScrollListView(QWidget* parent = nullptr);

    void setWheelStepPixels(int pixels);
    void setScrollBarInsets(int topBottomInset, int rightInset);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    bool viewportEvent(QEvent* event) override;

    void showOverlayScrollBar();
    void updateOverlayScrollBar();
    void setThemeBackgroundRole(ThemeColor role);
    void refreshTheme();

private slots:
    void onOverlayScrollBarValueChanged(int value);
    void onVerticalScrollRangeChanged(int minimum, int maximum);
    void onVerticalScrollValueChanged(int value);

private:
    int animatedScrollValue() const { return m_animatedScrollValue; }
    void setAnimatedScrollValue(int value);
    void updateOverlayScrollBarGeometry();
    bool canScroll() const;
    bool shouldUseAnimatedWheel(const QWheelEvent* event) const;
    void handlePrecisionWheel(QWheelEvent* event);
    int nextAnimatedWheelTarget(const QWheelEvent* event, int startValue);

    SmoothScrollBar* m_overlayScrollBar;
    QPropertyAnimation* m_scrollAnimation;
    int m_animatedScrollValue = 0;
    int m_wheelStepPixels = 64;
    int m_scrollBarInset = 8;
    int m_scrollBarRightInset = 8;
    qreal m_precisionScrollScale = 0.65;
    qint64 m_lastWheelEventMs = 0;
    int m_wheelStreak = 0;
    int m_lastWheelDirection = 0;
    bool m_hovered = false;
    ThemeColor m_themeBackgroundRole = ThemeColor::PanelBackground;
};
