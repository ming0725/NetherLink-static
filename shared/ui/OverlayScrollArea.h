#pragma once

#include <QAbstractScrollArea>
#include <QMargins>
#include <QVariantAnimation>

class QLayout;
class QVBoxLayout;
class QColor;
class SmoothScrollBar;

class OverlayScrollArea : public QAbstractScrollArea
{
public:
    explicit OverlayScrollArea(QWidget* parent = nullptr);
    QWidget* getContentWidget() const { return contentWidget; }
    QVBoxLayout* useVerticalContentLayout(const QMargins& margins = QMargins(),
                                          int spacing = 0);
    QLayout* contentLayout() const;
    void setWheelStepPixels(int pixels);
    void setScrollBarInsets(int topBottomInset, int rightInset);
    void setViewportBackgroundColor(const QColor& color);
    void clearViewportBackground();
    void relayoutContent();

protected:
    virtual void layoutContent();
    void refreshContentLayout();
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    QWidget* contentWidget;

private:
    void scheduleContentLayout();
    void showOverlayScrollBar();
    void updateOverlayScrollBar();
    void updateScrollMetrics();
    void syncContentPosition();
    void updateOverlayScrollBarGeometry();
    bool canScroll() const;
    bool shouldUseAnimatedWheel(const QWheelEvent* event) const;
    void handlePrecisionWheel(QWheelEvent* event);
    int nextAnimatedWheelTarget(const QWheelEvent* event, int startValue);
    void onOverlayScrollBarValueChanged(int value);
    void onVerticalScrollRangeChanged(int minimum, int maximum);
    void onVerticalScrollValueChanged(int value);

    SmoothScrollBar* m_overlayScrollBar;
    QVariantAnimation* m_scrollAnimation;
    int m_animatedScrollValue = 0;
    int m_wheelStepPixels = 64;
    int m_scrollBarInset = 8;
    int m_scrollBarRightInset = 8;
    qreal m_precisionScrollScale = 0.65;
    qint64 m_lastWheelEventMs = 0;
    int m_wheelStreak = 0;
    int m_lastWheelDirection = 0;
    bool m_hovered = false;
    bool m_refreshingLayout = false;
    bool m_layoutScheduled = false;
};
