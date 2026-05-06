#include "GroupListWidget.h"

#include <QAbstractItemView>
#include <QActionGroup>
#include <QApplication>
#include <QCursor>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStringList>
#include <QTimer>
#include <QVariantAnimation>

#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/model/GroupListModel.h"
#include "features/friend/ui/GroupListDelegate.h"
#include "shared/services/ImageService.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/theme/ThemeManager.h"

extern const int kContactGroupArrowYOffset;

namespace {

constexpr int kStickyHeaderHeight = 36;
constexpr int kCategoryLeftPadding = 12;
constexpr int kCategoryCountRightPadding = 18;
constexpr int kCategoryArrowSize = 12;
constexpr int kGroupPageSize = 80;

const QStringList& editableCategoryOrder()
{
    static const QStringList ids = {
            QStringLiteral("gg_joined"),
            QStringLiteral("gg_college"),
            QStringLiteral("gg_work"),
            QStringLiteral("gg_performance")
    };
    return ids;
}

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
    , m_searchDebounceTimer(new QTimer(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setUniformItemSizes(false);
    setSpacing(0);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    refreshTheme();
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &GroupListWidget::onCurrentChanged);
    connect(this, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex& index) {
        if (m_model->isGroupRow(index)) {
            emit requestMessage(m_model->groupIdAt(index));
        }
    });
    connect(&GroupRepository::instance(), &GroupRepository::groupListChanged,
            this, &GroupListWidget::reloadGroups);
    m_searchDebounceTimer->setSingleShot(true);
    m_searchDebounceTimer->setInterval(180);

    connect(m_searchDebounceTimer, &QTimer::timeout,
            this, &GroupListWidget::reloadGroups);
    connect(&ImageService::instance(), &ImageService::previewReady,
            viewport(), QOverload<>::of(&QWidget::update));
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        updateStickyHeader();
        loadMoreForVisibleCategory();
    });
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
        m_searchDebounceTimer->start();
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

void GroupListWidget::setNoticeSelected(bool selected)
{
    m_model->setNoticeSelected(selected);
    if (selected) {
        m_suppressGroupChangeSignal = true;
        clearCurrentSelection();
        m_suppressGroupChangeSignal = false;
    }
    viewport()->update();
}

void GroupListWidget::setNoticeUnreadCount(int count)
{
    m_model->setNoticeUnreadCount(count);
    viewport()->update();
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
    if (event->button() == Qt::RightButton) {
        const QModelIndex pressedIndex = indexAt(event->pos());
        if (m_model->isGroupRow(pressedIndex)) {
            showGroupMenu(event->globalPosition().toPoint(), pressedIndex);
        }
        event->accept();
        return;
    }

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
        emit noticeClicked();
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
    m_model->setHoverSuppressedGroup({});
    viewport()->update();
    OverlayScrollListView::mouseMoveEvent(event);
    if (m_stickyVisible) {
        viewport()->update(QRect(0, 0, viewport()->width(), kStickyHeaderHeight));
    }
}

void GroupListWidget::onCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    const QString nextGroupId = m_model->isGroupRow(current) ? m_model->groupIdAt(current) : QString();

    if (m_preservingSelection) {
        return;
    }

    if (nextGroupId.isEmpty()) {
        return;
    }

    m_state.selectedGroupId = nextGroupId;
    emit selectedGroupChanged(m_state.selectedGroupId);
}

