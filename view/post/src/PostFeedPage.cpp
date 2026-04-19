#include "PostFeedPage.h"
#include "PostPreviewItem.h"
#include "PostRepository.h"
#include <QTimer>
#include <QRandomGenerator>
#include <QPainter>

PostFeedPage::PostFeedPage(QWidget* parent)
        : CustomScrollArea(parent)
{
    // 1. 读取 avatar 目录下所有图片文件
    auto samplePosts = PostRepository::instance().getAllPosts();
    setPosts(samplePosts);

    connect(this, &CustomScrollArea::reachedBottom, this, [this]() {
        QTimer::singleShot(100, this, &PostFeedPage::loadMore);
    });

    setStyleSheet("border-width:0px;border-style:solid;");
}

void PostFeedPage::setPosts(const QVector<Post>& posts)
{
    qDeleteAll(m_items);
    m_items.clear();
    m_data = posts;

    for (int i = 0; i < m_data.size(); ++i) {
        const auto& pd = m_data[i];
        // 传入 title 字段
        auto *item = new PostPreviewItem(pd,
                                         contentWidget);
        connect(item, &PostPreviewItem::viewPostWithGeometry, this, &PostFeedPage::postClickedWithGeometry);
        m_items.append(item);
    }
}

void PostFeedPage::layoutContent()
{
    int W = viewport()->width();
    int availableW = W - 2 * margin;
    int cols = std::max(1, (availableW + hgap) / (minItemW + hgap));
    double itemW = (availableW - (cols-1) * hgap) / double(cols);

    QVector<int> colH(cols, topMargin);
    for (auto *it : m_items) {
        int h = it->scaledHeightFor(itemW);
        int col = std::min_element(colH.begin(), colH.end()) - colH.begin();
        int x = margin + col * (itemW + hgap);
        int y = colH[col];
        it->setGeometry(x, y, int(itemW), h);
        colH[col] += h + vgap;
    }
    int maxH = *std::max_element(colH.begin(), colH.end());
    contentWidget->resize(W, maxH + margin);
}

void PostFeedPage::showEvent(QShowEvent *event) {
    layoutContent();
    QWidget::showEvent(event);
}

