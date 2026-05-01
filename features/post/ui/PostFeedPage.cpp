#include "PostFeedPage.h"

#include <QTimer>

#include "PostCardDelegate.h"
#include "features/post/model/PostFeedModel.h"
#include "features/post/data/PostRepository.h"
#include "shared/theme/ThemeManager.h"

PostFeedPage::PostFeedPage(QWidget* parent)
    : PostMasonryView(parent)
    , m_model(new PostFeedModel(this))
    , m_delegate(new PostCardDelegate(this))
{
#ifdef Q_OS_WIN
    setAutoFillBackground(true);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::WindowBackground));
    setPalette(palette);
#endif
    setModel(m_model);
    setCardDelegate(m_delegate);

    connect(this, &PostMasonryView::reachedBottom, this, [this]() {
        QTimer::singleShot(100, this, &PostFeedPage::loadMore);
    });
    connect(this, &PostMasonryView::postActivated,
            this, &PostFeedPage::onPostActivated);
    connect(this, &PostMasonryView::postLikeRequested,
            this, &PostFeedPage::onPostLikeRequested);
    connect(&PostRepository::instance(), &PostRepository::postUpdated,
            this, &PostFeedPage::onRepositoryPostUpdated);
}

void PostFeedPage::ensureInitialized()
{
    if (m_initialized) {
        return;
    }

    m_initialized = true;
    loadMore();
}

void PostFeedPage::setPosts(const QVector<PostSummary>& posts)
{
    m_initialized = true;
    m_model->setPosts(posts);
    m_nextOffset = posts.size();
    m_hasMore = posts.size() >= kPageSize;
}

void PostFeedPage::loadMore()
{
    if (!m_hasMore) {
        return;
    }

    const QVector<PostSummary> posts = PostRepository::instance().requestPostFeed({m_nextOffset, kPageSize});
    if (posts.isEmpty()) {
        m_hasMore = false;
        return;
    }

    if (m_nextOffset == 0) {
        m_model->setPosts(posts);
    } else {
        m_model->appendPosts(posts);
    }
    m_nextOffset += posts.size();
    m_hasMore = posts.size() >= kPageSize;
}

void PostFeedPage::onPostActivated(const PostSummary& summary, const QRect& globalGeometry)
{
    emit postClicked(summary.postId);
    emit postClickedWithGeometry(summary, globalGeometry);
}

void PostFeedPage::onPostLikeRequested(const QString& postId, bool liked)
{
    PostRepository::instance().setPostLiked(postId, liked);
}

void PostFeedPage::onRepositoryPostUpdated(const PostSummary& summary)
{
    m_model->updatePost(summary);
}
