#include "AiChatMessageListView.h"

#include <algorithm>
#include <cmath>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDebug>
#include <QDesktopServices>
#include <QEasingCurve>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QUrl>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "AiChatMessageDelegate.h"
#include "features/aichat/model/AiChatMessageListModel.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/StyledActionMenu.h"

namespace {

constexpr int kUserCopyFadeDurationMs = 160;
constexpr int kUserMessageExpandDurationMs = 420;
constexpr int kCodeBlockLoadingFrameMs = 16;
constexpr bool kAiChatLayoutDebug = false;
constexpr int kViewPaintWidthInset = 4;

QString rectDebugString(const QRect& rect)
{
    return QStringLiteral("(%1,%2 %3x%4)")
            .arg(rect.x())
            .arg(rect.y())
            .arg(rect.width())
            .arg(rect.height());
}

QString pointDebugString(const QPoint& point)
{
    return QStringLiteral("(%1,%2)").arg(point.x()).arg(point.y());
}

QString messageActionDebugString(AiChatMessageDelegate::MessageAction action)
{
    switch (action) {
    case AiChatMessageDelegate::MessageAction::None:
        return QStringLiteral("None");
    case AiChatMessageDelegate::MessageAction::Copy:
        return QStringLiteral("Copy");
    case AiChatMessageDelegate::MessageAction::Refresh:
        return QStringLiteral("Refresh");
    case AiChatMessageDelegate::MessageAction::Like:
        return QStringLiteral("Like");
    case AiChatMessageDelegate::MessageAction::Dislike:
        return QStringLiteral("Dislike");
    case AiChatMessageDelegate::MessageAction::ToggleExpansion:
        return QStringLiteral("ToggleExpansion");
    }
    return QStringLiteral("Unknown");
}

QPoint mouseGlobalPosition(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

int scrollAnimationDuration(int distance, int viewportHeight, bool accelerateFarDistance)
{
    if (distance <= 0) {
        return 0;
    }

    constexpr double kMinDurationMs = 200.0;
    constexpr double kMaxDurationMs = 800.0;
    constexpr double kDistanceFactor = 0.5;

    const double normalized = std::min(distance / 1000.0, 1.0);
    const double curved = std::pow(normalized, kDistanceFactor);
    const double duration = kMaxDurationMs * curved;
    const int normalDuration = static_cast<int>(std::clamp(duration, kMinDurationMs, kMaxDurationMs));

    if (!accelerateFarDistance) {
        return normalDuration;
    }

    const int farDistanceThreshold = std::max(viewportHeight * 2, 1000);
    if (distance <= farDistanceThreshold) {
        return normalDuration;
    }

    constexpr double kFastestDurationScale = 0.35;
    const double farProgress = std::clamp(
        static_cast<double>(distance - farDistanceThreshold) /
            static_cast<double>(farDistanceThreshold * 2),
        0.0,
        1.0);
    const double scale = 1.0 - (1.0 - kFastestDurationScale) * farProgress;
    return static_cast<int>(std::clamp(std::round(normalDuration * scale),
                                       kMinDurationMs,
                                       static_cast<double>(normalDuration)));
}

} // namespace

AiChatMessageListView::AiChatMessageListView(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_delegate(new AiChatMessageDelegate(this))
    , m_scrollAnimation(new QPropertyAnimation(verticalScrollBar(), "value", this))
    , m_copyResetTimer(new QTimer(this))
    , m_codeBlockLoadingTimer(new QTimer(this))
{
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformItemSizes(false);
    setLayoutMode(QListView::Batched);
    setBatchSize(32);
    setSpacing(0);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setThemeBackgroundRole(ThemeColor::PanelBackground);
    setWheelStepPixels(72);
    setScrollBarInsets(8, 4);

    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_copyResetTimer->setSingleShot(true);
    m_codeBlockLoadingTimer->setTimerType(Qt::PreciseTimer);
    m_codeBlockLoadingTimer->setInterval(kCodeBlockLoadingFrameMs);
    connect(m_copyResetTimer, &QTimer::timeout, this, &AiChatMessageListView::clearCopiedCodeBlock);
    connect(m_codeBlockLoadingTimer, &QTimer::timeout,
            this, &AiChatMessageListView::updateCodeBlockLoadingAnimation);
    connect(m_delegate, &AiChatMessageDelegate::streamingMessageIdChanged,
            this, &AiChatMessageListView::syncCodeBlockLoadingAnimationTimer);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &AiChatMessageListView::onScrollValueChanged);
}

AiChatMessageDelegate* AiChatMessageListView::messageDelegate() const
{
    return m_delegate;
}

bool AiChatMessageListView::isBottomLocked() const
{
    return m_stickToBottom;
}

void AiChatMessageListView::scrollToBottom(bool accelerateFarDistance)
{
    m_stickToBottom = true;
    scheduleScrollToBottom(true, accelerateFarDistance);
}

void AiChatMessageListView::scrollToBottomIfLocked(bool accelerateFarDistance)
{
    if (!m_stickToBottom) {
        return;
    }

    scheduleScrollToBottom(false, accelerateFarDistance);
}

void AiChatMessageListView::jumpToBottom()
{
    m_scrollToBottomPending = false;
    m_forcePendingScrollToBottom = false;
    m_pendingScrollAccelerateFarDistance = false;
    m_scrollAnimation->stop();
    doItemsLayout();
    updateGeometries();
    setScrollBarToBottom();
}

void AiChatMessageListView::scheduleScrollToBottom(bool force, bool accelerateFarDistance)
{
    if (force) {
        m_forcePendingScrollToBottom = true;
    }
    if (accelerateFarDistance) {
        m_pendingScrollAccelerateFarDistance = true;
    }

    if (m_scrollToBottomPending) {
        return;
    }

    m_scrollToBottomPending = true;
    QTimer::singleShot(0, this, [this]() {
        const bool force = m_forcePendingScrollToBottom;
        const bool accelerateFarDistance = m_pendingScrollAccelerateFarDistance;
        m_scrollToBottomPending = false;
        m_forcePendingScrollToBottom = false;
        m_pendingScrollAccelerateFarDistance = false;
        if (!force && !m_stickToBottom) {
            return;
        }

        doItemsLayout();
        updateGeometries();
        if (force || m_stickToBottom) {
            animateScrollToBottom(accelerateFarDistance);
        }
    });
}

void AiChatMessageListView::animateScrollToBottom(bool accelerateFarDistance)
{
    QScrollBar* scrollBar = verticalScrollBar();
    const int targetValue = scrollBar->maximum();
    const int startValue = scrollBar->value();

    if (targetValue <= scrollBar->minimum() || startValue >= targetValue) {
        setScrollBarToBottom();
        return;
    }

    m_scrollAnimation->stop();
    m_scrollAnimation->setDuration(scrollAnimationDuration(targetValue - startValue,
                                                           viewport()->height(),
                                                           accelerateFarDistance));
    m_scrollAnimation->setStartValue(startValue);
    m_scrollAnimation->setEndValue(targetValue);
    m_scrollAnimation->start();
}

void AiChatMessageListView::clearTextSelection()
{
    if (!m_delegate->hasSelection() &&
            !m_delegate->selectionIndex().isValid() &&
            !m_delegate->hasBubbleSelection()) {
        return;
    }

    m_delegate->clearSelection();
    m_delegate->clearBubbleSelection();
    viewport()->update();
}

void AiChatMessageListView::setBottomViewportMargin(int margin)
{
    const bool shouldKeepBottom = m_stickToBottom;
    if (auto* messageModel = qobject_cast<AiChatMessageListModel*>(model())) {
        messageModel->setBottomSpaceHeight(qMax(0, margin));
        doItemsLayout();
        updateGeometries();
    }
    if (shouldKeepBottom) {
        setScrollBarToBottom();
    }
    updateOverlayScrollBar();
}

void AiChatMessageListView::refreshMessageLayout()
{
    const bool shouldKeepBottom = m_stickToBottom;
    doItemsLayout();
    updateGeometries();
    if (shouldKeepBottom) {
        setScrollBarToBottom();
    }
    updateOverlayScrollBar();
    viewport()->update();
}

void AiChatMessageListView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_PageUp:
    case Qt::Key_Home:
        emit userScrollUpIntent();
        unlockBottomLockForUserScrollUp();
        break;
    case Qt::Key_Down:
    case Qt::Key_PageDown:
    case Qt::Key_End:
        emit userScrollDownIntent();
        break;
    default:
        break;
    }

    if (event->matches(QKeySequence::SelectAll)) {
        selectAllTextInActiveBubble();
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Copy)) {
        copySelectionToClipboard();
        event->accept();
        return;
    }

    OverlayScrollListView::keyPressEvent(event);
}

