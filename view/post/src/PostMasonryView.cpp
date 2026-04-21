#include "PostMasonryView.h"

#include <QCursor>
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QWheelEvent>

#include "ImageService.h"
#include "PostCardDelegate.h"
#include "PostFeedModel.h"
#include "SmoothScrollBar.h"

PostMasonryView::PostMasonryView(QWidget* parent)
    : QAbstractItemView(parent)
    , m_overlayScrollBar(new SmoothScrollBar(this))
    , m_scrollAnimation(new QPropertyAnimation(this, "animatedScrollValue", this))
    , m_hoverAnimation(new QVariantAnimation(this))
    , m_resizeDebounceTimer(new QTimer(this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    viewport()->setAttribute(Qt::WA_TranslucentBackground);
    viewport()->setAutoFillBackground(false);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionMode(QAbstractItemView::NoSelection);

    m_overlayScrollBar->hide();
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_scrollAnimation->setDuration(180);
    m_resizeDebounceTimer->setSingleShot(true);
    m_resizeDebounceTimer->setInterval(kResizeDebounceMs);
    verticalScrollBar()->setSingleStep(28);
    m_hoverAnimation->setDuration(220);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_hoverTransitionProgress = value.toReal();
        if (m_hoveredIndex.isValid()) {
            viewport()->update(visualRect(m_hoveredIndex));
        }
        if (m_fadingIndex.isValid()) {
            viewport()->update(visualRect(m_fadingIndex));
        }
    });
    connect(m_hoverAnimation, &QVariantAnimation::finished, this, [this]() {
        if (m_fadingIndex.isValid()) {
            viewport()->update(visualRect(m_fadingIndex));
            m_fadingIndex = {};
        }
        if (!m_hoveredIndex.isValid()) {
            m_hoverTransitionProgress = 1.0;
        }
    });

    connect(m_overlayScrollBar, &SmoothScrollBar::valueChanged,
            this, &PostMasonryView::onOverlayScrollBarValueChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &PostMasonryView::onVerticalScrollValueChanged);
    connect(verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &PostMasonryView::onVerticalScrollRangeChanged);
    connect(&ImageService::instance(), &ImageService::previewReady,
            this, [this]() { viewport()->update(); });
    connect(m_resizeDebounceTimer, &QTimer::timeout,
            this, &PostMasonryView::onResizeDebounceTimeout);
}

void PostMasonryView::setCardDelegate(PostCardDelegate* delegate)
{
    m_postDelegate = delegate;
    setItemDelegate(delegate);
    relayout();
}

qreal PostMasonryView::hoverOverlayOpacityForIndex(const QModelIndex& index) const
{
    if (index.isValid() && index == m_hoveredIndex) {
        return m_hoverTransitionProgress;
    }
    if (index.isValid() && index == m_fadingIndex) {
        return 1.0 - m_hoverTransitionProgress;
    }
    return 0.0;
}

QRect PostMasonryView::visualRect(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_layoutItems.size()) {
        return {};
    }

    QRect rect = m_layoutItems.at(index.row()).rect;
    rect.translate(0, -verticalOffset());
    return rect;
}

void PostMasonryView::scrollTo(const QModelIndex& index, ScrollHint hint)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_layoutItems.size()) {
        return;
    }

    const QRect rect = m_layoutItems.at(index.row()).rect;
    int targetValue = verticalScrollBar()->value();

    switch (hint) {
    case PositionAtTop:
        targetValue = rect.top();
        break;
    case PositionAtBottom:
        targetValue = rect.bottom() - viewport()->height();
        break;
    default:
        if (rect.top() < verticalScrollBar()->value()) {
            targetValue = rect.top();
        } else if (rect.bottom() > verticalScrollBar()->value() + viewport()->height()) {
            targetValue = rect.bottom() - viewport()->height();
        }
        break;
    }

    verticalScrollBar()->setValue(qBound(verticalScrollBar()->minimum(),
                                         targetValue,
                                         verticalScrollBar()->maximum()));
}

QModelIndex PostMasonryView::indexAt(const QPoint& point) const
{
    if (!model()) {
        return {};
    }

    const QPoint contentPoint = point + QPoint(0, verticalOffset());
    for (int row = 0; row < m_layoutItems.size(); ++row) {
        if (m_layoutItems.at(row).rect.contains(contentPoint)) {
            return model()->index(row, 0, rootIndex());
        }
    }
    return {};
}

