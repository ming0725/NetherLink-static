#include "features/post/ui/PostSessionController.h"

#include <QDateTime>

#include "app/state/CurrentUser.h"
#include "features/post/data/PostCommentRepository.h"
#include "features/post/data/PostRepository.h"

PostSessionController::PostSessionController(QObject* parent)
    : QObject(parent)
{
    connect(&PostRepository::instance(), &PostRepository::postUpdated,
            this, [this](const PostSummary& summary) {
                emit postUpdated(summary);
                if (!m_currentPostId.isEmpty() && summary.postId == m_currentPostId) {
                    emit currentPostUpdated(summary);
                }
            });
    connect(&PostRepository::instance(), &PostRepository::postDetailReady,
            this, [this](const QString& requestId, const PostDetailData& detail) {
                if (requestId != m_currentDetailRequestId || detail.postId != m_currentPostId) {
                    return;
                }

                m_currentDetailRequestId.clear();
                emit currentPostDetailLoaded(detail);
            });
    connect(&PostCommentRepository::instance(), &PostCommentRepository::postCommentsReady,
            this, &PostSessionController::postCommentsLoaded);
}

QVector<PostSummary> PostSessionController::loadFeedPage(int offset, int limit, bool followOnly) const
{
    return PostRepository::instance().requestPostFeed({offset, limit, followOnly});
}

void PostSessionController::openPost(const PostSummary& summary)
{
    m_currentPostId = summary.postId;
    m_currentDetailRequestId = PostRepository::instance().requestPostDetailAsync({summary.postId});
}

void PostSessionController::closePost()
{
    m_currentPostId.clear();
    m_currentDetailRequestId.clear();
}

QString PostSessionController::currentPostId() const
{
    return m_currentPostId;
}

QString PostSessionController::requestPostComments(const QString& postId, int offset, int limit)
{
    return PostCommentRepository::instance().requestPostCommentsAsync({postId, offset, limit});
}

bool PostSessionController::setCurrentPostLiked(bool liked)
{
    if (m_currentPostId.isEmpty()) {
        return false;
    }
    return setPostLiked(m_currentPostId, liked);
}

bool PostSessionController::setPostLiked(const QString& postId, bool liked)
{
    if (postId.isEmpty()) {
        return false;
    }
    return PostRepository::instance().setPostLiked(postId, liked);
}

bool PostSessionController::setCommentLiked(const QString& commentId, bool liked)
{
    if (commentId.isEmpty()) {
        return false;
    }
    return PostCommentRepository::instance().setCommentLiked(commentId, liked);
}

bool PostSessionController::setReplyLiked(const QString& replyId, bool liked)
{
    if (replyId.isEmpty()) {
        return false;
    }
    return PostCommentRepository::instance().setReplyLiked(replyId, liked);
}

bool PostSessionController::adjustCurrentPostCommentCount(int delta)
{
    if (m_currentPostId.isEmpty() || delta == 0) {
        return false;
    }
    return PostRepository::instance().adjustPostCommentCount(m_currentPostId, delta);
}

PostComment PostSessionController::createLocalComment(const QString& postId, const QString& text) const
{
    const CurrentUser& currentUser = CurrentUser::instance();
    const QDateTime now = QDateTime::currentDateTime();
    const QString idSuffix = QString::number(now.toMSecsSinceEpoch());

    PostComment comment;
    comment.commentId = QStringLiteral("%1-local-c%2").arg(postId, idSuffix);
    comment.postId = postId;
    comment.authorId = currentUser.getUserId();
    comment.authorName = currentUser.getUserName();
    comment.authorAvatarPath = currentUser.getAvatarPath();
    comment.content = text;
    comment.createdAt = now;
    comment.likeCount = 0;
    comment.isLiked = false;
    comment.totalReplyCount = 0;
    return comment;
}

PostCommentReply PostSessionController::createLocalReply(const QString& postId,
                                                         const QString& commentId,
                                                         const QString& targetReplyId,
                                                         const QString& targetUserId,
                                                         const QString& targetUserName,
                                                         const QString& text) const
{
    const CurrentUser& currentUser = CurrentUser::instance();
    const QDateTime now = QDateTime::currentDateTime();
    const QString idSuffix = QString::number(now.toMSecsSinceEpoch());

    PostCommentReply reply;
    reply.replyId = QStringLiteral("%1-local-r%2").arg(commentId, idSuffix);
    reply.commentId = commentId;
    reply.postId = postId;
    reply.authorId = currentUser.getUserId();
    reply.authorName = currentUser.getUserName();
    reply.authorAvatarPath = currentUser.getAvatarPath();
    reply.targetUserId = targetUserId;
    reply.targetUserName = targetUserName;
    reply.targetReplyId = targetReplyId;
    reply.content = text;
    reply.createdAt = now;
    reply.likeCount = 0;
    reply.isLiked = false;
    return reply;
}
