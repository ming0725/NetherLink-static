#pragma once

#include <QObject>

#include "shared/types/RepositoryTypes.h"

class PostSessionController : public QObject
{
    Q_OBJECT

public:
    explicit PostSessionController(QObject* parent = nullptr);

    QVector<PostSummary> loadFeedPage(int offset, int limit, bool followOnly = false) const;
    void openPost(const PostSummary& summary);
    void closePost();
    QString currentPostId() const;

    QString requestPostComments(const QString& postId, int offset, int limit);

    bool setCurrentPostLiked(bool liked);
    bool setPostLiked(const QString& postId, bool liked);
    bool setCommentLiked(const QString& commentId, bool liked);
    bool setReplyLiked(const QString& replyId, bool liked);

    PostComment createLocalComment(const QString& postId, const QString& text) const;
    PostCommentReply createLocalReply(const QString& postId,
                                      const QString& commentId,
                                      const QString& targetReplyId,
                                      const QString& targetUserId,
                                      const QString& targetUserName,
                                      const QString& text) const;

signals:
    void postUpdated(const PostSummary& summary);
    void currentPostUpdated(const PostSummary& summary);
    void currentPostDetailLoaded(const PostDetailData& detail);
    void postCommentsLoaded(const QString& requestId, const PostCommentsPage& page);

private:
    QString m_currentPostId;
    QString m_currentDetailRequestId;
};