QModelIndex PostMasonryView::moveCursor(CursorAction cursorAction,
                                        Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);
    if (!model() || model()->rowCount(rootIndex()) == 0) {
        return {};
    }

    int row = currentIndex().isValid() ? currentIndex().row() : 0;
    if (cursorAction == MoveNext || cursorAction == MoveDown || cursorAction == MoveRight) {
        row = qMin(row + 1, model()->rowCount(rootIndex()) - 1);
    } else if (cursorAction == MovePrevious || cursorAction == MoveUp || cursorAction == MoveLeft) {
        row = qMax(row - 1, 0);
    }
    return model()->index(row, 0, rootIndex());
}

int PostMasonryView::horizontalOffset() const
{
    return 0;
}

int PostMasonryView::verticalOffset() const
{
    return verticalScrollBar()->value();
}

bool PostMasonryView::isIndexHidden(const QModelIndex& index) const
{
    Q_UNUSED(index);
    return false;
}

void PostMasonryView::setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command)
{
    Q_UNUSED(command);
    const QModelIndex index = indexAt(rect.center());
    if (index.isValid()) {
        setCurrentIndex(index);
    }
}

QRegion PostMasonryView::visualRegionForSelection(const QItemSelection& selection) const
{
    QRegion region;
    for (const QItemSelectionRange& range : selection) {
        for (int row = range.top(); row <= range.bottom(); ++row) {
            region += visualRect(model()->index(row, 0, rootIndex()));
        }
    }
    return region;
}

void PostMasonryView::resizeEvent(QResizeEvent* event)
{
    QAbstractItemView::resizeEvent(event);
    updateScrollBarGeometry();
    updateOverlayScrollBar();
    m_resizeDebounceTimer->start();
}

void PostMasonryView::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(event->rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    if (!model() || !m_postDelegate) {
        return;
    }

    const QRect visibleRect(0, verticalOffset(), viewport()->width(), viewport()->height());
    for (int row = 0; row < m_layoutItems.size(); ++row) {
        const QRect contentRect = m_layoutItems.at(row).rect;
        if (!contentRect.intersects(visibleRect)) {
            continue;
        }

        const QModelIndex index = model()->index(row, 0, rootIndex());
        QStyleOptionViewItem option = viewOptionsForIndex(index);
        option.rect = contentRect.translated(0, -verticalOffset());
        m_postDelegate->paint(&painter, option, index);
    }
}

void PostMasonryView::wheelEvent(QWheelEvent* event)
{
    if (!canScroll()) {
        event->accept();
        return;
    }

    if (!shouldUseAnimatedWheel(event)) {
        m_scrollAnimation->stop();
        handlePrecisionWheel(event);
        return;
    }

    if (event->angleDelta().y() == 0) {
        handlePrecisionWheel(event);
        return;
    }

    const int startValue = (m_scrollAnimation->state() == QPropertyAnimation::Running)
            ? m_animatedScrollValue
            : verticalScrollBar()->value();
    const int targetValue = nextAnimatedWheelTarget(event, startValue);

    m_scrollAnimation->stop();
    m_scrollAnimation->setStartValue(startValue);
    m_scrollAnimation->setEndValue(targetValue);
    m_scrollAnimation->start();

    showOverlayScrollBar();
    event->accept();
}

void PostMasonryView::mouseMoveEvent(QMouseEvent* event)
{
    QModelIndex hoveredIndex;
    PostCardDelegate::Action action = PostCardDelegate::NoAction;

    const QModelIndex index = indexAt(event->pos());
    if (index.isValid() && m_postDelegate) {
        const QStyleOptionViewItem option = viewOptionsForIndex(index);
        action = m_postDelegate->actionAt(option, index, event->pos());
        if (action == PostCardDelegate::OpenPostAction) {
            hoveredIndex = index;
        }
    }

    updateHoveredIndex(hoveredIndex);

    if (action != PostCardDelegate::NoAction) {
        viewport()->setCursor(Qt::PointingHandCursor);
    } else {
        viewport()->unsetCursor();
    }

    showOverlayScrollBar();
    QAbstractItemView::mouseMoveEvent(event);
}

