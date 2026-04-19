#pragma once

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QMap>
#include "Post.h"

class PostRepository : public QObject {
    Q_OBJECT
public:
    static PostRepository& instance();
    QVector<Post> getAllPosts() const;
    Post getPost(const QString& postID) const;

private:
    explicit PostRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(PostRepository)

    void generateSamplePosts();

    QVector<Post> postList;
    mutable QMutex mutex;
};
