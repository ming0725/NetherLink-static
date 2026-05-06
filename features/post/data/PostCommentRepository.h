#pragma once

#include <QMap>
#include <QMutex>
#include <QObject>

#include "shared/types/RepositoryTypes.h"

class PostCommentRepository : public QObject
{
    Q_OBJECT

public:
    static PostCommentRepository& instance();

    PostCommentsPage requestPostComments(const PostCommentsRequest& query) const;
    QString requestPostCommentsAsync(const PostCommentsRequest& query, int delayMs = 0);
    bool setCommentLiked(const QString& commentId, bool liked);
    bool setReplyLiked(const QString& replyId, bool liked);

signals:
    void postCommentsReady(const QString& requestId, const PostCommentsPage& page);

private:
    explicit PostCommentRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(PostCommentRepository)

    struct LikeState {
        int likes = 0;
        bool isLiked = false;
    };

    PostComment buildCommentAt(const QString& postId, int index) const;
    PostCommentReply buildReplyAt(const QString& postId,
                                  const QString& commentId,
                                  const QString& parentAuthorId,
                                  int commentIndex,
                                  int replyIndex) const;
    int commentIndexForId(const QString& commentId) const;
    int replyIndexForId(const QString& replyId) const;

    QMap<QString, LikeState> m_commentLikeStates;
    QMap<QString, LikeState> m_replyLikeStates;
    mutable QMutex m_mutex;
};
