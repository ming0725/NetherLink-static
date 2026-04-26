#include "GroupListWidget.h"

#include <QApplication>
#include <QCursor>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QVariantAnimation>

#include "features/chat/data/GroupRepository.h"
#include "features/friend/model/GroupListModel.h"
#include "features/friend/ui/GroupListDelegate.h"

extern const int kContactGroupArrowYOffset;

namespace {

constexpr int kStickyHeaderHeight = 36;
constexpr int kCategoryLeftPadding = 12;
constexpr int kCategoryCountRightPadding = 18;
constexpr int kCategoryArrowSize = 12;

QFont stickyCategoryFont()
{
    QFont font = QApplication::font();
    font.setPixelSize(12);
    font.setWeight(QFont::Medium);
    return font;
}

QFont stickyCategoryCountFont()
{
    QFont font = QApplication::font();
    font.setPixelSize(11);
    font.setWeight(QFont::Medium);
    return font;
}

} // namespace

GroupListWidget::GroupListWidget(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new GroupListModel(this))
    , m_delegate(new GroupListDelegate(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setUniformItemSizes(false);
    setSpacing(0);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setAutoFillBackground(true);
    viewport()->setAutoFillBackground(true);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Window, Qt::white);
    setPalette(palette);
    viewport()->setPalette(palette);
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &GroupListWidget::onCurrentChanged);
    connect(&GroupRepository::instance(), &GroupRepository::groupListChanged,
            this, &GroupListWidget::reloadGroups);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &GroupListWidget::updateStickyHeader);
    connect(m_model, &QAbstractItemModel::modelReset,
            this, [this]() { updateStickyHeader(); });
    connect(m_model, &QAbstractItemModel::rowsInserted,
            this, [this](const QModelIndex&, int, int) { updateStickyHeader(); });
    connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, [this](const QModelIndex&, int, int) { updateStickyHeader(); });
    connect(m_model, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex&, const QModelIndex&, const QVector<int>&) {
                updateStickyHeader();
            });
}

void GroupListWidget::ensureInitialized()
{
    if (m_state.initialized) {
        return;
    }

    m_state.initialized = true;
    reloadGroups();
    updateStickyHeader();
}

void GroupListWidget::setKeyword(const QString& keyword)
{
    if (m_state.keyword == keyword) {
        return;
    }

    m_state.keyword = keyword;
    if (m_state.initialized) {
        reloadGroups();
    }
}

QString GroupListWidget::selectedGroupId() const
{
    return m_model->groupIdAt(currentIndex());
}

Group GroupListWidget::selectedGroup() const
{
    return m_model->groupAt(currentIndex());
}

void GroupListWidget::showEvent(QShowEvent* event)
{
    OverlayScrollListView::showEvent(event);
    ensureInitialized();
    updateStickyHeader();
}

void GroupListWidget::paintEvent(QPaintEvent* event)
{
    OverlayScrollListView::paintEvent(event);
    drawStickyHeader();
}

void GroupListWidget::leaveEvent(QEvent* event)
{
    OverlayScrollListView::leaveEvent(event);
    if (m_stickyVisible) {
        viewport()->update(QRect(0, 0, viewport()->width(), kStickyHeaderHeight));
    }
}

void GroupListWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const QRect stickyRect(0, m_stickyOffsetY, viewport()->width(), kStickyHeaderHeight);
    if (m_stickyVisible && !m_stickyCategory.categoryId.isEmpty() && stickyRect.contains(event->pos())) {
        toggleStickyCategoryById(m_stickyCategory.categoryId);
        event->accept();
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        clearCurrentSelection();
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    if (m_model->isCategoryRow(index)) {
        toggleCategory(index);
        event->accept();
        return;
    }

    if (m_model->isNoticeRow(index)) {
        event->accept();
        return;
    }

    if (currentIndex() == index) {
        clearCurrentSelection();
        return;
    }

    OverlayScrollListView::mousePressEvent(event);
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void GroupListWidget::mouseMoveEvent(QMouseEvent* event)
{
    OverlayScrollListView::mouseMoveEvent(event);
    if (m_stickyVisible) {
        viewport()->update(QRect(0, 0, viewport()->width(), kStickyHeaderHeight));
    }
}

void GroupListWidget::onCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    m_state.selectedGroupId = m_model->isGroupRow(current) ? m_model->groupIdAt(current) : QString();
    emit selectedGroupChanged(m_state.selectedGroupId);
}

void GroupListWidget::reloadGroups()
{
    if (!m_state.initialized) {
        return;
    }

    m_model->setGroups(GroupRepository::instance().requestGroupList({m_state.keyword}));
    restoreSelection();
    updateStickyHeader();
}

