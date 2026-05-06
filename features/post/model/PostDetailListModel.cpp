#include "PostDetailListModel.h"

PostDetailListModel::PostDetailListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int PostDetailListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 1 + m_comments.size();
}

QVariant PostDetailListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    if (index.row() == 0) {
        switch (role) {
        case Qt::DisplayRole:
            return QString();
        case ItemTypeRole:
            return DetailItem;
        case DetailHeightRole:
            return m_detailHeight;
        default:
            return {};
        }
    }

    const PostComment& comment = m_comments.at(index.row() - 1);
    switch (role) {
    case Qt::DisplayRole:
        return comment.content;
    case ItemTypeRole:
        return CommentItem;
    case CommentIdRole:
        return comment.commentId;
    case CommentRevisionRole:
        return m_commentRevisions.value(comment.commentId, 0);
    default:
        return {};
    }
}

Qt::ItemFlags PostDetailListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled;
}

void PostDetailListModel::resetForPost(const QString& postId)
{
    if (!m_comments.isEmpty()) {
        beginRemoveRows(QModelIndex(), 1, m_comments.size());
        m_comments.clear();
        endRemoveRows();
    }
    m_postId = postId;
    m_commentRows.clear();
    m_commentRevisions.clear();
    m_expandedComments.clear();
    m_expandedReplies.clear();
    m_visibleReplyCounts.clear();
    m_hasMoreComments = false;
    const QModelIndex detailIndex = index(0, 0);
    emit dataChanged(detailIndex, detailIndex, {DetailHeightRole});
}

void PostDetailListModel::setDetailHeight(int height)
{
    const int boundedHeight = qMax(1, height);
    if (m_detailHeight == boundedHeight) {
        return;
    }
    m_detailHeight = boundedHeight;
    const QModelIndex detailIndex = index(0, 0);
    emit dataChanged(detailIndex, detailIndex, {DetailHeightRole});
}

void PostDetailListModel::setComments(QVector<PostComment> comments, bool hasMore)
{
    if (!m_comments.isEmpty()) {
        beginRemoveRows(QModelIndex(), 1, m_comments.size());
        m_comments.clear();
        endRemoveRows();
    }
    m_expandedComments.clear();
    m_expandedReplies.clear();
    m_visibleReplyCounts.clear();
    if (!comments.isEmpty()) {
        beginInsertRows(QModelIndex(), 1, comments.size());
        m_comments = std::move(comments);
        for (const PostComment& comment : m_comments) {
            m_visibleReplyCounts.insert(comment.commentId, qMin(1, comment.totalReplyCount));
            m_commentRevisions.insert(comment.commentId, 0);
        }
        rebuildCommentRowIndex();
        endInsertRows();
    } else {
        m_commentRows.clear();
        m_commentRevisions.clear();
    }
    m_hasMoreComments = hasMore;
}

void PostDetailListModel::appendComments(const QVector<PostComment>& comments, bool hasMore)
{
    if (comments.isEmpty()) {
        m_hasMoreComments = hasMore;
        return;
    }

    const int start = rowCount();
    const int end = start + comments.size() - 1;
    beginInsertRows(QModelIndex(), start, end);
    for (const PostComment& comment : comments) {
        m_comments.append(comment);
        m_visibleReplyCounts.insert(comment.commentId, qMin(1, comment.totalReplyCount));
        m_commentRevisions.insert(comment.commentId, 0);
    }
    rebuildCommentRowIndex();
    m_hasMoreComments = hasMore;
    endInsertRows();
}

bool PostDetailListModel::hasMoreComments() const
{
    return m_hasMoreComments;
}

int PostDetailListModel::commentCount() const
{
    return m_comments.size();
}

bool PostDetailListModel::isDetailIndex(const QModelIndex& index) const
{
    return index.isValid() && index.row() == 0;
}

PostComment PostDetailListModel::commentAt(const QModelIndex& index) const
{
    const PostComment* comment = commentAtIndex(index);
    return comment ? *comment : PostComment{};
}

const PostComment* PostDetailListModel::commentAtIndex(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() <= 0 || index.row() > m_comments.size()) {
        return {};
    }
    return &m_comments.at(index.row() - 1);
}

PostComment* PostDetailListModel::commentById(const QString& commentId)
{
    const int row = rowForCommentId(commentId);
    if (row <= 0 || row > m_comments.size()) {
        return nullptr;
    }
    return &m_comments[row - 1];
}

const PostComment* PostDetailListModel::commentById(const QString& commentId) const
{
    const int row = rowForCommentId(commentId);
    if (row <= 0 || row > m_comments.size()) {
        return nullptr;
    }
    return &m_comments.at(row - 1);
}

