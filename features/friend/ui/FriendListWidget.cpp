#include "FriendListWidget.h"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCursor>
#include <QItemSelectionModel>
#include <QMap>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>

#include "features/friend/ui/FriendListDelegate.h"
#include "features/friend/model/FriendListModel.h"
#include "features/friend/data/UserRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "shared/services/ImageService.h"
#include "shared/ui/StyledActionMenu.h"

extern const int kContactGroupArrowYOffset;

namespace {

constexpr int kStickyHeaderHeight = 36;
constexpr int kGroupLeftPadding = 12;
constexpr int kGroupCountRightPadding = 18;
constexpr int kGroupArrowSize = 12;
constexpr int kFriendPageSize = 80;

QFont stickyGroupFont()
{
    QFont font = QApplication::font();
    font.setPixelSize(12);
    font.setWeight(QFont::Medium);
    return font;
}

QFont stickyGroupCountFont()
{
    QFont font = QApplication::font();
    font.setPixelSize(11);
    font.setWeight(QFont::Medium);
    return font;
}

} // namespace

FriendListWidget::FriendListWidget(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new FriendListModel(this))
    , m_delegate(new FriendListDelegate(this))
    , m_searchDebounceTimer(new QTimer(this))
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
            this, &FriendListWidget::onCurrentChanged);
    connect(this, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex& index) {
        if (m_model->isFriendRow(index)) {
            emit requestMessage(m_model->friendIdAt(index));
        }
    });
    connect(&UserRepository::instance(), &UserRepository::friendListChanged,
            this, &FriendListWidget::reloadFriends);
    m_searchDebounceTimer->setSingleShot(true);
    m_searchDebounceTimer->setInterval(180);

    connect(m_searchDebounceTimer, &QTimer::timeout,
            this, &FriendListWidget::reloadFriends);
    connect(&ImageService::instance(), &ImageService::previewReady,
            viewport(), QOverload<>::of(&QWidget::update));
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        updateStickyHeader();
        loadMoreForVisibleGroup();
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

void FriendListWidget::ensureInitialized()
{
    if (m_state.initialized) {
        return;
    }

    m_state.initialized = true;
    reloadFriends();
    updateStickyHeader();
}

void FriendListWidget::setKeyword(const QString& keyword)
{
    if (m_state.keyword == keyword) {
        return;
    }

    m_state.keyword = keyword;
    if (m_state.initialized) {
        m_searchDebounceTimer->start();
    }
}

QString FriendListWidget::selectedFriendId() const
{
    return m_model->friendIdAt(currentIndex());
}

FriendSummary FriendListWidget::selectedFriend() const
{
    return m_model->friendAt(currentIndex());
}

void FriendListWidget::showEvent(QShowEvent* event)
{
    OverlayScrollListView::showEvent(event);
    ensureInitialized();
    updateStickyHeader();
}

void FriendListWidget::paintEvent(QPaintEvent* event)
{
    OverlayScrollListView::paintEvent(event);
    drawStickyHeader();
}

void FriendListWidget::leaveEvent(QEvent* event)
{
    OverlayScrollListView::leaveEvent(event);
    if (m_stickyVisible) {
        viewport()->update(QRect(0, 0, viewport()->width(), kStickyHeaderHeight));
    }
}

void FriendListWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        const QModelIndex pressedIndex = indexAt(event->pos());
        if (m_model->isFriendRow(pressedIndex)) {
            showFriendMenu(event->globalPosition().toPoint(), pressedIndex);
        }
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const QRect stickyRect(0, m_stickyOffsetY, viewport()->width(), kStickyHeaderHeight);
    if (m_stickyVisible && !m_stickyGroup.groupId.isEmpty() && stickyRect.contains(event->pos())) {
        toggleStickyGroupById(m_stickyGroup.groupId);
        event->accept();
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        clearCurrentSelection();
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    if (m_model->isGroupRow(index)) {
        toggleGroup(index);
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

void FriendListWidget::mouseMoveEvent(QMouseEvent* event)
{
    OverlayScrollListView::mouseMoveEvent(event);
    if (m_stickyVisible) {
        viewport()->update(QRect(0, 0, viewport()->width(), kStickyHeaderHeight));
    }
}

void FriendListWidget::onCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    const QString nextFriendId = m_model->isFriendRow(current) ? m_model->friendIdAt(current) : QString();

    if (m_preservingSelection) {
        return;
    }

    if (nextFriendId.isEmpty()) {
        return;
    }

    m_state.selectedFriendId = nextFriendId;
    emit selectedFriendChanged(m_state.selectedFriendId);
}

void FriendListWidget::reloadFriends()
{
    if (!m_state.initialized) {
        return;
    }

    m_searchDebounceTimer->stop();
    const bool preserveLoadedItems = m_state.loadedKeyword == m_state.keyword;
    m_preservingSelection = true;
    m_model->setGroups(UserRepository::instance().requestFriendGroupSummaries({m_state.keyword}),
                       preserveLoadedItems);
    m_state.loadedKeyword = m_state.keyword;
    if (preserveLoadedItems) {
        refreshLoadedGroups();
    }
    restoreSelection();
    m_preservingSelection = false;
    updateStickyHeader();
}

void FriendListWidget::restoreSelection()
{
    if (m_state.selectedFriendId.isEmpty()) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    const int row = m_model->indexOfFriend(m_state.selectedFriendId);
    if (row < 0) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        const bool hadSelection = !m_state.selectedFriendId.isEmpty();
        m_state.selectedFriendId.clear();
        if (hadSelection) {
            emit selectedFriendChanged(QString());
        }
        return;
    }

    const QModelIndex index = m_model->index(row, 0);
    if (index.data(FriendListModel::GroupProgressRole).toReal() <= 0.01) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    const int scrollValue = verticalScrollBar()->value();
    selectionModel()->clearSelection();
    selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    verticalScrollBar()->setValue(scrollValue);
}

void FriendListWidget::refreshLoadedGroups()
{
    for (const QString& groupId : m_model->groupIds()) {
        if (!m_model->isGroupExpanded(groupId)) {
            continue;
        }

        const int currentLoadedCount = m_model->loadedFriendCount(groupId);
        const int pageSize = qMax(kFriendPageSize, currentLoadedCount);
        QVector<FriendSummary> friends = UserRepository::instance().requestFriendsInGroup({
                groupId,
                m_state.keyword,
                0,
                pageSize + 1
        });
        const bool hasMore = friends.size() > pageSize;
        if (hasMore) {
            friends.resize(pageSize);
        }
        m_model->replaceFriendsInGroup(groupId, friends, hasMore);
    }
}

void FriendListWidget::toggleGroup(const QModelIndex& index)
{
    toggleGroupById(m_model->groupIdAt(index));
}

void FriendListWidget::toggleStickyGroupById(const QString& groupId)
{
    const QString stableGroupId = groupId;
    if (stableGroupId.isEmpty()) {
        return;
    }

    const bool expanded = m_model->isGroupExpanded(stableGroupId);
    const bool pinnedAtTop = isGroupPinnedAtTop(stableGroupId);

    if (expanded && !pinnedAtTop) {
        scrollGroupToTop(stableGroupId);
        collapseGroupById(stableGroupId, true);
        return;
    }

    toggleGroupById(stableGroupId, true);
}

void FriendListWidget::toggleGroupById(const QString& groupId, bool keepGroupAtTop)
{
    if (groupId.isEmpty()) {
        return;
    }

    const bool expanded = m_model->isGroupExpanded(groupId);
    setGroupExpandedAnimated(groupId, !expanded, keepGroupAtTop);
}

void FriendListWidget::collapseGroupById(const QString& groupId, bool keepGroupAtTop)
{
    setGroupExpandedAnimated(groupId, false, keepGroupAtTop);
}

