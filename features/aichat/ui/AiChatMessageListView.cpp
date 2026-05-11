#include "AiChatMessageListView.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QUrl>

#include "AiChatMessageDelegate.h"
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

} // namespace

AiChatMessageListView::AiChatMessageListView(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_delegate(new AiChatMessageDelegate(this))
{
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformItemSizes(false);
    setSpacing(2);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setThemeBackgroundRole(ThemeColor::PageBackground);
    setWheelStepPixels(72);
    setScrollBarInsets(8, 4);
}

AiChatMessageDelegate* AiChatMessageListView::messageDelegate() const
{
    return m_delegate;
}

void AiChatMessageListView::scrollToBottom()
{
    QTimer::singleShot(0, this, [this]() {
        doItemsLayout();
        updateGeometries();
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        updateOverlayScrollBar();
    });
}

void AiChatMessageListView::clearTextSelection()
{
    if (!m_delegate->hasSelection() && !m_delegate->selectionIndex().isValid()) {
        return;
    }

    m_delegate->clearSelection();
    viewport()->update();
}

void AiChatMessageListView::setBottomViewportMargin(int margin)
{
    setViewportMargins(0, 0, 0, qMax(0, margin));
    updateOverlayScrollBar();
}

void AiChatMessageListView::keyPressEvent(QKeyEvent* event)
{
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

void AiChatMessageListView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            const QString url = m_delegate->urlAt(option, index, event->pos());
            if (!url.isEmpty()) {
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
        }
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

            const QString url = m_delegate->urlAt(option, index, event->pos());
            if (!url.isEmpty()) {
                m_pressedUrlIndex = QPersistentModelIndex(index);
                m_pressedUrlPos = event->pos();
                m_pressedUrl = url;
                event->accept();
                return;
            }

            const int cursor = m_delegate->characterIndexAt(option, index, event->pos());
            if (cursor >= 0) {
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
    bool overText = false;
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        const QStyleOptionViewItem option = viewOptionForIndex(index);
        overUrl = !m_delegate->urlAt(option, index, event->pos()).isEmpty();
        overText = overUrl ||
                m_delegate->characterIndexAt(option, index, event->pos()) >= 0;
    }
    viewport()->setCursor(overUrl ? Qt::PointingHandCursor
                                  : (overText ? Qt::IBeamCursor : Qt::ArrowCursor));

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
    viewport()->unsetCursor();
    OverlayScrollListView::leaveEvent(event);
}

void AiChatMessageListView::resizeEvent(QResizeEvent* event)
{
    const bool wasNearBottom = verticalScrollBar()->maximum() - verticalScrollBar()->value() <= 5;
    OverlayScrollListView::resizeEvent(event);
    doItemsLayout();
    updateGeometries();
    if (wasNearBottom) {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
    updateOverlayScrollBar();
}

QStyleOptionViewItem AiChatMessageListView::viewOptionForIndex(const QModelIndex& index) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Enabled;
    option.widget = viewport();
    option.rect = visualRect(index);
    return option;
}

int AiChatMessageListView::characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const
{
    if (!index.isValid()) {
        return -1;
    }

    const QStyleOptionViewItem option = viewOptionForIndex(index);
    const int cursor = m_delegate->characterIndexAt(option, index, pos, true);
    if (cursor >= 0) {
        return cursor;
    }

    const QRect itemRect = visualRect(index);
    const QString text = index.data(Qt::DisplayRole).toString();
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

void AiChatMessageListView::selectAllTextInActiveBubble()
{
    if (!m_activeBubbleIndex.isValid()) {
        return;
    }

    const QString text = m_activeBubbleIndex.data(Qt::DisplayRole).toString();
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

    menu->addAction(QStringLiteral("搜索引用"));

    connect(menu, &QMenu::aboutToHide, menu, [menu]() {
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

    connect(menu, &QMenu::aboutToHide, menu, [menu]() {
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