void PostMasonryView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !model() || !m_postDelegate) {
        QAbstractItemView::mousePressEvent(event);
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        QAbstractItemView::mousePressEvent(event);
        return;
    }

    setCurrentIndex(index);
    const QStyleOptionViewItem option = viewOptionsForIndex(index);
    const PostCardDelegate::Action action = m_postDelegate->actionAt(option, index, event->pos());

    const QString postId = index.data(PostFeedModel::PostIdRole).toString();
    switch (action) {
    case PostCardDelegate::ToggleLikeAction:
        emit postLikeRequested(postId, !index.data(PostFeedModel::IsLikedRole).toBool());
        break;
    case PostCardDelegate::OpenAuthorAction:
        emit postAuthorActivated(index.data(PostFeedModel::AuthorIdRole).toString());
        break;
    case PostCardDelegate::OpenPostAction: {
        const auto* feedModel = qobject_cast<const PostFeedModel*>(model());
        if (!feedModel) {
            break;
        }
        emit postActivated(feedModel->postAt(index),
                           QRect(viewport()->mapToGlobal(m_postDelegate->imageRect(option, index).topLeft()),
                                 m_postDelegate->imageRect(option, index).size()));
        break;
    }
    default:
        break;
    }
}

void PostMasonryView::leaveEvent(QEvent* event)
{
    QAbstractItemView::leaveEvent(event);
    if (m_hoveredIndex.isValid()) {
        viewport()->update(visualRect(m_hoveredIndex));
        m_fadingIndex = m_hoveredIndex;
        m_hoveredIndex = {};
        m_hoverAnimation->stop();
        m_hoverTransitionProgress = 0.0;
        m_hoverAnimation->setStartValue(0.0);
        m_hoverAnimation->setEndValue(1.0);
        m_hoverAnimation->start();
    }

    const QPoint localPos = mapFromGlobal(QCursor::pos());
    if (!rect().contains(localPos) && !m_overlayScrollBar->geometry().contains(localPos)) {
        m_overlayScrollBar->startFadeOut();
    }
}

void PostMasonryView::enterEvent(QEnterEvent* event)
{
    QAbstractItemView::enterEvent(event);
    m_hovered = true;
    showOverlayScrollBar();
}

bool PostMasonryView::viewportEvent(QEvent* event)
{
    switch (event->type()) {
    case QEvent::Enter:
    case QEvent::MouseMove:
        m_hovered = true;
        showOverlayScrollBar();
        break;
    case QEvent::Leave: {
        m_hovered = false;
        const QPoint localPos = mapFromGlobal(QCursor::pos());
        if (!rect().contains(localPos) && !m_overlayScrollBar->geometry().contains(localPos)) {
            m_overlayScrollBar->startFadeOut();
        }
        break;
    }
    default:
        break;
    }
    return QAbstractItemView::viewportEvent(event);
}

void PostMasonryView::rowsInserted(const QModelIndex& parent, int start, int end)
{
    QAbstractItemView::rowsInserted(parent, start, end);
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    relayout();
}

void PostMasonryView::rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    relayout();
}

void PostMasonryView::dataChanged(const QModelIndex& topLeft,
                                  const QModelIndex& bottomRight,
                                  const QList<int>& roles)
{
    QAbstractItemView::dataChanged(topLeft, bottomRight, roles);
    Q_UNUSED(roles);
    relayout();
    viewport()->update();
}

void PostMasonryView::updateGeometries()
{
    QAbstractItemView::updateGeometries();
    relayout();
}

void PostMasonryView::onOverlayScrollBarValueChanged(int value)
{
    if (verticalScrollBar()->value() != value) {
        verticalScrollBar()->setValue(value);
    }
}

void PostMasonryView::onVerticalScrollValueChanged(int value)
{
    m_animatedScrollValue = value;
    viewport()->update();
    updateOverlayScrollBar();
    warmVisiblePreviews();
    maybeEmitReachedBottom();
}

void PostMasonryView::onVerticalScrollRangeChanged(int minimum, int maximum)
{
    Q_UNUSED(minimum);
    Q_UNUSED(maximum);
    updateOverlayScrollBar();
}

void PostMasonryView::onResizeDebounceTimeout()
{
    relayout();
}

