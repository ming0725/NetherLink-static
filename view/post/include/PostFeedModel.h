#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "RepositoryTypes.h"

class PostFeedModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        PostIdRole = Qt::UserRole + 1,
        TitleRole,
        ThumbnailImageRole,
        ThumbnailSizeRole,
        AuthorIdRole,
        AuthorNameRole,
        AuthorAvatarRole,
        LikeCountRole,
        CommentCountRole,
        IsLikedRole
    };

    explicit PostFeedModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setPosts(QVector<PostSummary> posts);
    void appendPosts(const QVector<PostSummary>& posts);
    void updatePost(const PostSummary& post);

    QString postIdAt(const QModelIndex& index) const;
    PostSummary postAt(const QModelIndex& index) const;

private:
    int indexOfPost(const QString& postId) const;

    QVector<PostSummary> m_posts;
};