void GroupListWidget::reloadGroups()
{
    if (!m_state.initialized) {
        return;
    }

    m_searchDebounceTimer->stop();
    const bool preserveLoadedItems = m_state.loadedKeyword == m_state.keyword;
    m_preservingSelection = true;
    m_model->setCategories(GroupRepository::instance().requestGroupCategorySummaries({m_state.keyword}),
                           preserveLoadedItems);
    m_state.loadedKeyword = m_state.keyword;
    restoreSelection();
    m_preservingSelection = false;
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
        clearSelection();
        setCurrentIndex(QModelIndex());
        const Group selectedGroup = GroupRepository::instance().requestGroupDetail({m_state.selectedGroupId});
        if (selectedGroup.groupId.isEmpty()) {
            m_state.selectedGroupId.clear();
            emit selectedGroupChanged(QString());
        }
        return;
    }

    const QModelIndex index = m_model->index(row, 0);
    if (index.data(GroupListModel::CategoryProgressRole).toReal() <= 0.01) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    const int scrollValue = verticalScrollBar()->value();
    selectionModel()->clearSelection();
    selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    verticalScrollBar()->setValue(scrollValue);
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

    const bool wasPreservingSelection = m_preservingSelection;
    m_preservingSelection = true;
    m_model->setCategoryExpanded(categoryId, targetExpanded);
    if (targetExpanded) {
        loadNextGroupPage(categoryId);
    }
    restoreSelection();
    m_preservingSelection = wasPreservingSelection;
    if (shouldKeepAtTop) {
        scrollCategoryToTop(categoryId);
    }

    auto* animation = new QVariantAnimation(this);
    m_categoryAnimations.insert(categoryId, animation);
    animation->setStartValue(startProgress);
    animation->setEndValue(endProgress);
    animation->setDuration(420);
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
        const bool wasPreservingSelection = m_preservingSelection;
        m_preservingSelection = true;
        if (endProgress <= 0.01) {
            m_model->pruneCollapsedRows();
        }
        restoreSelection();
        m_preservingSelection = wasPreservingSelection;
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

void GroupListWidget::loadNextGroupPage(const QString& categoryId)
{
    if (categoryId.isEmpty() || !m_model->isCategoryExpanded(categoryId) || !m_model->hasMoreGroups(categoryId)) {
        return;
    }

    const int offset = m_model->loadedGroupCount(categoryId);
    QVector<Group> groups = GroupRepository::instance().requestGroupsInCategory({
            categoryId,
            m_state.keyword,
            offset,
            kGroupPageSize + 1
    });
    const bool hasMore = groups.size() > kGroupPageSize;
    if (hasMore) {
        groups.resize(kGroupPageSize);
    }
    const bool wasPreservingSelection = m_preservingSelection;
    m_preservingSelection = true;
    m_model->appendGroupsToCategory(categoryId, groups, hasMore);
    restoreSelection();
    m_preservingSelection = wasPreservingSelection;
    updateStickyHeader();
}

void GroupListWidget::loadMoreForVisibleCategory()
{
    if (!m_state.initialized || verticalScrollBar()->value() < verticalScrollBar()->maximum() - 160) {
        return;
    }

    QModelIndex index = indexAt(QPoint(viewport()->width() / 2, qMax(0, viewport()->height() - 2)));
    if (!index.isValid() && m_model->rowCount() > 0) {
        index = m_model->index(m_model->rowCount() - 1, 0);
    }
    const QString categoryId = m_model->categoryIdAt(index);
    loadNextGroupPage(categoryId);
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
    const bool wasPreservingSelection = m_preservingSelection;
    m_preservingSelection = true;
    clearSelection();
    setCurrentIndex(QModelIndex());
    m_preservingSelection = wasPreservingSelection;
    m_state.selectedGroupId.clear();
    if (!m_suppressGroupChangeSignal) {
        emit selectedGroupChanged(QString());
    }
}