void PostDetailListModel::setCommentExpanded(const QString& commentId)
{
    if (m_expandedComments.contains(commentId)) {
        return;
    }
    m_expandedComments.insert(commentId);
    emitCommentChanged(commentId, CommentLayoutRole);
}

void PostDetailListModel::setReplyExpanded(const QString& commentId, const QString& replyId)
{
    if (m_expandedReplies.contains(replyId)) {
        return;
    }
    m_expandedReplies.insert(replyId);
    emitCommentChanged(commentId, CommentLayoutRole);
}

void PostDetailListModel::showMoreReplies(const QString& commentId)
{
    const PostComment* comment = commentById(commentId);
    if (!comment) {
        return;
    }

    const int current = visibleReplyCount(commentId);
    const int next = qMin(comment->totalReplyCount, current + 3);
    if (next == current) {
        return;
    }
    m_visibleReplyCounts.insert(commentId, next);
    emitCommentChanged(commentId, CommentLayoutRole);
}

int PostDetailListModel::visibleReplyCount(const QString& commentId) const
{
    return m_visibleReplyCounts.value(commentId, 0);
}

bool PostDetailListModel::isCommentExpanded(const QString& commentId) const
{
    return m_expandedComments.contains(commentId);
}

bool PostDetailListModel::isReplyExpanded(const QString& replyId) const
{
    return m_expandedReplies.contains(replyId);
}

void PostDetailListModel::updateCommentLike(const QString& commentId, bool liked)
{
    PostComment* comment = commentById(commentId);
    if (!comment || comment->isLiked == liked) {
        return;
    }
    comment->isLiked = liked;
    comment->likeCount = qMax(0, comment->likeCount + (liked ? 1 : -1));
    emitCommentChanged(commentId, CommentEngagementRole);
}

void PostDetailListModel::updateReplyLike(const QString& commentId, const QString& replyId, bool liked)
{
    PostComment* comment = commentById(commentId);
    if (!comment) {
        return;
    }
    for (PostCommentReply& reply : comment->replies) {
        if (reply.replyId == replyId && reply.isLiked != liked) {
            reply.isLiked = liked;
            reply.likeCount = qMax(0, reply.likeCount + (liked ? 1 : -1));
            emitCommentChanged(commentId, CommentEngagementRole);
            return;
        }
    }
}

void PostDetailListModel::insertComment(PostComment comment)
{
    if (comment.commentId.isEmpty()) {
        return;
    }

    beginInsertRows(QModelIndex(), 1, 1);
    m_visibleReplyCounts.insert(comment.commentId, comment.replies.size());
    m_commentRevisions.insert(comment.commentId, 0);
    m_comments.prepend(std::move(comment));
    rebuildCommentRowIndex();
    endInsertRows();
}

void PostDetailListModel::insertReply(const QString& commentId, const QString& afterReplyId, PostCommentReply reply)
{
    PostComment* comment = commentById(commentId);
    if (!comment || reply.replyId.isEmpty()) {
        return;
    }

    int insertAt = 0;
    if (!afterReplyId.isEmpty()) {
        insertAt = comment->replies.size();
        for (int i = 0; i < comment->replies.size(); ++i) {
            if (comment->replies.at(i).replyId == afterReplyId) {
                insertAt = i + 1;
                break;
            }
        }
    }

    insertAt = qBound(0, insertAt, comment->replies.size());
    reply.commentId = commentId;
    comment->replies.insert(insertAt, std::move(reply));
    comment->totalReplyCount = comment->replies.size();
    m_visibleReplyCounts.insert(commentId, qMax(visibleReplyCount(commentId), insertAt + 1));
    emitCommentChanged(commentId, CommentLayoutRole);
}

int PostDetailListModel::rowForCommentId(const QString& commentId) const
{
    return m_commentRows.value(commentId, -1);
}

void PostDetailListModel::rebuildCommentRowIndex()
{
    m_commentRows.clear();
    m_commentRows.reserve(m_comments.size());
    for (int i = 0; i < m_comments.size(); ++i) {
        m_commentRows.insert(m_comments.at(i).commentId, i + 1);
    }
}

int PostDetailListModel::bumpCommentRevision(const QString& commentId)
{
    const int nextRevision = m_commentRevisions.value(commentId, 0) + 1;
    m_commentRevisions.insert(commentId, nextRevision);
    return nextRevision;
}

void PostDetailListModel::emitCommentChanged(const QString& commentId, int reasonRole)
{
    const int row = rowForCommentId(commentId);
    if (row < 0) {
        return;
    }
    bumpCommentRevision(commentId);
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {CommentIdRole, CommentRevisionRole, reasonRole});
}
