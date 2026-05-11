#include "PostDetailListView.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>

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

PostDetailListView::PostDetailListView(QWidget* parent)
    : OverlayScrollListView(parent)
{
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setContextMenuPolicy(Qt::NoContextMenu);
    viewport()->setContextMenuPolicy(Qt::NoContextMenu);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
}

void PostDetailListView::setPostCommentDelegate(PostCommentDelegate* delegate)
{
    m_commentDelegate = delegate;
    setItemDelegate(delegate);
}

void PostDetailListView::setBackgroundRole(ThemeColor role)
{
    setThemeBackgroundRole(role);
}

void PostDetailListView::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::SelectAll)) {
        selectAllTextInActiveTarget();
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

void PostDetailListView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_commentDelegate && m_dragging && m_dragIndex.isValid()) {
        const int cursor = characterIndexForDrag(m_dragIndex, event->pos());
        if (cursor >= 0) {
            m_commentDelegate->setSelection(m_dragIndex,
                                            m_dragTarget,
                                            m_dragCommentId,
                                            m_dragReplyId,
                                            m_dragAnchor,
                                            cursor);
            viewport()->update();
        }
        event->accept();
        return;
    }

    bool hasAction = false;
    bool hasAuthor = false;
    bool hasText = false;
    if (m_commentDelegate && model()) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            hasAction = m_commentDelegate->actionAt(option, index, event->pos()).action
                    != PostCommentDelegate::NoAction;
            hasAuthor = m_commentDelegate->authorHitAt(option, index, event->pos());
            hasText = m_commentDelegate->textHitAt(option, index, event->pos()).isValid();
        }
    }

    if (hasAction || hasAuthor) {
        viewport()->setCursor(Qt::PointingHandCursor);
    } else if (hasText) {
        viewport()->setCursor(Qt::IBeamCursor);
    } else {
        viewport()->unsetCursor();
    }
    OverlayScrollListView::mouseMoveEvent(event);
}

void PostDetailListView::mousePressEvent(QMouseEvent* event)
{
    if (!m_commentDelegate || !model()) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        m_activeTextIndex = QPersistentModelIndex();
        m_activeTextTarget = PostCommentDelegate::NoTextTarget;
        m_commentDelegate->clearSelection();
        viewport()->update();
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const QStyleOptionViewItem option = viewOptionForIndex(index);

    if (event->button() == Qt::RightButton) {
        const PostCommentDelegate::TextHit textHit =
                m_commentDelegate->textHitAt(option, index, event->pos());
        if (textHit.isValid() &&
                m_commentDelegate->selectionContains(index,
                                                     textHit.target,
                                                     textHit.commentId,
                                                     textHit.replyId,
                                                     textHit.cursor)) {
            showSelectionMenu(mouseGlobalPosition(event));
            event->accept();
            return;
        }

        const PostCommentDelegate::InteractionTarget target =
                m_commentDelegate->interactionTargetAt(option, index, event->pos());
        if (target.isValid()) {
            showTargetMenu(mouseGlobalPosition(event), target);
            event->accept();
            return;
        }

        m_commentDelegate->clearSelection();
        viewport()->update();
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const PostCommentDelegate::HitAction hit = m_commentDelegate->actionAt(option, index, event->pos());
    switch (hit.action) {
    case PostCommentDelegate::ToggleCommentLikeAction:
        emit commentLikeRequested(hit.commentId, hit.nextLiked);
        event->accept();
        return;
    case PostCommentDelegate::ToggleReplyLikeAction:
        emit replyLikeRequested(hit.commentId, hit.replyId, hit.nextLiked);
        event->accept();
        return;
    case PostCommentDelegate::ReplyToCommentAction:
        emit replyToCommentRequested(hit.commentId);
        event->accept();
        return;
    case PostCommentDelegate::ReplyToReplyAction:
        emit replyToReplyRequested(hit.commentId, hit.replyId);
        event->accept();
        return;
    case PostCommentDelegate::ExpandCommentAction:
        emit commentExpandRequested(hit.commentId);
        event->accept();
        return;
    case PostCommentDelegate::ExpandReplyAction:
        emit replyExpandRequested(hit.commentId, hit.replyId);
        event->accept();
        return;
    case PostCommentDelegate::LoadMoreRepliesAction:
        emit moreRepliesRequested(hit.commentId);
        event->accept();
        return;
    default:
        break;
    }

    const PostCommentDelegate::TextHit textHit =
            m_commentDelegate->textHitAt(option, index, event->pos());
    if (textHit.isValid()) {
        setFocus(Qt::MouseFocusReason);
        m_activeTextIndex = QPersistentModelIndex(index);
        m_activeTextTarget = textHit.target;
        m_activeCommentId = textHit.commentId;
        m_activeReplyId = textHit.replyId;
        m_dragging = true;
        m_dragIndex = QPersistentModelIndex(index);
        m_dragTarget = textHit.target;
        m_dragCommentId = textHit.commentId;
        m_dragReplyId = textHit.replyId;
        m_dragAnchor = textHit.cursor;
        m_commentDelegate->setSelection(index,
                                        textHit.target,
                                        textHit.commentId,
                                        textHit.replyId,
                                        m_dragAnchor,
                                        textHit.cursor);
        viewport()->update();
        event->accept();
        return;
    }

    m_activeTextIndex = QPersistentModelIndex();
    m_activeTextTarget = PostCommentDelegate::NoTextTarget;
    m_commentDelegate->clearSelection();
    viewport()->update();
    OverlayScrollListView::mousePressEvent(event);
}

void PostDetailListView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_commentDelegate && event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragTarget = PostCommentDelegate::NoTextTarget;
        m_dragAnchor = -1;

        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            const QStyleOptionViewItem option = viewOptionForIndex(index);
            const PostCommentDelegate::TextHit hit =
                    m_commentDelegate->textHitAt(option, index, event->pos());
            if (hit.isValid() && m_commentDelegate->selectWordAt(option, index, event->pos())) {
                setFocus(Qt::MouseFocusReason);
                m_activeTextIndex = QPersistentModelIndex(index);
                m_activeTextTarget = hit.target;
                m_activeCommentId = hit.commentId;
                m_activeReplyId = hit.replyId;
                viewport()->update();
                event->accept();
                return;
            }
        }
    }

    OverlayScrollListView::mouseDoubleClickEvent(event);
}

