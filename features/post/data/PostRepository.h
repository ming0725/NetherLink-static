#pragma once

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QMap>
#include "shared/types/RepositoryTypes.h"
#include "shared/types/Post.h"

class PostRepository : public QObject {
    Q_OBJECT
public:
    static PostRepository& instance();
    QVector<PostSummary> requestPostFeed(const PostFeedRequest& query = {}) const;
    PostDetailData requestPostDetail(const PostDetailRequest& query) const;
    QString requestPostDetailAsync(const PostDetailRequest& query, int delayMs = 120);
    bool setPostLiked(const QString& postId, bool liked);
    bool adjustPostCommentCount(const QString& postId, int delta);

signals:
    void postUpdated(const PostSummary& summary);
    void postDetailReady(const QString& requestId, const PostDetailData& detail);

private:
    explicit PostRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(PostRepository)

    struct PostLikeState {
        int likes = 0;
        bool isLiked = false;
    };

    Post buildPostAt(int index) const;
    int postIndexForId(const QString& postId) const;
    PostSummary buildSummary(const Post& post) const;

    QMap<QString, PostLikeState> m_likeStates;
    QMap<QString, int> m_commentCountDeltas;
    mutable QMutex mutex;
};
