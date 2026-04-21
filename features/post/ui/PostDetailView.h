#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QJsonObject>
#include "shared/ui/LineEditComponent.h"
#include "shared/ui/OverlayScrollArea.h"
#include "shared/types/RepositoryTypes.h"

class PostDetailScrollArea : public OverlayScrollArea {
    Q_OBJECT
protected:
    void layoutContent() override;
public:
    explicit PostDetailScrollArea(QWidget* parent = nullptr);
    void setLabels(QLabel* titleLabel, QLabel* contentLabel);
    void addCommentWidget(QWidget* commentWidget);
    void relayout();
    QLabel* m_titleLabel = nullptr;
    QLabel* m_contentLabel = nullptr;
private:
    QList<QWidget*> m_commentWidgets;
};

class PostDetailView : public QWidget {
    Q_OBJECT
public:
    explicit PostDetailView(QWidget* parent = nullptr);
    void setPreviewSummary(const PostSummary& summary);
    void setPostData(const PostDetailData& data);
    void setImageVisible(bool visible);
    QSize preferredSize(const QSize& availableBounds) const;
    QRect imageRect() const;
    QRect paintedImageRect() const;
    QPixmap transitionPixmap() const;

signals:
    void closed();
    void followClicked(bool followed);
    void likeClicked(bool liked);
    void commentClicked();
protected:
    void resizeEvent(QResizeEvent* ev) override;
    void paintEvent(QPaintEvent*) override;
private:
    QRect fittedImageRect(const QRect& bounds, const QSize& imageSize) const;
    void setupUI();
    void updateLayout();
    void addComment(const QString& content);
    QWidget* createCommentWidget(const QString& userName, const QString& content);
private:
    QString m_previewImageSource;
    QSize m_previewImageSize;
    QString m_fullImageSource;
    QSize m_fullImageSize;
    qreal m_fullImageOpacity = 0.0;
    QLabel* m_authorAvatar;
    QLabel* m_authorName;
    QPushButton* m_followBtn;
    OverlayScrollArea* m_contentArea;
    QWidget* m_contentWidget;
    QLabel* m_titleLabel;
    QLabel* m_contentLabel;
    QPushButton* m_likeBtn;
    QLabel* m_likeCount;
    QPushButton* m_commentBtn;
    QLabel* m_commentCount;
    QString m_postId;
    QString m_authorId;
    LineEditComponent* commentLineEdit;
    bool m_imageVisible = true;
    bool m_isFollowed = false;
    bool m_isLiked = false;
    int m_likes = 0;
    int m_comments = 0;
};
