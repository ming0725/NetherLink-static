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

signals:
    void postUpdated(const PostSummary& summary);
    void postDetailReady(const QString& requestId, const PostDetailData& detail);

private:
    explicit PostRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(PostRepository)

    void generateSamplePosts();
    PostSummary buildSummary(const Post& post) const;

    QVector<Post> postList;
    mutable QMutex mutex;
};
