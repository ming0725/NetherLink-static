#include "PostFeedModel.h"

PostFeedModel::PostFeedModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int PostFeedModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_posts.size());
}

QVariant PostFeedModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_posts.size()) {
        return {};
    }

    const PostSummary& post = m_posts.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return post.title;
    case PostIdRole:
        return post.postId;
    case ThumbnailImageRole:
        return post.thumbnailImagePath;
    case ThumbnailSizeRole:
        return post.thumbnailImageSize;
    case AuthorIdRole:
        return post.authorId;
    case AuthorNameRole:
        return post.authorName;
    case AuthorAvatarRole:
        return post.authorAvatarPath;
    case LikeCountRole:
        return post.likeCount;
    case CommentCountRole:
        return post.commentCount;
    case IsLikedRole:
        return post.isLiked;
    default:
        return {};
    }
}

Qt::ItemFlags PostFeedModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void PostFeedModel::setPosts(QVector<PostSummary> posts)
{
    beginResetModel();
    m_posts = std::move(posts);
    rebuildPostIndex();
    endResetModel();
}

void PostFeedModel::appendPosts(const QVector<PostSummary>& posts)
{
    if (posts.isEmpty()) {
        return;
    }

    const int start = m_posts.size();
    const int end = start + posts.size() - 1;
    beginInsertRows(QModelIndex(), start, end);
    for (const PostSummary& post : posts) {
        if (!post.postId.isEmpty()) {
            m_postRows.insert(post.postId, m_posts.size());
        }
        m_posts.append(post);
    }
    endInsertRows();
}

void PostFeedModel::updatePost(const PostSummary& post)
{
    const int row = indexOfPost(post.postId);
    if (row < 0) {
        return;
    }

    const PostSummary previous = m_posts.at(row);
    QVector<int> changedRoles;
    changedRoles.reserve(10);

    if (previous.title != post.title) {
        changedRoles.append(Qt::DisplayRole);
        changedRoles.append(TitleRole);
    }
    if (previous.thumbnailImagePath != post.thumbnailImagePath) {
        changedRoles.append(ThumbnailImageRole);
    }
    if (previous.thumbnailImageSize != post.thumbnailImageSize) {
        changedRoles.append(ThumbnailSizeRole);
    }
    if (previous.authorId != post.authorId) {
        changedRoles.append(AuthorIdRole);
    }
    if (previous.authorName != post.authorName) {
        changedRoles.append(AuthorNameRole);
    }
    if (previous.authorAvatarPath != post.authorAvatarPath) {
        changedRoles.append(AuthorAvatarRole);
    }
    if (previous.likeCount != post.likeCount) {
        changedRoles.append(LikeCountRole);
    }
    if (previous.commentCount != post.commentCount) {
        changedRoles.append(CommentCountRole);
    }
    if (previous.isLiked != post.isLiked) {
        changedRoles.append(IsLikedRole);
    }

    if (changedRoles.isEmpty()) {
        return;
    }

    m_posts[row] = post;
    if (previous.postId != post.postId) {
        m_postRows.remove(previous.postId);
        if (!post.postId.isEmpty()) {
            m_postRows.insert(post.postId, row);
        }
    }
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, changedRoles);
}

QString PostFeedModel::postIdAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_posts.size()) {
        return {};
    }
    return m_posts.at(index.row()).postId;
}

PostSummary PostFeedModel::postAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_posts.size()) {
        return {};
    }
    return m_posts.at(index.row());
}

int PostFeedModel::indexOfPost(const QString& postId) const
{
    return m_postRows.value(postId, -1);
}

void PostFeedModel::rebuildPostIndex()
{
    m_postRows.clear();
    m_postRows.reserve(m_posts.size());
    for (int i = 0; i < m_posts.size(); ++i) {
        const QString& postId = m_posts.at(i).postId;
        if (!postId.isEmpty()) {
            m_postRows.insert(postId, i);
        }
    }
}