void GroupListWidget::restoreSelection()
{
    if (m_state.selectedGroupId.isEmpty()) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    const int row = m_model->indexOfGroup(m_state.selectedGroupId);
    if (row < 0) {
        m_state.selectedGroupId.clear();
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    const QModelIndex index = m_model->index(row, 0);
    if (index.data(GroupListModel::CategoryProgressRole).toReal() <= 0.01) {
        m_state.selectedGroupId.clear();
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void GroupListWidget::toggleCategory(const QModelIndex& index)
{
    toggleCategoryById(m_model->categoryIdAt(index));
}

void GroupListWidget::toggleStickyCategoryById(const QString& categoryId)
{
    const QString stableCategoryId = categoryId;
    if (stableCategoryId.isEmpty()) {
        return;
    }

    const bool expanded = m_model->isCategoryExpanded(stableCategoryId);
    const bool pinnedAtTop = isCategoryPinnedAtTop(stableCategoryId);

    if (expanded && !pinnedAtTop) {
        scrollCategoryToTop(stableCategoryId);
        collapseCategoryById(stableCategoryId, true);
        return;
    }

    toggleCategoryById(stableCategoryId, true);
}

void GroupListWidget::toggleCategoryById(const QString& categoryId, bool keepCategoryAtTop)
{
    if (categoryId.isEmpty()) {
        return;
    }

    const bool expanded = m_model->isCategoryExpanded(categoryId);
    setCategoryExpandedAnimated(categoryId, !expanded, keepCategoryAtTop);
}

void GroupListWidget::collapseCategoryById(const QString& categoryId, bool keepCategoryAtTop)
{
    setCategoryExpandedAnimated(categoryId, false, keepCategoryAtTop);
}

void GroupListWidget::setCategoryExpandedAnimated(const QString& categoryId, bool targetExpanded, bool keepCategoryAtTop)
{
    if (categoryId.isEmpty()) {
        return;
    }

    const bool expanded = m_model->isCategoryExpanded(categoryId);
    const qreal startProgress = m_model->categoryProgress(categoryId);
    const qreal endProgress = targetExpanded ? 1.0 : 0.0;
    const bool shouldKeepAtTop = keepCategoryAtTop && !targetExpanded;

    if (expanded == targetExpanded && qAbs(startProgress - endProgress) <= 0.001) {
        return;
    }

    if (QPointer<QVariantAnimation> running = m_categoryAnimations.value(categoryId)) {
        running->stop();
        running->deleteLater();
    }

    m_model->setCategoryExpanded(categoryId, targetExpanded);
    if (shouldKeepAtTop) {
        scrollCategoryToTop(categoryId);
    }

    auto* animation = new QVariantAnimation(this);
    m_categoryAnimations.insert(categoryId, animation);
    animation->setStartValue(startProgress);
    animation->setEndValue(endProgress);
    animation->setDuration(240);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this, categoryId, shouldKeepAtTop](const QVariant& value) {
        m_model->setCategoryProgress(categoryId, value.toReal());
        doItemsLayout();
        if (shouldKeepAtTop) {
            scrollCategoryToTop(categoryId);
        }
        viewport()->update();
        updateOverlayScrollBar();
        updateStickyHeader();
    });
    connect(animation, &QVariantAnimation::finished, this, [this, categoryId, endProgress, animation, shouldKeepAtTop]() {
        m_model->setCategoryProgress(categoryId, endProgress);
        m_categoryAnimations.remove(categoryId);
        animation->deleteLater();
        doItemsLayout();
        if (shouldKeepAtTop) {
            scrollCategoryToTop(categoryId);
        }
        viewport()->update();
        updateOverlayScrollBar();
        updateStickyHeader();
    });

    animation->start();
}

bool GroupListWidget::isCategoryPinnedAtTop(const QString& categoryId) const
{
    const int categoryRow = m_model->categoryRowForId(categoryId);
    if (categoryRow < 0) {
        return false;
    }

    const QRect categoryRect = visualRect(m_model->index(categoryRow, 0));
    return categoryRect.isValid() && qAbs(categoryRect.top()) <= 1;
}

void GroupListWidget::scrollCategoryToTop(const QString& categoryId)
{
    const QString stableCategoryId = categoryId;
    const int categoryRow = m_model->categoryRowForId(stableCategoryId);
    if (categoryRow < 0) {
        return;
    }

    scrollTo(m_model->index(categoryRow, 0), QAbstractItemView::PositionAtTop);
}

void GroupListWidget::clearCurrentSelection()
{
    clearSelection();
    setCurrentIndex(QModelIndex());
    m_state.selectedGroupId.clear();
}

void GroupListWidget::updateStickyHeader()
{
    const StickyHeaderState state = calculateStickyHeaderState();
    m_stickyVisible = state.visible;
    m_stickyOffsetY = state.offsetY;

    if (!m_stickyVisible) {
        m_stickyCategory = {};
        viewport()->update();
        return;
    }

    m_stickyCategory = state.category;

    viewport()->update();
}

GroupListWidget::StickyHeaderState GroupListWidget::calculateStickyHeaderState() const
{
    StickyHeaderState state;
    if (m_model->rowCount() <= 0 || verticalScrollBar()->value() <= 0) {
        return state;
    }

    QModelIndex firstVisible = indexAt(QPoint(1, 1));
    if (!firstVisible.isValid()) {
        firstVisible = m_model->index(0, 0);
    }
    if (!firstVisible.isValid()) {
        return state;
    }

    const int currentCategoryRow = m_model->categoryRowForRow(firstVisible.row());
    if (currentCategoryRow < 0) {
        return state;
    }

    state.visible = true;
    state.category = stickyCategoryDataForRow(currentCategoryRow);

    const int nextCategoryRow = m_model->nextCategoryRow(currentCategoryRow);
    if (nextCategoryRow >= 0) {
        const QRect nextRect = visualRect(m_model->index(nextCategoryRow, 0));
        if (nextRect.isValid()) {
            state.offsetY = qMin(0, nextRect.top() - kStickyHeaderHeight);
        }
    }

    return state;
}

GroupListWidget::StickyCategoryData GroupListWidget::stickyCategoryDataForRow(int row) const
{
    const QModelIndex index = m_model->index(row, 0);
    if (!index.isValid()) {
        return {};
    }

    StickyCategoryData category;
    category.categoryId = index.data(GroupListModel::CategoryIdRole).toString();
    category.title = index.data(GroupListModel::CategoryNameRole).toString();
    category.count = index.data(GroupListModel::CategoryGroupCountRole).toInt();
    category.progress = index.data(GroupListModel::CategoryProgressRole).toReal();
    return category;
}

void GroupListWidget::drawStickyHeader() const
{
    if (!m_stickyVisible || m_stickyCategory.categoryId.isEmpty()) {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(QRect(0, m_stickyOffsetY, viewport()->width(), kStickyHeaderHeight));

    const QRect headerRect(0, m_stickyOffsetY, viewport()->width(), kStickyHeaderHeight);
    painter.fillRect(headerRect, Qt::white);

    drawStickyCategory(&painter, headerRect, m_stickyCategory);
}

void GroupListWidget::drawStickyCategory(QPainter* painter,
                                         const QRect& rect,
                                         const StickyCategoryData& category) const
{
    if (category.categoryId.isEmpty()) {
        return;
    }

    painter->save();

    if (rect.contains(viewport()->mapFromGlobal(QCursor::pos()))) {
        const QRect hoverRect = rect.adjusted(6, 3, -6, -3);
        painter->setBrush(QColor(0xee, 0xee, 0xee));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(hoverRect, 6, 6);
    }

    const QRect arrowRect(rect.left() + kCategoryLeftPadding,
                          rect.top() + (rect.height() - kCategoryArrowSize) / 2
                                  + kContactGroupArrowYOffset,
                          kCategoryArrowSize,
                          kCategoryArrowSize);

    painter->save();
    painter->translate(arrowRect.center());
    painter->rotate(category.progress * 90.0);
    QPen arrowPen(QColor(0x78, 0x86, 0x94), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(arrowPen);
    painter->drawLine(QPointF(-2.5, -4.0), QPointF(2.5, 0.0));
    painter->drawLine(QPointF(2.5, 0.0), QPointF(-2.5, 4.0));
    painter->restore();

    const QFont font = stickyCategoryFont();
    const QFont countFont = stickyCategoryCountFont();
    const QFontMetrics metrics(font);
    const QFontMetrics countMetrics(countFont);
    const QString countText = QString::number(category.count);
    const int titleLeft = arrowRect.right() + 8;
    const int countWidth = countMetrics.horizontalAdvance(countText);
    const int rightEdge = rect.right() - kCategoryCountRightPadding;
    const QRect countRect(qMax(titleLeft, rightEdge - countWidth + 1),
                          rect.top(),
                          countWidth,
                          rect.height());
    const QRect titleRect(titleLeft,
                          rect.top(),
                          qMax(0, countRect.left() - titleLeft - 8),
                          rect.height());

    painter->setFont(font);
    painter->setPen(QColor(0x38, 0x45, 0x52));
    painter->drawText(titleRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      metrics.elidedText(category.title, Qt::ElideRight, titleRect.width()));
    painter->setFont(countFont);
    painter->setPen(QColor(0x99, 0xa3, 0xad));
    painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, countText);
    painter->restore();
}