void AiChatMessageListView::wheelEvent(QWheelEvent* event)
{
    if (hasUpwardScrollIntent(event)) {
        emit userScrollUpIntent();
        unlockBottomLockForUserScrollUp();
    } else if (hasDownwardScrollIntent(event)) {
        emit userScrollDownIntent();
    }

    OverlayScrollListView::wheelEvent(event);
}

void AiChatMessageListView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            const QString url = m_delegate->urlAt(option, index, event->pos());
            if (!url.isEmpty()) {
                m_delegate->clearBubbleSelection();
                viewport()->update();
                showUrlMenu(mouseGlobalPosition(event), url);
                event->accept();
                return;
            }

            const int cursor = m_delegate->characterIndexAt(option, index, event->pos());
            if (m_delegate->selectionContains(index, cursor)) {
                showSelectionMenu(mouseGlobalPosition(event));
                event->accept();
                return;
            }

            if (m_delegate->bubbleHitTest(option, index, event->pos())) {
                setFocus(Qt::MouseFocusReason);
                m_activeBubbleIndex = QPersistentModelIndex(index);
                m_delegate->setBubbleSelection(index);
                viewport()->update();
                showBubbleMenu(mouseGlobalPosition(event), index);
                event->accept();
                return;
            }
        }

        clearTextSelection();
    }

    if (event->button() == Qt::LeftButton) {
        m_pressedUrlIndex = QPersistentModelIndex();
        m_pressedUrl.clear();

        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            const bool hitBubble = m_delegate->bubbleHitTest(option, index, event->pos());
            if (hitBubble) {
                setFocus(Qt::MouseFocusReason);
                m_activeBubbleIndex = QPersistentModelIndex(index);
            }

            const AiChatMessageDelegate::MessageAction messageAction =
                    m_delegate->messageActionAt(option, index, event->pos());
            if (kAiChatLayoutDebug && index.data(AiChatMessageListModel::IsFromUserRole).toBool()) {
                qDebug().noquote()
                        << "AICHAT_LAYOUT mousePress"
                        << "row=" << index.row()
                        << "id=" << index.data(AiChatMessageListModel::MessageIdRole).toString()
                        << "pos=" << pointDebugString(event->pos())
                        << "visual=" << rectDebugString(visualRect(index))
                        << "option=" << rectDebugString(option.rect)
                        << "hitBubble=" << hitBubble
                        << "action=" << messageActionDebugString(messageAction)
                        << "scroll=" << verticalScrollBar()->value()
                        << "max=" << verticalScrollBar()->maximum();
            }
            if (messageAction != AiChatMessageDelegate::MessageAction::None) {
                const QString messageId = index.data(AiChatMessageListModel::MessageIdRole).toString();
                switch (messageAction) {
                case AiChatMessageDelegate::MessageAction::Copy:
                    copyBubbleToClipboard(index);
                    GlobalNotification::showSuccess(this, QStringLiteral("复制成功"));
                    break;
                case AiChatMessageDelegate::MessageAction::ToggleExpansion:
                    toggleUserMessageExpansion(index);
                    break;
                case AiChatMessageDelegate::MessageAction::Refresh:
                    emit regenerateAiReplyRequested(
                            index.data(AiChatMessageListModel::ConversationIdRole).toString(),
                            messageId);
                    break;
                case AiChatMessageDelegate::MessageAction::Like: {
                    const AiChatMessageDelegate::MessageFeedback current =
                            m_delegate->messageFeedback(messageId);
                    m_delegate->setMessageFeedback(
                            messageId,
                            current == AiChatMessageDelegate::MessageFeedback::Liked
                                    ? AiChatMessageDelegate::MessageFeedback::None
                                    : AiChatMessageDelegate::MessageFeedback::Liked);
                    viewport()->update(visualRect(index));
                    break;
                }
                case AiChatMessageDelegate::MessageAction::Dislike: {
                    const AiChatMessageDelegate::MessageFeedback current =
                            m_delegate->messageFeedback(messageId);
                    m_delegate->setMessageFeedback(
                            messageId,
                            current == AiChatMessageDelegate::MessageFeedback::Disliked
                                    ? AiChatMessageDelegate::MessageFeedback::None
                                    : AiChatMessageDelegate::MessageFeedback::Disliked);
                    viewport()->update(visualRect(index));
                    break;
                }
                case AiChatMessageDelegate::MessageAction::None:
                    break;
                }
                event->accept();
                return;
            }

            if (m_delegate->isCodeCopyButtonDisabledAt(option, index, event->pos())) {
                event->accept();
                return;
            }

            if (m_delegate->isCodeCopyButtonAt(option, index, event->pos())) {
                const QString codeText = m_delegate->codeBlockTextAt(option, index, event->pos());
                const int blockRow = m_delegate->codeCopyBlockRowAt(option, index, event->pos());
                if (!codeText.isEmpty() && blockRow >= 0) {
                    QApplication::clipboard()->setText(codeText);
                    setCopiedCodeBlock(index, blockRow);
                    GlobalNotification::showSuccess(this, QStringLiteral("复制成功"));
                }
                event->accept();
                return;
            }

            if (m_delegate->isSettingActionButtonAt(option, index, event->pos())) {
                if (m_delegate->toggleSettingActionAt(option, index, event->pos())) {
                    viewport()->update(visualRect(index));
                }
                event->accept();
                return;
            }

            const QString url = m_delegate->urlAt(option, index, event->pos());
            if (!url.isEmpty()) {
                m_delegate->clearBubbleSelection();
                viewport()->update();
                m_pressedUrlIndex = QPersistentModelIndex(index);
                m_pressedUrlPos = event->pos();
                m_pressedUrl = url;
                event->accept();
                return;
            }

            const int cursor = m_delegate->characterIndexAt(option, index, event->pos());
            if (cursor >= 0) {
                m_delegate->clearBubbleSelection();
                m_dragging = true;
                m_dragIndex = QPersistentModelIndex(index);
                m_dragAnchor = cursor;
                m_delegate->setSelection(index, m_dragAnchor, cursor);
                viewport()->update();
                event->accept();
                return;
            }

            if (hitBubble) {
                clearTextSelection();
                event->accept();
                return;
            }
        }

        m_activeBubbleIndex = QPersistentModelIndex();
        clearTextSelection();
    }

    OverlayScrollListView::mousePressEvent(event);
}

