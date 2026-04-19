#pragma once

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QMap>
#include "RepositoryTypes.h"
#include "Post.h"

class PostRepository : public QObject {
    Q_OBJECT
public:
    static PostRepository& instance();
    QVector<PostSummary> requestPostFeed(const PostFeedRequest& query = {}) const;
    PostDetailData requestPostDetail(const PostDetailRequest& query) const;

private:
    explicit PostRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(PostRepository)

    void generateSamplePosts();

    QVector<Post> postList;
    mutable QMutex mutex;
};
