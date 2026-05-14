#include "PostFeedPage.h"

#include <QTimer>

#include "PostCardDelegate.h"
#include "features/post/model/PostFeedModel.h"
#include "features/post/ui/PostSessionController.h"
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

    connect(this, &PostMasonryView::reachedBottom, this, &PostFeedPage::scheduleLoadMore);
    connect(this, &PostMasonryView::postActivated,
            this, &PostFeedPage::onPostActivated);
    connect(this, &PostMasonryView::postLikeRequested,
            this, &PostFeedPage::onPostLikeRequested);
}

void PostFeedPage::setController(PostSessionController* controller)
{
    if (m_controller == controller) {
        return;
    }

    if (m_postUpdatedConnection) {
        disconnect(m_postUpdatedConnection);
        m_postUpdatedConnection = {};
    }

    m_controller = controller;
    if (!m_controller) {
        return;
    }

    m_postUpdatedConnection = connect(m_controller, &PostSessionController::postUpdated,
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
    m_loading = false;
    m_loadMoreScheduled = false;
    m_model->setPosts(posts);
    m_nextOffset = posts.size();
    m_hasMore = posts.size() >= kPageSize;
}

void PostFeedPage::loadMore()
{
    m_loadMoreScheduled = false;
    if (!m_controller || !m_hasMore || m_loading) {
        return;
    }

    m_loading = true;
    const QVector<PostSummary> posts = m_controller->loadFeedPage(m_nextOffset, kPageSize);
    m_loading = false;
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

void PostFeedPage::scheduleLoadMore()
{
    if (m_loadMoreScheduled || m_loading || !m_hasMore) {
        return;
    }

    m_loadMoreScheduled = true;
    QTimer::singleShot(100, this, &PostFeedPage::loadMore);
}

void PostFeedPage::onPostActivated(const PostSummary& summary, const QRect& globalGeometry)
{
    emit postClicked(summary.postId);
    emit postClickedWithGeometry(summary, globalGeometry);
}

void PostFeedPage::onPostLikeRequested(const QString& postId, bool liked)
{
    if (m_controller) {
        m_controller->setPostLiked(postId, liked);
    }
}

void PostFeedPage::onRepositoryPostUpdated(const PostSummary& summary)
{
    m_model->updatePost(summary);
}