void AiChatMessageListView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        m_pressedUrlIndex = QPersistentModelIndex();
        m_pressedUrl.clear();

        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            if (m_delegate->bubbleHitTest(option, index, event->pos())) {
                setFocus(Qt::MouseFocusReason);
                m_activeBubbleIndex = QPersistentModelIndex(index);

                if (m_delegate->urlAt(option, index, event->pos()).isEmpty() &&
                        m_delegate->selectWordAt(option, index, event->pos())) {
                    viewport()->update();
                    event->accept();
                    return;
                }
            }
        }
    }

    OverlayScrollListView::mouseDoubleClickEvent(event);
}

void AiChatMessageListView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && m_dragIndex.isValid()) {
        const int cursor = characterIndexForDrag(m_dragIndex, event->pos());
        if (kAiChatLayoutDebug) {
            qDebug().noquote()
                    << "AICHAT_LAYOUT mouseDrag"
                    << "row=" << m_dragIndex.row()
                    << "id=" << m_dragIndex.data(AiChatMessageListModel::MessageIdRole).toString()
                    << "pos=" << pointDebugString(event->pos())
                    << "visual=" << rectDebugString(visualRect(m_dragIndex))
                    << "anchor=" << m_dragAnchor
                    << "cursor=" << cursor
                    << "scroll=" << verticalScrollBar()->value()
                    << "max=" << verticalScrollBar()->maximum();
        }
        if (cursor >= 0) {
            m_delegate->setSelection(m_dragIndex, m_dragAnchor, cursor);
            viewport()->update();
        }
        event->accept();
        return;
    }

    if (!m_pressedUrl.isEmpty()) {
        if ((event->pos() - m_pressedUrlPos).manhattanLength() >
                QApplication::startDragDistance()) {
            m_pressedUrlIndex = QPersistentModelIndex();
            m_pressedUrl.clear();
        }
        event->accept();
        return;
    }

    bool overUrl = false;
    bool overCodeCopy = false;
    bool overCodeCopyDisabled = false;
    bool overSettingAction = false;
    bool overMessageAction = false;
    bool overText = false;
    QModelIndex hoveredUserCopyIndex;
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        const QStyleOptionViewItem option = viewOptionForIndex(index);
        const AiChatMessageDelegate::MessageAction messageAction =
                m_delegate->messageActionAt(option, index, event->pos());
        const bool overUserCopyAction = messageAction == AiChatMessageDelegate::MessageAction::Copy &&
                index.data(AiChatMessageListModel::IsFromUserRole).toBool();
        if (index.data(AiChatMessageListModel::IsFromUserRole).toBool() &&
                (m_delegate->bubbleHitTest(option, index, event->pos()) || overUserCopyAction)) {
            hoveredUserCopyIndex = index;
        }
        overMessageAction = messageAction != AiChatMessageDelegate::MessageAction::None;
        overCodeCopyDisabled = !overMessageAction &&
                m_delegate->isCodeCopyButtonDisabledAt(option, index, event->pos());
        overCodeCopy = !overMessageAction && !overCodeCopyDisabled &&
                m_delegate->isCodeCopyButtonAt(option, index, event->pos());
        overSettingAction = !overMessageAction && !overCodeCopyDisabled && !overCodeCopy &&
                m_delegate->isSettingActionButtonAt(option, index, event->pos());
        overUrl = !overCodeCopyDisabled && !overCodeCopy && !overSettingAction &&
                !m_delegate->urlAt(option, index, event->pos()).isEmpty();
        overText = overMessageAction ||
                overUrl ||
                overCodeCopy ||
                overCodeCopyDisabled ||
                overSettingAction ||
                m_delegate->characterIndexAt(option, index, event->pos()) >= 0;
        if (kAiChatLayoutDebug && index.data(AiChatMessageListModel::IsFromUserRole).toBool() &&
                (overMessageAction || overUserCopyAction || m_delegate->bubbleHitTest(option, index, event->pos()))) {
            qDebug().noquote()
                    << "AICHAT_LAYOUT mouseMove"
                    << "row=" << index.row()
                    << "id=" << index.data(AiChatMessageListModel::MessageIdRole).toString()
                    << "pos=" << pointDebugString(event->pos())
                    << "visual=" << rectDebugString(visualRect(index))
                    << "option=" << rectDebugString(option.rect)
                    << "action=" << messageActionDebugString(messageAction)
                    << "overText=" << overText
                    << "scroll=" << verticalScrollBar()->value()
                    << "max=" << verticalScrollBar()->maximum();
        }
    }
    updateHoveredUserCopyIndex(hoveredUserCopyIndex);
    viewport()->setCursor(overCodeCopyDisabled ? Qt::ForbiddenCursor
                                  : ((overMessageAction || overCodeCopy || overSettingAction || overUrl)
                                             ? Qt::PointingHandCursor
                                             : (overText ? Qt::IBeamCursor : Qt::ArrowCursor)));

    OverlayScrollListView::mouseMoveEvent(event);
}

