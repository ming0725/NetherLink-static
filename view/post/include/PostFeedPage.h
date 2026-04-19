// PostFeedPage.h
#pragma once

#include "CustomScrollArea.h"
#include "PostPreviewItem.h"
#include "Post.h"
#include <QVector>
#include <QWidget>


class PostFeedPage : public CustomScrollArea {
    Q_OBJECT
public:
    explicit PostFeedPage(QWidget* parent = nullptr);
    // 设置数据源
    void setPosts(const QVector<Post>& posts);
signals:
    void loadMore();
    void postClicked(const Post& data);
    void postClickedWithGeometry(Post& post, QRect globalGeometry, QPixmap originalImage);
protected:
    void layoutContent() Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
private:
    QVector<Post>           m_data;
    QVector<PostPreviewItem*>   m_items;
    // 布局参数
    const int margin    = 16;
    const int topMargin = 2;
    const int hgap      = 12;
    const int vgap      = 12;
    const int minItemW  = 200;  // 最小宽度
};
