#pragma once

#include <QString>

#include "shared/ui/OverlayScrollListView.h"

class PostCommentDelegate;

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
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    PostCommentDelegate* m_commentDelegate = nullptr;
};