void AiChatMessageListView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_pressedUrl.isEmpty()) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid() && m_pressedUrlIndex == index) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            const QString releaseUrl = m_delegate->urlAt(option, index, event->pos());
            if (releaseUrl == m_pressedUrl) {
                openUrl(m_pressedUrl);
            }
        }

        m_pressedUrlIndex = QPersistentModelIndex();
        m_pressedUrl.clear();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        viewport()->update();
        event->accept();
        return;
    }

    OverlayScrollListView::mouseReleaseEvent(event);
}

void AiChatMessageListView::leaveEvent(QEvent* event)
{
    updateHoveredUserCopyIndex(QModelIndex());
    viewport()->unsetCursor();
    OverlayScrollListView::leaveEvent(event);
}

void AiChatMessageListView::resizeEvent(QResizeEvent* event)
{
    const bool shouldKeepBottom = m_stickToBottom;
    OverlayScrollListView::resizeEvent(event);
    m_scrollAnimation->stop();
    doItemsLayout();
    updateGeometries();
    if (shouldKeepBottom) {
        setScrollBarToBottom();
    }
    updateOverlayScrollBar();
}

void AiChatMessageListView::setScrollBarToBottom()
{
    QScrollBar* scrollBar = verticalScrollBar();
    m_scrollAnimation->stop();
    m_programmaticScrollChange = true;
    scrollBar->setValue(scrollBar->maximum());
    m_programmaticScrollChange = false;
    m_lastScrollValue = scrollBar->value();
    m_stickToBottom = true;
    updateOverlayScrollBar();
}