void GroupListWidget::showGroupMenu(const QPoint& globalPos, const QModelIndex& index)
{
    const Group group = m_model->groupAt(index);
    if (group.groupId.isEmpty()) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    menu->setItemHoverColor(QColor(238, 238, 238));
    m_model->setHoverSuppressedGroup({});

    QAction* messageAction = menu->addAction(QStringLiteral("发消息"));
    connect(messageAction, &QAction::triggered, this, [this, groupId = group.groupId]() {
        emit requestMessage(groupId);
    });

    StyledActionMenu* categoryMenu = menu->addStyledMenu(QStringLiteral("添加到分组"));
    auto* actionGroup = new QActionGroup(categoryMenu);
    actionGroup->setExclusive(true);

    const QMap<QString, QString> categories = GroupRepository::instance().requestGroupCategories();
    const QString currentCategoryId = group.listGroupId.isEmpty()
            ? QStringLiteral("gg_joined")
            : group.listGroupId;

    auto addCategoryAction = [this, categoryMenu, actionGroup, group, currentCategoryId]
            (const QString& categoryId, const QString& categoryName) {
        QAction* action = categoryMenu->addAction(categoryName);
        action->setCheckable(true);
        action->setChecked(categoryId == currentCategoryId);
        actionGroup->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, groupId = group.groupId, categoryId, categoryName]() {
            changeGroupCategory(groupId, categoryId, categoryName);
        });
    };

    for (const QString& categoryId : editableCategoryOrder()) {
        addCategoryAction(categoryId, categories.value(categoryId));
    }

    menu->addSeparator();

    QAction* exitAction = menu->addAction(QStringLiteral("退出群聊"));
    const bool canExit = !GroupRepository::instance().isCurrentUserGroupOwner(group);
    exitAction->setEnabled(canExit);
    if (canExit) {
        StyledActionMenu::setActionColors(exitAction,
                                          QColor(235, 87, 87),
                                          QColor(255, 255, 255),
                                          QColor(235, 87, 87));
    }
    connect(exitAction, &QAction::triggered, this, [this, groupId = group.groupId]() {
        exitGroupFromMenu(groupId);
    });

    connect(menu, &QMenu::aboutToHide, this, [this, menu, groupId = group.groupId]() {
        m_model->setHoverSuppressedGroup(groupId);
        viewport()->update();
        menu->deleteLater();
    });
    menu->popupWhenMouseReleased(globalPos);
}

void GroupListWidget::changeGroupCategory(const QString& groupId,
                                          const QString& categoryId,
                                          const QString& categoryName)
{
    if (groupId.isEmpty() || categoryId.isEmpty()) {
        return;
    }

    Group group = GroupRepository::instance().requestGroupDetail({groupId});
    const QString currentCategoryId = group.listGroupId.isEmpty()
            ? QStringLiteral("gg_joined")
            : group.listGroupId;
    if (group.groupId.isEmpty() || categoryId == currentCategoryId) {
        return;
    }

    group.listGroupId = categoryId;
    group.listGroupName = categoryName;
    GroupRepository::instance().saveGroup(group);
    if (m_state.selectedGroupId == groupId) {
        emit selectedGroupChanged(groupId);
    }
}

void GroupListWidget::exitGroupFromMenu(const QString& groupId)
{
    if (groupId.isEmpty()) {
        return;
    }

    const Group group = GroupRepository::instance().requestGroupDetail({groupId});
    if (group.groupId.isEmpty() || GroupRepository::instance().isCurrentUserGroupOwner(group)) {
        return;
    }

    const int result = QMessageBox::question(this,
                                             QStringLiteral("退出群聊"),
                                             QStringLiteral("确认退出该群聊吗？"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    const bool exitingSelected = m_state.selectedGroupId == groupId;
    MessageRepository::instance().removeConversation(groupId);
    GroupRepository::instance().removeGroup(groupId);
    if (exitingSelected) {
        clearCurrentSelection();
    }
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
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setClipRect(QRect(0, m_stickyOffsetY, viewport()->width(), kStickyHeaderHeight));

    const QRect headerRect(0, m_stickyOffsetY, viewport()->width(), kStickyHeaderHeight);
    painter.fillRect(headerRect, ThemeManager::instance().color(ThemeColor::PanelBackground));

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
        painter->setBrush(ThemeManager::instance().color(ThemeColor::ListHover));
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
    QPen arrowPen(ThemeManager::instance().color(ThemeColor::TertiaryText),
                  1.5,
                  Qt::SolidLine,
                  Qt::RoundCap,
                  Qt::RoundJoin);
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
    painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
    painter->drawText(titleRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      metrics.elidedText(category.title, Qt::ElideRight, titleRect.width()));
    painter->setFont(countFont);
    painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
    painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, countText);
    painter->restore();
}
