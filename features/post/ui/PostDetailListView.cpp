#include "PostDetailListView.h"

#include <QMouseEvent>

#include "PostCommentDelegate.h"

PostDetailListView::PostDetailListView(QWidget* parent)
    : OverlayScrollListView(parent)
{
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setContextMenuPolicy(Qt::NoContextMenu);
    viewport()->setContextMenuPolicy(Qt::NoContextMenu);
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

void PostDetailListView::mouseMoveEvent(QMouseEvent* event)
{
    bool hasAction = false;
    if (m_commentDelegate && model()) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            QStyleOptionViewItem option;
            initViewItemOption(&option);
            option.widget = viewport();
            option.rect = visualRect(index);
            hasAction = m_commentDelegate->actionAt(option, index, event->pos()).action
                    != PostCommentDelegate::NoAction;
        }
    }

    if (hasAction) {
        viewport()->setCursor(Qt::PointingHandCursor);
    } else {
        viewport()->unsetCursor();
    }
    OverlayScrollListView::mouseMoveEvent(event);
}

void PostDetailListView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_commentDelegate || !model()) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.widget = viewport();
    option.rect = visualRect(index);
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

    OverlayScrollListView::mousePressEvent(event);
}

void PostDetailListView::leaveEvent(QEvent* event)
{
    viewport()->unsetCursor();
    OverlayScrollListView::leaveEvent(event);
}