void AiChatMessageListView::onScrollValueChanged(int value)
{
    if (!m_programmaticScrollChange && value < m_lastScrollValue && !isAtBottom()) {
        m_stickToBottom = false;
        if (m_scrollAnimation->state() == QAbstractAnimation::Running) {
            m_scrollAnimation->stop();
        }
    }

    m_lastScrollValue = value;
    if (isAtBottom()) {
        m_stickToBottom = true;
    }
}

void AiChatMessageListView::unlockBottomLockForUserScrollUp()
{
    if (verticalScrollBar()->value() > verticalScrollBar()->minimum()) {
        m_stickToBottom = false;
        if (m_scrollAnimation->state() == QAbstractAnimation::Running) {
            m_scrollAnimation->stop();
        }
    }
}

bool AiChatMessageListView::isAtBottom() const
{
    const QScrollBar* scrollBar = verticalScrollBar();
    return scrollBar->maximum() - scrollBar->value() <= kBottomReachedThreshold;
}

bool AiChatMessageListView::hasUpwardScrollIntent(const QWheelEvent* event) const
{
    if (!event->pixelDelta().isNull()) {
        return event->pixelDelta().y() > 0;
    }

    return event->angleDelta().y() > 0;
}

bool AiChatMessageListView::hasDownwardScrollIntent(const QWheelEvent* event) const
{
    if (!event->pixelDelta().isNull()) {
        return event->pixelDelta().y() < 0;
    }

    return event->angleDelta().y() < 0;
}

