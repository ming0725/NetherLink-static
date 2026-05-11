#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QMap>
#include <QSet>

#include "shared/types/RepositoryTypes.h"

class PostDetailListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemType {
        PostBodyItem,
        CommentItem
    };

    enum Role {
        ItemTypeRole = Qt::UserRole + 1,
        CommentIdRole,
        CommentRole,
        CommentRevisionRole,
        CommentEngagementRole,
        CommentLayoutRole,
        PostTitleTextRole,
        PostBodyTextRole,
        PostBodyDateTextRole,
        PostBodyRevisionRole
    };

    explicit PostDetailListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void resetForPost(const QString& postId);
    void setPostBody(QString title, QString text, QString dateText);
    void setComments(QVector<PostComment> comments, bool hasMore);
    void appendComments(const QVector<PostComment>& comments, bool hasMore);
    bool hasMoreComments() const;
    int commentCount() const;

    PostComment commentAt(const QModelIndex& index) const;
    const PostComment* commentAtIndex(const QModelIndex& index) const;
    PostComment* commentById(const QString& commentId);
    const PostComment* commentById(const QString& commentId) const;

    void setCommentExpanded(const QString& commentId);
    void setReplyExpanded(const QString& commentId, const QString& replyId);
    void showMoreReplies(const QString& commentId);
    int visibleReplyCount(const QString& commentId) const;
    bool isCommentExpanded(const QString& commentId) const;
    bool isReplyExpanded(const QString& replyId) const;
    void updateCommentLike(const QString& commentId, bool liked);
    void updateReplyLike(const QString& commentId, const QString& replyId, bool liked);
    void insertComment(PostComment comment);
    void insertReply(const QString& commentId, const QString& afterReplyId, PostCommentReply reply);
    int rowForCommentId(const QString& commentId) const;

private:
    void rebuildCommentRowIndex();
    int bumpCommentRevision(const QString& commentId);
    void emitCommentChanged(const QString& commentId, int reasonRole);

    QString m_postId;
    QVector<PostComment> m_comments;
    QHash<QString, int> m_commentRows;
    QHash<QString, int> m_commentRevisions;
    QSet<QString> m_expandedComments;
    QSet<QString> m_expandedReplies;
    QMap<QString, int> m_visibleReplyCounts;
    QString m_postTitleText;
    QString m_postBodyText;
    QString m_postBodyDateText;
    int m_postBodyRevision = 0;
    bool m_hasMoreComments = false;
};
