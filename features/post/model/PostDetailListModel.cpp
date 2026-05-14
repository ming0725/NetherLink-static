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
        case PostBodyTextRole:
            return m_postBodyText;
        case ItemTypeRole:
            return PostBodyItem;
        case PostTitleTextRole:
            return m_postTitleText;
        case PostBodyDateTextRole:
            return m_postBodyDateText;
        case PostBodyRevisionRole:
            return m_postBodyRevision;
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
    m_commentExpansionProgress.clear();
    m_replyExpansionProgress.clear();
    m_replyRevealProgress.clear();
    m_loadMoreRepliesPlaceholderProgress.clear();
    m_hasMoreComments = false;
    m_postTitleText.clear();
    m_postBodyText.clear();
    m_postBodyDateText.clear();
    ++m_postBodyRevision;
    const QModelIndex bodyIndex = index(0, 0);
    emit dataChanged(bodyIndex, bodyIndex,
                     {PostTitleTextRole, PostBodyTextRole, PostBodyDateTextRole, PostBodyRevisionRole});
}

void PostDetailListModel::setPostBody(QString title, QString text, QString dateText)
{
    if (m_postTitleText == title && m_postBodyText == text && m_postBodyDateText == dateText) {
        return;
    }

    m_postTitleText = std::move(title);
    m_postBodyText = std::move(text);
    m_postBodyDateText = std::move(dateText);
    ++m_postBodyRevision;
    const QModelIndex bodyIndex = index(0, 0);
    emit dataChanged(bodyIndex,
                     bodyIndex,
                     {PostTitleTextRole, PostBodyTextRole, PostBodyDateTextRole, PostBodyRevisionRole, CommentLayoutRole});
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
    m_commentExpansionProgress.clear();
    m_replyExpansionProgress.clear();
    m_replyRevealProgress.clear();
    m_loadMoreRepliesPlaceholderProgress.clear();
    if (!comments.isEmpty()) {
        beginInsertRows(QModelIndex(), 1, comments.size());
        m_comments = std::move(comments);
        for (const PostComment& comment : m_comments) {
            m_visibleReplyCounts.insert(comment.commentId,
                                        qMin(1, qMin(comment.totalReplyCount, comment.replies.size())));
            m_commentRevisions.insert(comment.commentId, 0);
            const int visibleReplies = qMin(1, comment.replies.size());
            for (int i = 0; i < visibleReplies; ++i) {
                m_replyRevealProgress.insert(comment.replies.at(i).replyId, 1.0);
            }
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
        m_visibleReplyCounts.insert(comment.commentId,
                                    qMin(1, qMin(comment.totalReplyCount, comment.replies.size())));
        m_commentRevisions.insert(comment.commentId, 0);
        const int visibleReplies = qMin(1, comment.replies.size());
        for (int i = 0; i < visibleReplies; ++i) {
            m_replyRevealProgress.insert(comment.replies.at(i).replyId, 1.0);
        }
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

PostComment PostDetailListModel::commentAt(const QModelIndex& index) const
{
    const PostComment* comment = commentAtIndex(index);
    return comment ? *comment : PostComment{};
}

const PostComment* PostDetailListModel::commentAtIndex(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 1 || index.row() >= rowCount()) {
        return {};
    }
    return &m_comments.at(index.row() - 1);
}

PostComment* PostDetailListModel::commentById(const QString& commentId)
{
    const int row = rowForCommentId(commentId);
    if (row < 1 || row >= rowCount()) {
        return nullptr;
    }
    return &m_comments[row - 1];
}

const PostComment* PostDetailListModel::commentById(const QString& commentId) const
{
    const int row = rowForCommentId(commentId);
    if (row < 1 || row >= rowCount()) {
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
    m_commentExpansionProgress.insert(commentId, 1.0);
    emitCommentChanged(commentId, CommentLayoutRole);
}

void PostDetailListModel::setReplyExpanded(const QString& commentId, const QString& replyId)
{
    if (m_expandedReplies.contains(replyId)) {
        return;
    }
    m_expandedReplies.insert(replyId);
    m_replyExpansionProgress.insert(replyId, 1.0);
    emitCommentChanged(commentId, CommentLayoutRole);
}

void PostDetailListModel::showMoreReplies(const QString& commentId)
{
    const PostComment* comment = commentById(commentId);
    if (!comment) {
        return;
    }

    const int current = visibleReplyCount(commentId);
    const int next = qMin(qMin(comment->totalReplyCount, comment->replies.size()), current + 3);
    if (next == current) {
        return;
    }
    m_visibleReplyCounts.insert(commentId, next);
    m_loadMoreRepliesPlaceholderProgress.remove(commentId);
    for (int i = current; i < next && i < comment->replies.size(); ++i) {
        m_replyRevealProgress.insert(comment->replies.at(i).replyId, 1.0);
    }
    emitCommentChanged(commentId, CommentLayoutRole);
}

void PostDetailListModel::beginCommentExpansion(const QString& commentId)
{
    if (commentId.isEmpty() || m_expandedComments.contains(commentId)) {
        return;
    }
    m_expandedComments.insert(commentId);
    m_commentExpansionProgress.insert(commentId, 0.0);
    emitCommentChanged(commentId, CommentLayoutRole);
}

void PostDetailListModel::beginReplyExpansion(const QString& commentId, const QString& replyId)
{
    if (commentId.isEmpty() || replyId.isEmpty() || m_expandedReplies.contains(replyId)) {
        return;
    }
    m_expandedReplies.insert(replyId);
    m_replyExpansionProgress.insert(replyId, 0.0);
    emitCommentChanged(commentId, CommentLayoutRole);
}

QStringList PostDetailListModel::beginShowMoreReplies(const QString& commentId)
{
    const PostComment* comment = commentById(commentId);
    if (!comment) {
        return {};
    }

    const int current = visibleReplyCount(commentId);
    const int next = qMin(qMin(comment->totalReplyCount, comment->replies.size()), current + 3);
    if (next == current) {
        return {};
    }

    QStringList replyIds;
    for (int i = current; i < next && i < comment->replies.size(); ++i) {
        const QString& replyId = comment->replies.at(i).replyId;
        replyIds.push_back(replyId);
        m_replyRevealProgress.insert(replyId, 0.0);
    }

    m_visibleReplyCounts.insert(commentId, next);
    if (next >= comment->totalReplyCount) {
        m_loadMoreRepliesPlaceholderProgress.insert(commentId, 1.0);
    } else {
        m_loadMoreRepliesPlaceholderProgress.remove(commentId);
    }
    emitCommentChanged(commentId, CommentLayoutRole);
    return replyIds;
}

void PostDetailListModel::setCommentExpansionProgress(const QString& commentId, qreal progress)
{
    if (commentId.isEmpty()) {
        return;
    }

    const qreal boundedProgress = qBound(0.0, progress, 1.0);
    if (qAbs(m_commentExpansionProgress.value(commentId, 0.0) - boundedProgress) <= 0.0001) {
        return;
    }
    m_expandedComments.insert(commentId);
    m_commentExpansionProgress.insert(commentId, boundedProgress);
    emitCommentChanged(commentId, CommentLayoutRole);
}

void PostDetailListModel::setReplyExpansionProgress(const QString& commentId, const QString& replyId, qreal progress)
{
    if (commentId.isEmpty() || replyId.isEmpty()) {
        return;
    }

    const qreal boundedProgress = qBound(0.0, progress, 1.0);
    if (qAbs(m_replyExpansionProgress.value(replyId, 0.0) - boundedProgress) <= 0.0001) {
        return;
    }
    m_expandedReplies.insert(replyId);
    m_replyExpansionProgress.insert(replyId, boundedProgress);
    emitCommentChanged(commentId, CommentLayoutRole);
}

void PostDetailListModel::setReplyRevealProgress(const QString& commentId,
                                                 const QStringList& replyIds,
                                                 qreal progress)
{
    if (commentId.isEmpty() || replyIds.isEmpty()) {
        return;
    }

    const qreal boundedProgress = qBound(0.0, progress, 1.0);
    bool changed = false;
    for (const QString& replyId : replyIds) {
        if (replyId.isEmpty()) {
            continue;
        }
        if (qAbs(m_replyRevealProgress.value(replyId, 1.0) - boundedProgress) <= 0.0001) {
            continue;
        }
        m_replyRevealProgress.insert(replyId, boundedProgress);
        changed = true;
    }

    if (m_loadMoreRepliesPlaceholderProgress.contains(commentId)) {
        const qreal placeholderProgress = 1.0 - boundedProgress;
        if (placeholderProgress <= 0.0001) {
            m_loadMoreRepliesPlaceholderProgress.remove(commentId);
        } else if (qAbs(m_loadMoreRepliesPlaceholderProgress.value(commentId, 0.0) - placeholderProgress) > 0.0001) {
            m_loadMoreRepliesPlaceholderProgress.insert(commentId, placeholderProgress);
        }
        changed = true;
    }

    if (changed) {
        emitCommentChanged(commentId, CommentLayoutRole);
    }
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

qreal PostDetailListModel::commentExpansionProgress(const QString& commentId) const
{
    return m_expandedComments.contains(commentId)
            ? m_commentExpansionProgress.value(commentId, 1.0)
            : 0.0;
}

qreal PostDetailListModel::replyExpansionProgress(const QString& replyId) const
{
    return m_expandedReplies.contains(replyId)
            ? m_replyExpansionProgress.value(replyId, 1.0)
            : 0.0;
}

qreal PostDetailListModel::replyRevealProgress(const QString& replyId) const
{
    return m_replyRevealProgress.value(replyId, 1.0);
}

qreal PostDetailListModel::loadMoreRepliesPlaceholderProgress(const QString& commentId) const
{
    return m_loadMoreRepliesPlaceholderProgress.value(commentId, 0.0);
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
    for (const PostCommentReply& reply : comment.replies) {
        m_replyRevealProgress.insert(reply.replyId, 1.0);
    }
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
    m_replyRevealProgress.insert(comment->replies.at(insertAt).replyId, 1.0);
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