void AiChatMessageListView::updateCodeBlockLoadingAnimation()
{
    if (!model() || m_delegate->streamingMessageId().isEmpty()) {
        syncCodeBlockLoadingAnimationTimer();
        return;
    }

    const QString streamingMessageId = m_delegate->streamingMessageId();
    if (!m_codeBlockLoadingIndex.isValid() ||
            m_codeBlockLoadingIndex.data(AiChatMessageListModel::MessageIdRole).toString() != streamingMessageId) {
        m_codeBlockLoadingIndex = QPersistentModelIndex();
        const int rows = model()->rowCount();
        for (int row = 0; row < rows; ++row) {
            const QModelIndex index = model()->index(row, 0);
            if (index.isValid() &&
                    index.data(AiChatMessageListModel::MessageIdRole).toString() == streamingMessageId) {
                m_codeBlockLoadingIndex = QPersistentModelIndex(index);
                break;
            }
        }
    }

    if (!m_codeBlockLoadingIndex.isValid()) {
        return;
    }

    const QStyleOptionViewItem option = viewOptionForIndex(m_codeBlockLoadingIndex);
    const QRect rect = m_delegate->streamingCodeBlockUpdateRect(option, m_codeBlockLoadingIndex);
    if (rect.isValid() && viewport()->rect().intersects(rect)) {
        viewport()->update(rect);
    }
}

void AiChatMessageListView::syncCodeBlockLoadingAnimationTimer()
{
    if (!m_codeBlockLoadingTimer) {
        return;
    }

    if (m_delegate->streamingMessageId().isEmpty()) {
        m_codeBlockLoadingTimer->stop();
        m_codeBlockLoadingIndex = QPersistentModelIndex();
        viewport()->update();
        return;
    }

    m_codeBlockLoadingIndex = QPersistentModelIndex();
    if (!m_codeBlockLoadingTimer->isActive()) {
        m_codeBlockLoadingTimer->start();
    }
}

QStyleOptionViewItem AiChatMessageListView::viewOptionForIndex(const QModelIndex& index) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Enabled;
    option.widget = viewport();
    option.rect = visualRect(index);
    option.rect.setWidth(qMax(1, option.rect.width() - kViewPaintWidthInset));
    return option;
}

int AiChatMessageListView::characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const
{
    if (!index.isValid()) {
        return -1;
    }

    const QStyleOptionViewItem option = viewOptionForIndex(index);
    const int cursor = m_delegate->characterIndexAt(option, index, pos, true);
    if (kAiChatLayoutDebug) {
        qDebug().noquote()
                << "AICHAT_LAYOUT dragChar"
                << "row=" << index.row()
                << "id=" << index.data(AiChatMessageListModel::MessageIdRole).toString()
                << "pos=" << pointDebugString(pos)
                << "visual=" << rectDebugString(visualRect(index))
                << "option=" << rectDebugString(option.rect)
                << "cursor=" << cursor;
    }
    if (cursor >= 0) {
        return cursor;
    }

    const QRect itemRect = visualRect(index);
    const QString text = m_delegate->renderedText(index);
    if (pos.y() <= itemRect.top()) {
        return 0;
    }
    if (pos.y() >= itemRect.bottom()) {
        return text.size();
    }
    return -1;
}