void FriendListWidget::setGroupExpandedAnimated(const QString& groupId, bool targetExpanded, bool keepGroupAtTop)
{
    if (groupId.isEmpty()) {
        return;
    }

    const bool expanded = m_model->isGroupExpanded(groupId);
    const qreal startProgress = m_model->groupProgress(groupId);
    const qreal endProgress = targetExpanded ? 1.0 : 0.0;
    const bool shouldKeepAtTop = keepGroupAtTop && !targetExpanded;

    if (expanded == targetExpanded && qAbs(startProgress - endProgress) <= 0.001) {
        return;
    }

    if (QPointer<QVariantAnimation> running = m_groupAnimations.value(groupId)) {
        running->stop();
        running->deleteLater();
    }

    const bool wasPreservingSelection = m_preservingSelection;
    m_preservingSelection = true;
    m_model->setGroupExpanded(groupId, targetExpanded);
    if (targetExpanded) {
        loadNextFriendPage(groupId);
    }
    restoreSelection();
    m_preservingSelection = wasPreservingSelection;
    if (shouldKeepAtTop) {
        scrollGroupToTop(groupId);
    }

    auto* animation = new QVariantAnimation(this);
    m_groupAnimations.insert(groupId, animation);
    animation->setStartValue(startProgress);
    animation->setEndValue(endProgress);
    animation->setDuration(240);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this, groupId, shouldKeepAtTop](const QVariant& value) {
        m_model->setGroupProgress(groupId, value.toReal());
        doItemsLayout();
        if (shouldKeepAtTop) {
            scrollGroupToTop(groupId);
        }
        viewport()->update();
        updateOverlayScrollBar();
        updateStickyHeader();
    });
    connect(animation, &QVariantAnimation::finished, this, [this, groupId, endProgress, animation, shouldKeepAtTop]() {
        m_model->setGroupProgress(groupId, endProgress);
        const bool wasPreservingSelection = m_preservingSelection;
        m_preservingSelection = true;
        if (endProgress <= 0.01) {
            m_model->pruneCollapsedRows();
        }
        restoreSelection();
        m_preservingSelection = wasPreservingSelection;
        m_groupAnimations.remove(groupId);
        animation->deleteLater();
        doItemsLayout();
        if (shouldKeepAtTop) {
            scrollGroupToTop(groupId);
        }
        viewport()->update();
        updateOverlayScrollBar();
        updateStickyHeader();
    });

    animation->start();
}

void FriendListWidget::loadNextFriendPage(const QString& groupId)
{
    if (groupId.isEmpty() || !m_model->isGroupExpanded(groupId) || !m_model->hasMoreFriends(groupId)) {
        return;
    }

    const int offset = m_model->loadedFriendCount(groupId);
    QVector<FriendSummary> friends = UserRepository::instance().requestFriendsInGroup({
            groupId,
            m_state.keyword,
            offset,
            kFriendPageSize + 1
    });
    const bool hasMore = friends.size() > kFriendPageSize;
    if (hasMore) {
        friends.resize(kFriendPageSize);
    }
    const bool wasPreservingSelection = m_preservingSelection;
    m_preservingSelection = true;
    m_model->appendFriendsToGroup(groupId, friends, hasMore);
    restoreSelection();
    m_preservingSelection = wasPreservingSelection;
    updateStickyHeader();
}

void FriendListWidget::loadMoreForVisibleGroup()
{
    if (!m_state.initialized || verticalScrollBar()->value() < verticalScrollBar()->maximum() - 160) {
        return;
    }

    QModelIndex index = indexAt(QPoint(viewport()->width() / 2, qMax(0, viewport()->height() - 2)));
    if (!index.isValid() && m_model->rowCount() > 0) {
        index = m_model->index(m_model->rowCount() - 1, 0);
    }
    const QString groupId = m_model->groupIdAt(index);
    loadNextFriendPage(groupId);
}

bool FriendListWidget::isGroupPinnedAtTop(const QString& groupId) const
{
    const int groupRow = m_model->groupRowForId(groupId);
    if (groupRow < 0) {
        return false;
    }

    const QRect groupRect = visualRect(m_model->index(groupRow, 0));
    return groupRect.isValid() && qAbs(groupRect.top()) <= 1;
}

