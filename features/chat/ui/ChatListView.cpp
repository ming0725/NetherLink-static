#include "ChatListView.h"

#include <algorithm>
#include <cmath>

#include <QAction>
#include <QAbstractItemModel>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QUrl>

#include "features/chat/ui/ChatItemDelegate.h"
#include "shared/ui/StyledActionMenu.h"

namespace {

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

ChatListView::ChatListView(QWidget *parent)
    : OverlayScrollListView(parent)
    , m_scrollAnimation(new QPropertyAnimation(verticalScrollBar(), "value", this))
{
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setThemeBackgroundRole(ThemeColor::PageBackground);
    setScrollBarInsets(8, 4);

    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void ChatListView::setModel(QAbstractItemModel *model)
{
    if (this->model()) {
        disconnect(this->model(), &QAbstractItemModel::rowsInserted,
                   this, &ChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::rowsRemoved,
                   this, &ChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::modelReset,
                   this, &ChatListView::onModelRowsChanged);
    }

    OverlayScrollListView::setModel(model);

    if (!model) {
        return;
    }

    connect(model, &QAbstractItemModel::rowsInserted,
            this, &ChatListView::onModelRowsChanged);
    connect(model, &QAbstractItemModel::rowsRemoved,
            this, &ChatListView::onModelRowsChanged);
    connect(model, &QAbstractItemModel::modelReset,
            this, &ChatListView::onModelRowsChanged);

    QTimer::singleShot(0, this, [this]() { updateOverlayScrollBar(); });
}

void ChatListView::scrollToBottom(bool accelerateFarDistance)
{
    doItemsLayout();
    updateGeometries();

    QScrollBar* scrollBar = verticalScrollBar();
    const int targetValue = scrollBar->maximum();
    const int startValue = scrollBar->value();

    if (targetValue <= scrollBar->minimum() || startValue >= targetValue) {
        jumpToBottom();
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

void ChatListView::jumpToBottom()
{
    m_scrollAnimation->stop();
    doItemsLayout();
    updateGeometries();
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    updateOverlayScrollBar();
}

void ChatListView::preserveScrollPositionAfterPrepend(int previousValue, int previousMaximum)
{
    m_scrollAnimation->stop();
    QTimer::singleShot(0, this, [this, previousValue, previousMaximum]() {
        doItemsLayout();
        updateGeometries();
        QScrollBar* scrollBar = verticalScrollBar();
        const int addedHeight = qMax(0, scrollBar->maximum() - previousMaximum);
        scrollBar->setValue(qBound(scrollBar->minimum(),
                                   previousValue + addedHeight,
                                   scrollBar->maximum()));
        updateOverlayScrollBar();
    });
}

void ChatListView::clearTextSelection()
{
    ChatItemDelegate* delegate = chatDelegate();
    if (!delegate || (!delegate->hasSelection() && !delegate->selectionIndex().isValid())) {
        return;
    }

    delegate->clearSelection();
    viewport()->update();
}

void ChatListView::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::SelectAll)) {
        selectAllTextInActiveBubble();
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Copy)) {
        if (ChatItemDelegate* delegate = chatDelegate();
                delegate && delegate->hasSelection()) {
            copySelectionToClipboard();
            event->accept();
            return;
        }
    }

    OverlayScrollListView::keyPressEvent(event);
}

void ChatListView::resizeEvent(QResizeEvent* event)
{
    const bool wasNearBottom = verticalScrollBar()->maximum() - verticalScrollBar()->value() <= 5;

    OverlayScrollListView::resizeEvent(event);
    m_scrollAnimation->stop();
    doItemsLayout();
    updateGeometries();
    if (wasNearBottom) {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
    updateOverlayScrollBar();
}

void ChatListView::mousePressEvent(QMouseEvent* event)
{
    ChatItemDelegate* delegate = chatDelegate();

    if (delegate && event->button() == Qt::RightButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            const QString url = delegate->urlAt(option, index, event->pos());
            if (!url.isEmpty()) {
                showUrlMenu(mouseGlobalPosition(event), url);
                event->accept();
                return;
            }

            const int cursor = delegate->characterIndexAt(option, index, event->pos());
            if (delegate->selectionContains(index, cursor)) {
                showSelectionMenu(mouseGlobalPosition(event));
                event->accept();
                return;
            }
        }

        clearTextSelection();
    }

    if (delegate && event->button() == Qt::LeftButton) {
        m_pressedUrlIndex = QPersistentModelIndex();
        m_pressedUrl.clear();

        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            const bool hitBubble = delegate->bubbleHitTest(option, index, event->pos());
            if (hitBubble) {
                setFocus(Qt::MouseFocusReason);
                m_activeBubbleIndex = QPersistentModelIndex(index);
            }

            const QString url = delegate->urlAt(option, index, event->pos());
            if (!url.isEmpty()) {
                if (model()) {
                    static_cast<ChatListModel*>(model())->clearSelection();
                }
                m_pressedUrlIndex = QPersistentModelIndex(index);
                m_pressedUrlPos = event->pos();
                m_pressedUrl = url;
                event->accept();
                return;
            }

            const int cursor = delegate->characterIndexAt(option, index, event->pos());
            if (cursor >= 0) {
                if (model()) {
                    static_cast<ChatListModel*>(model())->clearSelection();
                }
                m_dragging = true;
                m_dragIndex = QPersistentModelIndex(index);
                m_dragAnchor = cursor;
                delegate->setSelection(index, m_dragAnchor, cursor);
                viewport()->update();
                event->accept();
                return;
            }

            if (hitBubble) {
                clearTextSelection();
                if (model()) {
                    model()->setData(index, true, Qt::UserRole + 1);
                }
                event->accept();
                return;
            }
        }

        m_activeBubbleIndex = QPersistentModelIndex();
        clearTextSelection();
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() && model()) {
        static_cast<ChatListModel*>(model())->clearSelection();
    }
    OverlayScrollListView::mousePressEvent(event);
}