void AiChatMessageListView::copySelectionToClipboard()
{
    const QString selectedText = m_delegate->selectedText();
    if (selectedText.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(selectedText);
}

void AiChatMessageListView::copyBubbleToClipboard(const QModelIndex& index)
{
    const QString text = m_delegate->renderedText(index);
    if (text.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(text);
}

void AiChatMessageListView::setCopiedCodeBlock(const QModelIndex& index, int blockRow)
{
    if (m_copiedCodeIndex.isValid()) {
        viewport()->update(visualRect(m_copiedCodeIndex));
    }

    m_copiedCodeIndex = QPersistentModelIndex(index);
    m_delegate->setCopiedCodeBlock(index, blockRow);
    viewport()->update(visualRect(index));
    m_copyResetTimer->start(3000);
}

void AiChatMessageListView::clearCopiedCodeBlock()
{
    if (m_copyResetTimer) {
        m_copyResetTimer->stop();
    }

    const QPersistentModelIndex previousIndex = m_copiedCodeIndex;
    m_copiedCodeIndex = QPersistentModelIndex();
    m_delegate->clearCopiedCodeBlock();
    if (previousIndex.isValid()) {
        viewport()->update(visualRect(previousIndex));
    }
}

void AiChatMessageListView::selectAllTextInActiveBubble()
{
    if (!m_activeBubbleIndex.isValid()) {
        return;
    }

    const QString text = m_delegate->renderedText(m_activeBubbleIndex);
    if (text.isEmpty()) {
        return;
    }

    m_delegate->setSelection(m_activeBubbleIndex, 0, text.size());
    viewport()->update();
}

void AiChatMessageListView::showSelectionMenu(const QPoint& globalPos)
{
    auto* menu = new StyledActionMenu(this);

    QAction* copyAction = menu->addAction(QStringLiteral("复制"));
    connect(copyAction, &QAction::triggered, this, [this]() {
        copySelectionToClipboard();
    });

    connect(menu, &StyledActionMenu::aboutToHide, menu, [menu]() {
        menu->deleteLater();
    });

    menu->popupWhenMouseReleased(globalPos);
}

void AiChatMessageListView::showBubbleMenu(const QPoint& globalPos, const QModelIndex& index)
{
    auto* menu = new StyledActionMenu(this);
    const QPersistentModelIndex persistentIndex(index);

    QAction* copyAction = menu->addAction(QStringLiteral("复制"));
    connect(copyAction, &QAction::triggered, this, [this, persistentIndex]() {
        if (persistentIndex.isValid()) {
            copyBubbleToClipboard(persistentIndex);
        }
    });

    connect(menu, &StyledActionMenu::aboutToHide, menu, [this, menu]() {
        m_delegate->clearBubbleSelection();
        viewport()->update();
        menu->deleteLater();
    });

    menu->popupWhenMouseReleased(globalPos);
}

void AiChatMessageListView::showUrlMenu(const QPoint& globalPos, const QString& url)
{
    auto* menu = new StyledActionMenu(this);

    QAction* copyAction = menu->addAction(QStringLiteral("复制"));
    connect(copyAction, &QAction::triggered, this, [url]() {
        QApplication::clipboard()->setText(url);
    });

    QAction* openAction = menu->addAction(QStringLiteral("用默认浏览器打开"));
    connect(openAction, &QAction::triggered, this, [this, url]() {
        openUrl(url);
    });

    connect(menu, &StyledActionMenu::aboutToHide, menu, [menu]() {
        menu->deleteLater();
    });

    menu->popupWhenMouseReleased(globalPos);
}

void AiChatMessageListView::openUrl(const QString& url)
{
    const QUrl resolvedUrl = QUrl::fromUserInput(url);
    if (!resolvedUrl.isValid()) {
        return;
    }

    QDesktopServices::openUrl(resolvedUrl);
}

void AiChatMessageListView::updateHoveredUserCopyIndex(const QModelIndex& index)
{
    if (m_hoveredUserCopyIndex == index) {
        return;
    }

    const QPersistentModelIndex previous = m_hoveredUserCopyIndex;
    m_hoveredUserCopyIndex = QPersistentModelIndex(index);

    if (previous.isValid()) {
        animateUserCopyButton(previous, 0.0);
    }
    if (m_hoveredUserCopyIndex.isValid()) {
        animateUserCopyButton(m_hoveredUserCopyIndex, 1.0);
    }
}

void AiChatMessageListView::animateUserCopyButton(const QPersistentModelIndex& index, qreal targetOpacity)
{
    if (!index.isValid()) {
        return;
    }

    const QString messageId = index.data(AiChatMessageListModel::MessageIdRole).toString();
    if (messageId.isEmpty()) {
        return;
    }

    if (QPointer<QVariantAnimation> running = m_userCopyFadeAnimations.value(messageId)) {
        running->stop();
        running->deleteLater();
    }

    auto* animation = new QVariantAnimation(this);
    m_userCopyFadeAnimations.insert(messageId, animation);
    animation->setStartValue(m_delegate->userCopyButtonOpacity(messageId));
    animation->setEndValue(qBound(0.0, targetOpacity, 1.0));
    animation->setDuration(kUserCopyFadeDurationMs);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this, index, messageId](const QVariant& value) {
        m_delegate->setUserCopyButtonOpacity(messageId, value.toReal());
        if (index.isValid()) {
            viewport()->update(visualRect(index));
        } else {
            viewport()->update();
        }
    });
    connect(animation, &QVariantAnimation::finished, this, [this, messageId, targetOpacity, animation]() {
        m_delegate->setUserCopyButtonOpacity(messageId, targetOpacity);
        m_userCopyFadeAnimations.remove(messageId);
        animation->deleteLater();
    });

    animation->start();
}

