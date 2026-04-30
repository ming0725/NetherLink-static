#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QJsonObject>
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/OverlayScrollArea.h"
#include "shared/types/RepositoryTypes.h"

class PostDetailScrollArea : public OverlayScrollArea {
    Q_OBJECT
public:
    explicit PostDetailScrollArea(QWidget* parent = nullptr);
    void setLabels(QLabel* titleLabel, QLabel* contentLabel);
    void relayout();
    QLabel* m_titleLabel = nullptr;
    QLabel* m_contentLabel = nullptr;
};

class PostDetailView : public QWidget {
    Q_OBJECT
public:
    explicit PostDetailView(QWidget* parent = nullptr);
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
    void applySummaryState(const PostSummary& summary, bool resetDetailContent);
    void syncUiFromState();
    void syncEngagementUi();
private:
    State m_state;
    QLabel* m_authorAvatar;
    QLabel* m_authorName;
    QPushButton* m_followBtn;
    QWidget* m_panelContainer;
    PostDetailScrollArea* m_contentArea;
    QLabel* m_titleLabel;
    QLabel* m_contentLabel;
    QPushButton* m_likeBtn;
    QLabel* m_likeCount;
    QPushButton* m_commentBtn;
    QLabel* m_commentCount;
    IconLineEdit* m_commentLineEdit;
};