void PostMasonryView::setAnimatedScrollValue(int value)
{
    if (m_animatedScrollValue == value) {
        return;
    }

    m_animatedScrollValue = value;
    if (verticalScrollBar()->value() != value) {
        verticalScrollBar()->setValue(value);
    }
}

void PostMasonryView::relayout()
{
    m_layoutItems.clear();
    if (!model() || !m_postDelegate) {
        verticalScrollBar()->setRange(0, 0);
        viewport()->update();
        updateOverlayScrollBar();
        return;
    }

    const int rowCount = model()->rowCount(rootIndex());
    if (rowCount <= 0 || viewport()->width() <= 0) {
        verticalScrollBar()->setRange(0, 0);
        viewport()->update();
        updateOverlayScrollBar();
        return;
    }

    const int availableWidth = viewport()->width() - 2 * kMargin;
    const int columns = qMax(1, (availableWidth + kHorizontalGap) / (kMinItemWidth + kHorizontalGap));
    const int itemWidth = qMax(kMinItemWidth,
                               qRound(qreal(availableWidth - (columns - 1) * kHorizontalGap) / qreal(columns)));
    QVector<int> columnHeights(columns, kTopMargin);
    m_layoutItems.resize(rowCount);

    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex index = model()->index(row, 0, rootIndex());
        const int height = m_postDelegate->heightForWidth(index, itemWidth, devicePixelRatioF());
        const int column = std::distance(columnHeights.begin(),
                                         std::min_element(columnHeights.begin(), columnHeights.end()));
        const int x = kMargin + column * (itemWidth + kHorizontalGap);
        const int y = columnHeights[column];
        m_layoutItems[row] = {QRect(x, y, itemWidth, height), height};
        columnHeights[column] += height + kVerticalGap;
    }

    const int contentHeight = *std::max_element(columnHeights.begin(), columnHeights.end()) + kMargin;
    const int maximum = qMax(0, contentHeight - viewport()->height());
    verticalScrollBar()->setPageStep(viewport()->height());
    {
        QSignalBlocker blocker(verticalScrollBar());
        verticalScrollBar()->setRange(0, maximum);
        verticalScrollBar()->setValue(qBound(0, verticalScrollBar()->value(), maximum));
    }

    updateScrollBarGeometry();
    updateOverlayScrollBar();
    warmVisiblePreviews();
    viewport()->update();
}

void PostMasonryView::updateHoveredIndex(const QModelIndex& index)
{
    if (index == m_hoveredIndex) {
        return;
    }

    if (m_hoveredIndex.isValid()) {
        m_fadingIndex = m_hoveredIndex;
    }
    m_hoveredIndex = index;
    m_hoverAnimation->stop();
    m_hoverTransitionProgress = 0.0;
    m_hoverAnimation->setStartValue(0.0);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->start();
    if (m_hoveredIndex.isValid()) {
        viewport()->update(visualRect(m_hoveredIndex));
    }
    if (m_fadingIndex.isValid()) {
        viewport()->update(visualRect(m_fadingIndex));
    }
}

void PostMasonryView::warmVisiblePreviews()
{
    if (!model() || !m_postDelegate) {
        return;
    }

    const QRect visibleRect(0, verticalOffset(), viewport()->width(), viewport()->height());
    const qreal dpr = devicePixelRatioF();
    for (int row = 0; row < m_layoutItems.size(); ++row) {
        const QRect contentRect = m_layoutItems.at(row).rect;
        if (!contentRect.intersects(visibleRect)) {
            continue;
        }

        const QModelIndex index = model()->index(row, 0, rootIndex());
        const QStyleOptionViewItem option = viewOptionsForIndex(index);
        const QString source = index.data(PostFeedModel::ThumbnailImageRole).toString();
        if (!source.isEmpty()) {
            ImageService::instance().requestPreviewWarmup(source,
                                                          m_postDelegate->imageRect(option, index).size(),
                                                          dpr);
        }
    }
}

void PostMasonryView::updateScrollBarGeometry()
{
    const int barHeight = qMax(0, height() - 2 * kScrollBarInset);
    m_overlayScrollBar->setGeometry(width() - m_overlayScrollBar->width() - kScrollBarRightInset,
                                    kScrollBarInset,
                                    m_overlayScrollBar->width(),
                                    barHeight);
    m_overlayScrollBar->raise();
}