void FriendListWidget::scrollGroupToTop(const QString& groupId)
{
    const QString stableGroupId = groupId;
    const int groupRow = m_model->groupRowForId(stableGroupId);
    if (groupRow < 0) {
        return;
    }

    scrollTo(m_model->index(groupRow, 0), QAbstractItemView::PositionAtTop);
}

void FriendListWidget::clearCurrentSelection()
{
    const bool wasPreservingSelection = m_preservingSelection;
    m_preservingSelection = true;
    clearSelection();
    setCurrentIndex(QModelIndex());
    m_preservingSelection = wasPreservingSelection;
    m_state.selectedFriendId.clear();
    emit selectedFriendChanged(QString());
}

void FriendListWidget::showFriendMenu(const QPoint& globalPos, const QModelIndex& index)
{
    const FriendSummary friendSummary = m_model->friendAt(index);
    if (friendSummary.userId.isEmpty()) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    menu->setItemHoverColor(QColor(238, 238, 238));
    m_model->setContextMenuFriend(friendSummary.userId);

    QAction* messageAction = menu->addAction(QStringLiteral("发消息"));
    connect(messageAction, &QAction::triggered, this, [this, userId = friendSummary.userId]() {
        emit requestMessage(userId);
    });

    StyledActionMenu* groupMenu = menu->addStyledMenu(QStringLiteral("添加到分组"));
    auto* actionGroup = new QActionGroup(groupMenu);
    actionGroup->setExclusive(true);

    auto addGroupAction = [this, groupMenu, actionGroup, friendSummary](const QString& groupId,
                                                                        const QString& groupName) {
        QAction* action = groupMenu->addAction(groupName);
        action->setCheckable(true);
        action->setChecked(groupId == friendSummary.groupId);
        actionGroup->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, userId = friendSummary.userId, groupId, groupName]() {
            changeFriendGroup(userId, groupId, groupName);
        });
    };

    addGroupAction(friendSummary.groupId, friendSummary.groupName);
    const QMap<QString, QString> groups = UserRepository::instance().requestFriendGroups();
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        if (it.key() == friendSummary.groupId) {
            continue;
        }
        addGroupAction(it.key(), it.value());
    }

    menu->addSeparator();

    QAction* deleteAction = menu->addAction(QStringLiteral("删除好友"));
    StyledActionMenu::setActionColors(deleteAction,
                                      QColor(235, 87, 87),
                                      QColor(255, 255, 255),
                                      QColor(235, 87, 87));
    connect(deleteAction, &QAction::triggered, this, [this, userId = friendSummary.userId]() {
        deleteFriendFromMenu(userId);
    });

    connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
        m_model->setContextMenuFriend({});
        viewport()->update();
        menu->deleteLater();
    });
    menu->popupWhenMouseReleased(globalPos);
}

void FriendListWidget::changeFriendGroup(const QString& userId,
                                         const QString& groupId,
                                         const QString& groupName)
{
    if (userId.isEmpty() || groupId.isEmpty()) {
        return;
    }

    User user = UserRepository::instance().requestUserDetail({userId});
    if (user.id.isEmpty() || user.friendGroupId == groupId) {
        return;
    }

    user.friendGroupId = groupId;
    user.friendGroupName = groupName;
    UserRepository::instance().saveUser(user);
    if (m_state.selectedFriendId == userId) {
        emit selectedFriendChanged(userId);
    }
}

