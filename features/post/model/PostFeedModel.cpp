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

    m_posts[row] = post;
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex);
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
    for (int i = 0; i < m_posts.size(); ++i) {
        if (m_posts.at(i).postId == postId) {
            return i;
        }
    }
    return -1;
}
