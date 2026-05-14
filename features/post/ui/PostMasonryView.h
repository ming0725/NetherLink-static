#pragma once

#include <QAbstractItemView>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVector>

#include "shared/types/RepositoryTypes.h"

class PostCardDelegate;
class SmoothScrollBar;

class PostMasonryView : public QAbstractItemView
{
    Q_OBJECT
    Q_PROPERTY(int animatedScrollValue READ animatedScrollValue WRITE setAnimatedScrollValue)

public:
    explicit PostMasonryView(QWidget* parent = nullptr);

    void setCardDelegate(PostCardDelegate* delegate);
    qreal hoverOverlayOpacityForIndex(const QModelIndex& index) const;

signals:
    void reachedBottom();
    void postActivated(const PostSummary& summary, const QRect& globalGeometry);
    void postAuthorActivated(const QString& authorId);
    void postLikeRequested(const QString& postId, bool liked);

protected:
    QRect visualRect(const QModelIndex& index) const override;
    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;
    QModelIndex indexAt(const QPoint& point) const override;
    QModelIndex moveCursor(CursorAction cursorAction,
                           Qt::KeyboardModifiers modifiers) override;
    int horizontalOffset() const override;
    int verticalOffset() const override;
    bool isIndexHidden(const QModelIndex& index) const override;
    void setSelection(const QRect& rect,
                      QItemSelectionModel::SelectionFlags command) override;
    QRegion visualRegionForSelection(const QItemSelection& selection) const override;

    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void rowsInserted(const QModelIndex& parent, int start, int end) override;
    void rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) override;
    void dataChanged(const QModelIndex& topLeft,
                     const QModelIndex& bottomRight,
                     const QList<int>& roles) override;
    void updateGeometries() override;

private slots:
    void onOverlayScrollBarValueChanged(int value);
    void onVerticalScrollValueChanged(int value);
    void onVerticalScrollRangeChanged(int minimum, int maximum);
    void onResizeDebounceTimeout();

private:
    struct LayoutItem {
        QRect rect;
        int height = 0;
    };

    int animatedScrollValue() const { return m_animatedScrollValue; }
    void setAnimatedScrollValue(int value);
    void relayout();
    void scheduleWarmVisiblePreviews(int delayMs);
    void scheduleViewportRefresh(int delayMs);
    void updateScrollBarGeometry();
    void updateOverlayScrollBar();
    void showOverlayScrollBar();
    void updateHoveredIndex(const QModelIndex& index);
    void warmVisiblePreviews();
    bool canScroll() const;
    bool shouldUseAnimatedWheel(const QWheelEvent* event) const;
    void handlePrecisionWheel(QWheelEvent* event);
    int nextAnimatedWheelTarget(const QWheelEvent* event, int startValue);
    QStyleOptionViewItem viewOptionsForIndex(const QModelIndex& index) const;
    QRect itemRectInViewport(const QModelIndex& index) const;
    int totalContentHeight() const;
    void maybeEmitReachedBottom();
    QVector<int> visibleRowsForContentRect(const QRect& visibleRect) const;

    QVector<LayoutItem> m_layoutItems;
    QVector<QVector<int>> m_columnRows;
    PostCardDelegate* m_postDelegate = nullptr;
    SmoothScrollBar* m_overlayScrollBar;
    QPropertyAnimation* m_scrollAnimation;
    QVariantAnimation* m_hoverAnimation;
    QTimer* m_resizeDebounceTimer;
    QTimer* m_previewWarmupTimer;
    QTimer* m_previewUpdateTimer;
    int m_animatedScrollValue = 0;
    int m_wheelStepPixels = 56;
    qreal m_precisionScrollScale = 0.72;
    qint64 m_lastWheelEventMs = 0;
    int m_wheelStreak = 0;
    int m_lastWheelDirection = 0;
    QModelIndex m_hoveredIndex;
    QModelIndex m_fadingIndex;
    qreal m_hoverTransitionProgress = 1.0;
    bool m_hovered = false;
    bool m_bottomSignalArmed = true;
    int m_layoutColumnWidth = 0;
    int m_layoutColumnCount = 0;

    static constexpr int kMargin = 16;
    static constexpr int kTopMargin = 0;
    static constexpr int kHorizontalGap = 12;
    static constexpr int kVerticalGap = 12;
    static constexpr int kMinItemWidth = 200;
    static constexpr int kScrollBarInset = 8;
    static constexpr int kScrollBarRightInset = 4;
    static constexpr int kResizeDebounceMs = 100;
};
