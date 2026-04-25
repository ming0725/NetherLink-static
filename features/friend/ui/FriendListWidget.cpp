#include "FriendListWidget.h"

#include <QApplication>
#include <QCursor>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QVariantAnimation>

#include "features/friend/ui/FriendListDelegate.h"
#include "features/friend/model/FriendListModel.h"
#include "features/friend/data/UserRepository.h"

extern const int kContactGroupArrowYOffset;

namespace {

constexpr int kStickyHeaderHeight = 36;
constexpr int kGroupLeftPadding = 12;
constexpr int kGroupCountRightPadding = 18;
constexpr int kGroupArrowSize = 12;

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
    connect(&UserRepository::instance(), &UserRepository::friendListChanged,
            this, &FriendListWidget::reloadFriends);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &FriendListWidget::updateStickyHeader);
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
        reloadFriends();
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
    m_state.selectedFriendId = m_model->isFriendRow(current) ? m_model->friendIdAt(current) : QString();
    emit selectedFriendChanged(m_state.selectedFriendId);
}

void FriendListWidget::reloadFriends()
{
    if (!m_state.initialized) {
        return;
    }

    m_model->setFriends(UserRepository::instance().requestFriendList({m_state.keyword}));
    restoreSelection();
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
        m_state.selectedFriendId.clear();
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    const QModelIndex index = m_model->index(row, 0);
    if (index.data(FriendListModel::GroupProgressRole).toReal() <= 0.01) {
        m_state.selectedFriendId.clear();
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
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

    if (!targetExpanded && m_model->groupIdAt(currentIndex()) == groupId) {
        clearCurrentSelection();
    }

    if (QPointer<QVariantAnimation> running = m_groupAnimations.value(groupId)) {
        running->stop();
        running->deleteLater();
    }

    m_model->setGroupExpanded(groupId, targetExpanded);
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
    clearSelection();
    setCurrentIndex(QModelIndex());
    m_state.selectedFriendId.clear();
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