void ChatListView::mouseDoubleClickEvent(QMouseEvent* event)
{
    ChatItemDelegate* delegate = chatDelegate();
    if (delegate && event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        m_pressedUrlIndex = QPersistentModelIndex();
        m_pressedUrl.clear();

        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            if (delegate->bubbleHitTest(option, index, event->pos())) {
                setFocus(Qt::MouseFocusReason);
                m_activeBubbleIndex = QPersistentModelIndex(index);

                if (delegate->urlAt(option, index, event->pos()).isEmpty() &&
                        delegate->selectWordAt(option, index, event->pos())) {
                    if (model()) {
                        static_cast<ChatListModel*>(model())->clearSelection();
                    }
                    viewport()->update();
                    event->accept();
                    return;
                }
            }
        }
    }

    OverlayScrollListView::mouseDoubleClickEvent(event);
}

void ChatListView::mouseMoveEvent(QMouseEvent* event)
{
    ChatItemDelegate* delegate = chatDelegate();
    if (delegate && m_dragging && m_dragIndex.isValid()) {
        const int cursor = characterIndexForDrag(m_dragIndex, event->pos());
        if (cursor >= 0) {
            delegate->setSelection(m_dragIndex, m_dragAnchor, cursor);
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
    bool overText = false;
    if (delegate) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            overUrl = !delegate->urlAt(option, index, event->pos()).isEmpty();
            overText = overUrl || delegate->characterIndexAt(option, index, event->pos()) >= 0;
        }
    }
    viewport()->setCursor(overUrl ? Qt::PointingHandCursor
                                  : (overText ? Qt::IBeamCursor : Qt::ArrowCursor));

    OverlayScrollListView::mouseMoveEvent(event);
}

void ChatListView::mouseReleaseEvent(QMouseEvent* event)
{
    ChatItemDelegate* delegate = chatDelegate();
    if (delegate && event->button() == Qt::LeftButton && !m_pressedUrl.isEmpty()) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid() && m_pressedUrlIndex == index) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            const QString releaseUrl = delegate->urlAt(option, index, event->pos());
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

void ChatListView::leaveEvent(QEvent* event)
{
    viewport()->unsetCursor();
    OverlayScrollListView::leaveEvent(event);
}

void ChatListView::onModelRowsChanged()
{
    QTimer::singleShot(0, this, [this]() {
        doItemsLayout();
        updateGeometries();
        updateOverlayScrollBar();
    });
}

ChatItemDelegate* ChatListView::chatDelegate() const
{
    return qobject_cast<ChatItemDelegate*>(itemDelegate());
}

QStyleOptionViewItem ChatListView::viewOptionForIndex(const QModelIndex& index) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Enabled;
    option.widget = viewport();
    option.rect = visualRect(index);
    return option;
}

int ChatListView::characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const
{
    ChatItemDelegate* delegate = chatDelegate();
    if (!delegate || !index.isValid()) {
        return -1;
    }

    const QStyleOptionViewItem option = viewOptionForIndex(index);
    const int cursor = delegate->characterIndexAt(option, index, pos, true);
    if (cursor >= 0) {
        return cursor;
    }

    const QRect itemRect = visualRect(index);
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Text) {
        return -1;
    }

    if (pos.y() <= itemRect.top()) {
        return 0;
    }
    if (pos.y() >= itemRect.bottom()) {
        return message->getContent().size();
    }
    return -1;
}

void ChatListView::copySelectionToClipboard()
{
    ChatItemDelegate* delegate = chatDelegate();
    if (!delegate) {
        return;
    }

    const QString selectedText = delegate->selectedText();
    if (selectedText.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(selectedText);
}

void ChatListView::selectAllTextInActiveBubble()
{
    ChatItemDelegate* delegate = chatDelegate();
    if (!delegate || !m_activeBubbleIndex.isValid()) {
        return;
    }

    const ChatMessage* message = m_activeBubbleIndex.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Text || message->getContent().isEmpty()) {
        return;
    }

    if (model()) {
        static_cast<ChatListModel*>(model())->clearSelection();
    }
    delegate->setSelection(m_activeBubbleIndex, 0, message->getContent().size());
    viewport()->update();
}

void ChatListView::showSelectionMenu(const QPoint& globalPos)
{
    auto* menu = new StyledActionMenu(this);

    QAction* copyAction = menu->addAction(QStringLiteral("复制"));
    connect(copyAction, &QAction::triggered, this, [this]() {
        copySelectionToClipboard();
    });

    connect(menu, &QMenu::aboutToHide, menu, [menu]() {
        menu->deleteLater();
    });

    menu->popupWhenMouseReleased(globalPos);
}

void ChatListView::showUrlMenu(const QPoint& globalPos, const QString& url)
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

    connect(menu, &QMenu::aboutToHide, menu, [menu]() {
        menu->deleteLater();
    });

    menu->popupWhenMouseReleased(globalPos);
}

void ChatListView::openUrl(const QString& url)
{
    const QUrl resolvedUrl = QUrl::fromUserInput(url);
    if (!resolvedUrl.isValid()) {
        return;
    }

    QDesktopServices::openUrl(resolvedUrl);
}