void PostMasonryView::updateOverlayScrollBar()
{
    m_overlayScrollBar->setRange(verticalScrollBar()->minimum(), verticalScrollBar()->maximum());
    m_overlayScrollBar->setPageStep(verticalScrollBar()->pageStep());
    m_overlayScrollBar->setValue(verticalScrollBar()->value());
    updateScrollBarGeometry();

    if (!canScroll()) {
        m_overlayScrollBar->hide();
    } else if (m_hovered || m_overlayScrollBar->isDragging()) {
        m_overlayScrollBar->showScrollBar();
    }
}

void PostMasonryView::showOverlayScrollBar()
{
    updateOverlayScrollBar();
    if (canScroll()) {
        m_overlayScrollBar->showScrollBar();
    }
}

bool PostMasonryView::canScroll() const
{
    return verticalScrollBar()->maximum() > verticalScrollBar()->minimum();
}

bool PostMasonryView::shouldUseAnimatedWheel(const QWheelEvent* event) const
{
#ifdef Q_OS_MACOS
    Q_UNUSED(event);
    return false;
#else
    return event->pixelDelta().isNull() && event->phase() == Qt::NoScrollPhase;
#endif
}

void PostMasonryView::handlePrecisionWheel(QWheelEvent* event)
{
    int scrollDelta = 0;
    if (!event->pixelDelta().isNull()) {
        scrollDelta = qRound(-event->pixelDelta().y() * m_precisionScrollScale);
    } else if (event->angleDelta().y() != 0) {
        const qreal steps = qreal(event->angleDelta().y()) / 120.0;
        scrollDelta = qRound(-steps * m_wheelStepPixels * m_precisionScrollScale);
    }

    if (scrollDelta != 0) {
        verticalScrollBar()->setValue(qBound(verticalScrollBar()->minimum(),
                                             verticalScrollBar()->value() + scrollDelta,
                                             verticalScrollBar()->maximum()));
    }

    showOverlayScrollBar();
    event->accept();
}

int PostMasonryView::nextAnimatedWheelTarget(const QWheelEvent* event, int startValue)
{
    const int delta = event->angleDelta().y();
    const int direction = (delta > 0) ? 1 : -1;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    if (m_lastWheelDirection == direction && (nowMs - m_lastWheelEventMs) <= 180) {
        ++m_wheelStreak;
    } else {
        m_wheelStreak = 1;
    }

    m_lastWheelDirection = direction;
    m_lastWheelEventMs = nowMs;

    const qreal steps = qAbs(qreal(delta) / 120.0);
    const qreal acceleration = qMin(2.0, 1.0 + 0.16 * (m_wheelStreak - 1));
    const int scrollDelta = -direction * qRound(steps * m_wheelStepPixels * acceleration);
    const int tailDistance = -direction * qMin(24, qRound((8.0 + 4.0 * steps) * acceleration));

    return qBound(verticalScrollBar()->minimum(),
                  startValue + scrollDelta + tailDistance,
                  verticalScrollBar()->maximum());
}

QStyleOptionViewItem PostMasonryView::viewOptionsForIndex(const QModelIndex& index) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Enabled;
    if (hoverOverlayOpacityForIndex(index) > 0.0) {
        option.state |= QStyle::State_MouseOver;
    }
    option.widget = viewport();
    option.rect = visualRect(index);
    return option;
}

QRect PostMasonryView::itemRectInViewport(const QModelIndex& index) const
{
    return visualRect(index);
}

int PostMasonryView::totalContentHeight() const
{
    if (m_layoutItems.isEmpty()) {
        return 0;
    }
    int maxBottom = 0;
    for (const LayoutItem& item : m_layoutItems) {
        maxBottom = qMax(maxBottom, item.rect.bottom());
    }
    return maxBottom + kMargin;
}

void PostMasonryView::maybeEmitReachedBottom()
{
    if (!canScroll()) {
        m_bottomSignalArmed = true;
        return;
    }

    const int remaining = verticalScrollBar()->maximum() - verticalScrollBar()->value();
    if (remaining <= 240) {
        if (m_bottomSignalArmed) {
            m_bottomSignalArmed = false;
            emit reachedBottom();
        }
    } else {
        m_bottomSignalArmed = true;
    }
}
