#pragma once

#include <QDateTime>
#include <QHash>
#include <QMetaObject>
#include <QPixmap>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QWidget>

#include "shared/types/RepositoryTypes.h"

class IconLineEdit;
class QLabel;
class PostCommentDelegate;
class PostDetailListModel;
class PostDetailListView;
class PostSessionController;
class QPushButton;
class QVariantAnimation;

class PostDetailView : public QWidget {
    Q_OBJECT
public:
    explicit PostDetailView(QWidget* parent = nullptr);
    void setController(PostSessionController* controller);
    void setPreviewSummary(const PostSummary& summary);
    void setPostData(const PostDetailData& data);
    void updatePostSummary(const PostSummary& summary);
    void setImageVisible(bool visible);
    QWidget* panelWidget() const;
    QSize preferredSize(const QSize& availableBounds) const;
    QRect imageRect() const;
    QRect paintedImageRect() const;
    QPixmap transitionPixmap() const;

signals:
    void closed();
    void followClicked(bool followed);
    void likeClicked(bool liked);
protected:
    void resizeEvent(QResizeEvent* ev) override;
    void paintEvent(QPaintEvent*) override;
private:
    struct State {
        QString postId;
        QString authorId;
        QString authorName;
        QString authorAvatarPath;
        QString title;
        QString content;
        QDateTime contentCreatedAt;
        QString previewImageSource;
        QSize previewImageSize;
        QString fullImageSource;
        QSize fullImageSize;
        qreal fullImageOpacity = 0.0;
        bool imageVisible = true;
        bool isFollowed = false;
        bool isLiked = false;
        int likeCount = 0;
        int commentCount = 0;
    };

    QRect fittedImageRect(const QRect& bounds, const QSize& imageSize) const;
    void setupUI();
    void updateLayout();
    void applyTheme();
    void applySummaryState(const PostSummary& summary, bool resetDetailContent);
    void syncUiFromState();
    void syncEngagementUi();
    void loadInitialComments();
    void loadMoreComments();
    void maybeLoadMoreComments();
    void scheduleMaybeLoadMoreComments();
    void onCommentsReady(const QString& requestId, const PostCommentsPage& page);
    void animateCommentExpansion(const QString& commentId);
    void animateReplyExpansion(const QString& commentId, const QString& replyId);
    void animateMoreReplies(const QString& commentId);
    void stopCommentAnimations();
    void stopImageFadeAnimation();
    void setReplyTarget(const QString& commentId, const QString& replyId = QString());
    void clearReplyTarget();
    void submitCommentText();
private:
    struct ReplyTarget {
        QString commentId;
        QString replyId;
    };

    State m_state;
    ReplyTarget m_replyTarget;
    QLabel* m_authorAvatar;
    QLabel* m_authorName;
    QPushButton* m_followBtn;
    QWidget* m_panelContainer;
    PostDetailListView* m_contentList;
    PostDetailListModel* m_detailModel;
    PostCommentDelegate* m_commentDelegate;
    PostSessionController* m_controller = nullptr;
    QMetaObject::Connection m_commentsLoadedConnection;
    QPushButton* m_likeBtn;
    QLabel* m_likeCount;
    QPushButton* m_commentBtn;
    QLabel* m_commentCount;
    IconLineEdit* m_commentLineEdit;
    bool m_loadingComments = false;
    bool m_pendingLoadMoreCheck = false;
    QString m_commentsRequestId;
    int m_pendingCommentsOffset = -1;
    int m_commentPageSize = 12;
    QHash<QString, QPointer<QVariantAnimation>> m_commentExpansionAnimations;
    QHash<QString, QPointer<QVariantAnimation>> m_replyExpansionAnimations;
    QHash<QString, QPointer<QVariantAnimation>> m_moreReplyAnimations;
    QPointer<QVariantAnimation> m_imageFadeAnimation;
};
