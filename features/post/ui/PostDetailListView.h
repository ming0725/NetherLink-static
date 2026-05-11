#pragma once

#include <QString>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QStyleOptionViewItem>

#include "PostCommentDelegate.h"
#include "shared/ui/OverlayScrollListView.h"

class QKeyEvent;

class PostDetailListView : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit PostDetailListView(QWidget* parent = nullptr);
    void setPostCommentDelegate(PostCommentDelegate* delegate);
    void setBackgroundRole(ThemeColor role);

signals:
    void commentLikeRequested(const QString& commentId, bool liked);
    void replyLikeRequested(const QString& commentId, const QString& replyId, bool liked);
    void replyToCommentRequested(const QString& commentId);
    void replyToReplyRequested(const QString& commentId, const QString& replyId);
    void commentExpandRequested(const QString& commentId);
    void replyExpandRequested(const QString& commentId, const QString& replyId);
    void moreRepliesRequested(const QString& commentId);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QStyleOptionViewItem viewOptionForIndex(const QModelIndex& index) const;
    int characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const;
    void copySelectionToClipboard();
    void selectAllTextInActiveTarget();
    void showSelectionMenu(const QPoint& globalPos);
    void showTargetMenu(const QPoint& globalPos,
                        const PostCommentDelegate::InteractionTarget& target);

    PostCommentDelegate* m_commentDelegate = nullptr;
    QPersistentModelIndex m_activeTextIndex;
    PostCommentDelegate::TextTargetType m_activeTextTarget = PostCommentDelegate::NoTextTarget;
    QString m_activeCommentId;
    QString m_activeReplyId;
    QPersistentModelIndex m_dragIndex;
    PostCommentDelegate::TextTargetType m_dragTarget = PostCommentDelegate::NoTextTarget;
    QString m_dragCommentId;
    QString m_dragReplyId;
    int m_dragAnchor = -1;
    bool m_dragging = false;
};
