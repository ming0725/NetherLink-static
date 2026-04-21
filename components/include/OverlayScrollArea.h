#pragma once

#include <QAbstractScrollArea>
#include <QVariantAnimation>

class SmoothScrollBar;

class OverlayScrollArea : public QAbstractScrollArea
{
public:
    explicit OverlayScrollArea(QWidget* parent = nullptr);
    QWidget* getContentWidget() const { return contentWidget; }
    void setWheelStepPixels(int pixels);
    void setScrollBarInsets(int topBottomInset, int rightInset);

protected:
    virtual void layoutContent() = 0;
    void refreshContentLayout();
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;

    QWidget* contentWidget;

private:
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
};
