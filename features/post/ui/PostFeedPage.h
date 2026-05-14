#pragma once

#include <QMetaObject>

#include "PostMasonryView.h"
#include "shared/types/RepositoryTypes.h"

class PostCardDelegate;
class PostFeedModel;
class PostSessionController;

class PostFeedPage : public PostMasonryView
{
    Q_OBJECT

public:
    explicit PostFeedPage(QWidget* parent = nullptr);
    void setController(PostSessionController* controller);
    void ensureInitialized();
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
    void scheduleLoadMore();

    PostFeedModel* m_model;
    PostCardDelegate* m_delegate;
    PostSessionController* m_controller = nullptr;
    QMetaObject::Connection m_postUpdatedConnection;
    int m_nextOffset = 0;
    bool m_hasMore = true;
    bool m_initialized = false;
    bool m_loading = false;
    bool m_loadMoreScheduled = false;

    static constexpr int kPageSize = 12;
};
