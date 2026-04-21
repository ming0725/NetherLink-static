#pragma once

#include "PostMasonryView.h"
#include "RepositoryTypes.h"

class PostCardDelegate;
class PostFeedModel;

class PostFeedPage : public PostMasonryView
{
    Q_OBJECT

public:
    explicit PostFeedPage(QWidget* parent = nullptr);
    void setPosts(const QVector<PostSummary>& posts);

signals:
    void postClicked(const QString& postId);
    void postClickedWithGeometry(const PostSummary& summary, QRect globalGeometry);

private slots:
    void loadMore();
    void onPostActivated(const PostSummary& summary, const QRect& globalGeometry);
    void onPostLikeRequested(const QString& postId, bool liked);
    void onRepositoryPostUpdated(const PostSummary& summary);

private:
    PostFeedModel* m_model;
    PostCardDelegate* m_delegate;
    int m_nextOffset = 0;
    bool m_hasMore = true;

    static constexpr int kPageSize = 12;
};