void AiChatMessageListView::toggleUserMessageExpansion(const QModelIndex& index)
{
    if (!index.isValid() || !index.data(AiChatMessageListModel::IsFromUserRole).toBool()) {
        return;
    }

    const QString messageId = index.data(AiChatMessageListModel::MessageIdRole).toString();
    if (messageId.isEmpty()) {
        return;
    }

    if (QPointer<QVariantAnimation> running = m_userExpansionAnimations.value(messageId)) {
        running->stop();
        running->deleteLater();
    }

    const qreal startProgress = m_delegate->userMessageExpansionProgress(messageId);
    const bool collapse = m_delegate->isUserMessageExpanded(messageId) && startProgress > 0.001;
    const qreal endProgress = collapse ? 0.0 : 1.0;
    if (kAiChatLayoutDebug) {
        qDebug().noquote()
                << "AICHAT_LAYOUT toggleExpansion"
                << "row=" << index.row()
                << "id=" << messageId
                << "collapse=" << collapse
                << "startProgress=" << startProgress
                << "endProgress=" << endProgress
                << "visualBefore=" << rectDebugString(visualRect(index))
                << "scroll=" << verticalScrollBar()->value()
                << "max=" << verticalScrollBar()->maximum()
                << "stickToBottom=" << m_stickToBottom;
    }
    m_delegate->setUserMessageExpanded(messageId, true);
    m_delegate->setUserMessageExpansionProgress(messageId, startProgress);

    auto* animation = new QVariantAnimation(this);
    m_userExpansionAnimations.insert(messageId, animation);
    animation->setStartValue(startProgress);
    animation->setEndValue(endProgress);
    animation->setDuration(kUserMessageExpandDurationMs);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    const QPersistentModelIndex animatedIndex(index);
    connect(animation, &QVariantAnimation::valueChanged, this, [this, animatedIndex, messageId](const QVariant& value) {
        m_delegate->setUserMessageExpansionProgress(messageId, value.toReal());
        refreshAnimatedMessageLayout(animatedIndex);
    });
    connect(animation, &QVariantAnimation::finished, this, [this, animatedIndex, messageId, collapse, animation]() {
        if (collapse) {
            m_delegate->setUserMessageExpanded(messageId, false);
        } else {
            m_delegate->setUserMessageExpansionProgress(messageId, 1.0);
        }
        m_userExpansionAnimations.remove(messageId);
        animation->deleteLater();
        refreshAnimatedMessageLayout(animatedIndex);
    });

    animation->start();
}

void AiChatMessageListView::refreshAnimatedMessageLayout(const QModelIndex& index)
{
    const int previousValue = verticalScrollBar()->value();
    const int previousMaximum = verticalScrollBar()->maximum();
    const QRect previousVisualRect = index.isValid() ? visualRect(index) : QRect();
    if (kAiChatLayoutDebug) {
        qDebug().noquote()
                << "AICHAT_LAYOUT refreshAnimated before"
                << "row=" << index.row()
                << "id=" << index.data(AiChatMessageListModel::MessageIdRole).toString()
                << "visual=" << rectDebugString(previousVisualRect)
                << "scroll=" << previousValue
                << "max=" << previousMaximum
                << "stickToBottom=" << m_stickToBottom;
    }
    m_delegate->notifySizeHintChanged(index);
    doItemsLayout();
    updateGeometries();
    m_programmaticScrollChange = true;
    verticalScrollBar()->setValue(qMin(previousValue, verticalScrollBar()->maximum()));
    m_programmaticScrollChange = false;
    m_lastScrollValue = verticalScrollBar()->value();
    m_stickToBottom = isAtBottom();
    if (kAiChatLayoutDebug) {
        qDebug().noquote()
                << "AICHAT_LAYOUT refreshAnimated after"
                << "row=" << index.row()
                << "id=" << index.data(AiChatMessageListModel::MessageIdRole).toString()
                << "visual=" << rectDebugString(index.isValid() ? visualRect(index) : QRect())
                << "scroll=" << verticalScrollBar()->value()
                << "max=" << verticalScrollBar()->maximum()
                << "previousScroll=" << previousValue
                << "previousMax=" << previousMaximum
                << "stickToBottom=" << m_stickToBottom;
    }
    updateOverlayScrollBar();
    viewport()->update();
}
