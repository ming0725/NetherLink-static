// PostPreviewItem.h
#pragma once

#include <QWidget>
#include <QLabel>
#include "ClickableLabel.h"
#include "RepositoryTypes.h"

class PostPreviewItem : public QWidget {
    Q_OBJECT
public:
    explicit PostPreviewItem(const PostSummary& post,
                             QWidget* parent = nullptr);

    int scaledHeightFor(double itemW);
signals:
    void viewPost(QString postID);
    void viewAuthor();
    void loadFinished();
    void viewPostWithGeometry(const QString& postId, QRect globalGeometry);
private slots:
    void onViewPost();
    void onViewAuthor();
    void onClickLike();
protected:
    void resizeEvent(QResizeEvent* ev) Q_DECL_OVERRIDE;
private:
    void setupUI(const QString&);
    int imageBaseWidth = 200;
    int imageBaseHeight = 0;
    PostSummary m_post;
    QString m_authorName;
    // UI 元素
    ClickableLabel* m_imageLabel;
    ClickableLabel* m_titleLabel;
    ClickableLabel* m_avatarLabel;
    ClickableLabel* m_authorLabel;
    ClickableLabel* m_likeIconLabel;
    ClickableLabel* m_likeCountLabel;
    // 参数
    static constexpr int MinWidth = 200;
    static constexpr int MaxImgH  = 300;
    static constexpr int Margin   = 6;
    static constexpr int AvatarR  = 30; // 头像直径
};