void PostDetailListView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragTarget = PostCommentDelegate::NoTextTarget;
        m_dragCommentId.clear();
        m_dragReplyId.clear();
        m_dragAnchor = -1;
        viewport()->update();
        event->accept();
        return;
    }

    OverlayScrollListView::mouseReleaseEvent(event);
}

void PostDetailListView::leaveEvent(QEvent* event)
{
    viewport()->unsetCursor();
    OverlayScrollListView::leaveEvent(event);
}

QStyleOptionViewItem PostDetailListView::viewOptionForIndex(const QModelIndex& index) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Enabled;
    option.widget = viewport();
    option.rect = visualRect(index);
    return option;
}

int PostDetailListView::characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const
{
    if (!m_commentDelegate || !index.isValid()) {
        return -1;
    }

    const QStyleOptionViewItem option = viewOptionForIndex(index);
    const PostCommentDelegate::TextHit hit =
            m_commentDelegate->textHitAt(option, index, pos, true);
    if (hit.isValid() &&
            hit.target == m_dragTarget &&
            hit.commentId == m_dragCommentId &&
            hit.replyId == m_dragReplyId) {
        return hit.cursor;
    }

    const QRect itemRect = visualRect(index);
    if (pos.y() <= itemRect.top()) {
        return 0;
    }

    if (pos.y() >= itemRect.bottom()) {
        return m_commentDelegate->textLengthForTarget(index,
                                                      m_dragTarget,
                                                      m_dragCommentId,
                                                      m_dragReplyId);
    }
    return -1;
}

void PostDetailListView::copySelectionToClipboard()
{
    if (!m_commentDelegate) {
        return;
    }

    const QString selectedText = m_commentDelegate->selectedText();
    if (selectedText.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(selectedText);
}

void PostDetailListView::selectAllTextInActiveTarget()
{
    if (!m_commentDelegate || !m_activeTextIndex.isValid() ||
            m_activeTextTarget == PostCommentDelegate::NoTextTarget) {
        return;
    }

    m_commentDelegate->selectAll(m_activeTextIndex,
                                 m_activeTextTarget,
                                 m_activeCommentId,
                                 m_activeReplyId);
    viewport()->update();
}

void PostDetailListView::showSelectionMenu(const QPoint& globalPos)
{
    auto* menu = new StyledActionMenu(this);

    QAction* copyAction = menu->addAction(QStringLiteral("复制"));
    connect(copyAction, &QAction::triggered, this, [this]() {
        copySelectionToClipboard();
    });

    const QString commentId = m_commentDelegate ? m_commentDelegate->selectionCommentId() : QString();
    const QString replyId = m_commentDelegate ? m_commentDelegate->selectionReplyId() : QString();
    if (!commentId.isEmpty()) {
        QAction* replyAction = menu->addAction(QStringLiteral("回复"));
        connect(replyAction, &QAction::triggered, this, [this, commentId, replyId]() {
            if (replyId.isEmpty()) {
                emit replyToCommentRequested(commentId);
            } else {
                emit replyToReplyRequested(commentId, replyId);
            }
        });
    }

    connect(menu, &QMenu::aboutToHide, menu, [menu]() {
        menu->deleteLater();
    });

    menu->popupWhenMouseReleased(globalPos);
}

void PostDetailListView::showTargetMenu(const QPoint& globalPos,
                                        const PostCommentDelegate::InteractionTarget& target)
{
    auto* menu = new StyledActionMenu(this);

    QAction* copyAction = menu->addAction(QStringLiteral("复制"));
    connect(copyAction, &QAction::triggered, this, [target]() {
        QApplication::clipboard()->setText(target.text);
    });

    QAction* replyAction = menu->addAction(QStringLiteral("回复"));
    connect(replyAction, &QAction::triggered, this, [this, target]() {
        if (target.isReply) {
            emit replyToReplyRequested(target.commentId, target.replyId);
        } else {
            emit replyToCommentRequested(target.commentId);
        }
    });

    connect(menu, &QMenu::aboutToHide, menu, [menu]() {
        menu->deleteLater();
    });

    menu->popupWhenMouseReleased(globalPos);
}