void FriendListWidget::deleteFriendFromMenu(const QString& userId)
{
    if (userId.isEmpty()) {
        return;
    }

    const int result = QMessageBox::question(this,
                                             QStringLiteral("删除好友"),
                                             QStringLiteral("确认删除该好友吗？"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    const bool deletingSelected = m_state.selectedFriendId == userId;
    MessageRepository::instance().removeConversation(userId);
    UserRepository::instance().removeUser(userId);
    if (deletingSelected) {
        clearCurrentSelection();
    }
}

void FriendListWidget::updateStickyHeader()
{
    const StickyHeaderState state = calculateStickyHeaderState();
    m_stickyVisible = state.visible;
    m_stickyOffsetY = state.offsetY;

    if (!m_stickyVisible) {
        m_stickyGroup = {};
        viewport()->update();
        return;
    }

    m_stickyGroup = state.group;

    viewport()->update();
}

FriendListWidget::StickyHeaderState FriendListWidget::calculateStickyHeaderState() const
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

    const int currentGroupRow = m_model->groupRowForRow(firstVisible.row());
    if (currentGroupRow < 0) {
        return state;
    }

    state.visible = true;
    state.group = stickyGroupDataForRow(currentGroupRow);

    const int nextGroupRow = m_model->nextGroupRow(currentGroupRow);
    if (nextGroupRow >= 0) {
        const QRect nextRect = visualRect(m_model->index(nextGroupRow, 0));
        if (nextRect.isValid()) {
            state.offsetY = qMin(0, nextRect.top() - kStickyHeaderHeight);
        }
    }

    return state;
}

FriendListWidget::StickyGroupData FriendListWidget::stickyGroupDataForRow(int row) const
{
    const QModelIndex index = m_model->index(row, 0);
    if (!index.isValid()) {
        return {};
    }

    StickyGroupData group;
    group.groupId = index.data(FriendListModel::GroupIdRole).toString();
    group.title = index.data(FriendListModel::GroupNameRole).toString();
    group.count = index.data(FriendListModel::GroupFriendCountRole).toInt();
    group.progress = index.data(FriendListModel::GroupProgressRole).toReal();
    return group;
}

void FriendListWidget::drawStickyHeader() const
{
    if (!m_stickyVisible || m_stickyGroup.groupId.isEmpty()) {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(QRect(0, m_stickyOffsetY, viewport()->width(), kStickyHeaderHeight));

    const QRect headerRect(0, m_stickyOffsetY, viewport()->width(), kStickyHeaderHeight);
    painter.fillRect(headerRect, Qt::white);

    drawStickyGroup(&painter, headerRect, m_stickyGroup);
}

void FriendListWidget::drawStickyGroup(QPainter* painter,
                                       const QRect& rect,
                                       const StickyGroupData& group) const
{
    if (group.groupId.isEmpty()) {
        return;
    }

    painter->save();

    if (rect.contains(viewport()->mapFromGlobal(QCursor::pos()))) {
        const QRect hoverRect = rect.adjusted(6, 3, -6, -3);
        painter->setBrush(QColor(0xee, 0xee, 0xee));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(hoverRect, 6, 6);
    }

    const QRect arrowRect(rect.left() + kGroupLeftPadding,
                          rect.top() + (rect.height() - kGroupArrowSize) / 2
                                  + kContactGroupArrowYOffset,
                          kGroupArrowSize,
                          kGroupArrowSize);

    painter->save();
    painter->translate(arrowRect.center());
    painter->rotate(group.progress * 90.0);
    QPen arrowPen(QColor(0x78, 0x86, 0x94), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(arrowPen);
    painter->drawLine(QPointF(-2.5, -4.0), QPointF(2.5, 0.0));
    painter->drawLine(QPointF(2.5, 0.0), QPointF(-2.5, 4.0));
    painter->restore();

    const QFont font = stickyGroupFont();
    const QFont countFont = stickyGroupCountFont();
    const QFontMetrics metrics(font);
    const QFontMetrics countMetrics(countFont);
    const QString countText = QString::number(group.count);
    const int titleLeft = arrowRect.right() + 8;
    const int countWidth = countMetrics.horizontalAdvance(countText);
    const int rightEdge = rect.right() - kGroupCountRightPadding;
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
                      metrics.elidedText(group.title, Qt::ElideRight, titleRect.width()));
    painter->setFont(countFont);
    painter->setPen(QColor(0x99, 0xa3, 0xad));
    painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, countText);
    painter->restore();
}
