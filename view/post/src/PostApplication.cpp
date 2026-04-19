// PostApplication.cpp
#include "PostApplication.h"
#include "PostApplicationBar.h"
#include "PostFeedPage.h"
#include "DefaultPage.h"
#include "CurrentUser.h"
#include "PostDetailView.h"
#include "PostCreatePage.h"
#include <QResizeEvent>
#include <QRandomGenerator>

PostApplication::PostApplication(QWidget* parent)
        : QWidget(parent)
        , m_bar(new PostApplicationBar(this))
        , m_stack(new QStackedWidget(this))
        , m_detailView(nullptr)
        , m_createPage(new PostCreatePage(this))
{
    m_bar->enableBlur();
    auto* postFeedPage = new PostFeedPage(this);
    auto* followFeedPage = new PostFeedPage(this);
    m_stack->addWidget(postFeedPage);
    m_stack->addWidget(followFeedPage);
    m_stack->addWidget(m_createPage);  // 添加创建页面到堆栈
    m_stack->setCurrentIndex(0);
    m_stack->setStyleSheet("background: rgba(0,0,0,0);");
    connect(postFeedPage, &PostFeedPage::postClickedWithGeometry,
            this, &PostApplication::onPostClickedWithGeometry);
    connect(followFeedPage, &PostFeedPage::postClickedWithGeometry,
            this, &PostApplication::onPostClickedWithGeometry);
    connect(m_bar, &PostApplicationBar::pageClicked, this, &PostApplication::onPageChanged);

    if (!m_overlay) {
        m_overlay = new QWidget(this);
        m_overlay->setStyleSheet("background: rgba(0,0,0,100);");
        m_overlay->hide();
        m_overlay->installEventFilter(this);
    }
    m_overlay->setGeometry(rect());
    m_overlay->lower();

    noiseTexture = QImage(100, 100, QImage::Format_ARGB32);
    auto *rng = QRandomGenerator::global();  // 获取全局随机数生成器
    for (int x = 0; x < noiseTexture.width(); ++x) {
        for (int y = 0; y < noiseTexture.height(); ++y) {
            int gray = rng->bounded(56) + 200;   // 200~255
            int alpha = rng->bounded(51) + 30;   // 30~80
            noiseTexture.setPixelColor(x, y, QColor(gray, gray, gray, alpha));
        }
    }
}

void PostApplication::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    const int w = width();
    const int h = height();
    // 1. 让 stack 铺满整个区域
    m_stack->setGeometry(0, 32, w, h);
    // 2. 计算 bar 的居中底部位置
    const int barW = m_bar->width();
    const int barH = m_bar->height();
    const int bottomMargin = 15;
    const int x = (w - barW) / 2;
    const int y = h - barH - bottomMargin;
    m_bar->setGeometry(x, y, barW, barH);
    m_bar->raise();
    m_overlay->setGeometry(rect());

    // 3. 更新详情视图位置
    if (m_detailView) {
        const int margin = 40;
        const int detailW = w - margin * 2;
        const int detailH = h - margin * 2;
        m_detailView->setGeometry((w - detailW) / 2, (h - detailH) / 2, detailW, detailH);
        m_detailView->raise();
    }
}

void PostApplication::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), QColor(255, 255, 255, 64));
    painter.setOpacity(0.5);  // 调整噪声层透明度
    painter.drawTiledPixmap(rect(), QPixmap::fromImage(noiseTexture));
}

bool PostApplication::eventFilter(QObject *obj, QEvent *ev) {
    if (obj == m_overlay && ev->type() == QEvent::MouseButtonPress) {
        if (m_detailView) {
            // 创建一个动画，从当前位置返回到原始位置
            auto *anim = new QPropertyAnimation(m_detailView, "geometry", this);
            anim->setDuration(250);  // 300毫秒的动画
            anim->setStartValue(m_detailView->geometry());
            anim->setEndValue(m_detailView->initialGeometry());  // 使用保存的原始位置
            anim->setEasingCurve(QEasingCurve::InOutQuad);  // 使用平滑的缓动曲线

            // 动画结束后才 deleteLater 并隐藏 overlay
            connect(anim, &QAbstractAnimation::finished, this, [this]() {
                if (m_detailView) {
                    m_detailView->deleteLater();
                    m_detailView = nullptr;
                }
                m_overlay->hide();
            });

            anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
            // 如果 detailView 已经不存在，直接隐藏 overlay
            m_overlay->hide();
        }
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}

void PostApplication::onPostClickedWithGeometry(Post &post, const QRect &sourceGeometry, const QPixmap& originalImage) {

    m_overlay->setGeometry(rect());
    m_overlay->lower();
    m_overlay->show();
    m_overlay->raise();

    // 创建并显示详情视图
    if (!m_detailView) {
        m_detailView = new PostDetailView(this);
        m_detailView->setPostData(post);
        m_detailView->setImage(originalImage);  // 设置原始图片
        m_detailView->hide();  // 先隐藏，等动画准备好再 show()

        // 1. 计算最终要显示的矩形（正常大小）
        const int margin = 40;
        const int finalW = width() - margin * 2;
        const int finalH = height() - margin * 2;
        QRect finalRect((width() - finalW) / 2,
                        (height() - finalH) / 2,
                        finalW, finalH);

        // 2. 将全局坐标转换为PostApplication的局部坐标
        QRect localSourceGeometry = QRect(mapFromGlobal(sourceGeometry.topLeft()),
                                          sourceGeometry.size());

        // 3. 设置初始位置和大小
        m_detailView->setInitialGeometry(localSourceGeometry);
        m_detailView->setGeometry(localSourceGeometry);
        m_detailView->show();
        m_detailView->raise();

        // 4. 创建动画
        auto *anim = new QPropertyAnimation(m_detailView, "geometry", this);
        anim->setDuration(250);
        anim->setStartValue(localSourceGeometry);
        anim->setEndValue(finalRect);
        anim->setEasingCurve(QEasingCurve::OutQuad);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        // 如果已经有 detailView，可以直接更新内容并 show()
        m_detailView->setPostData(post);
        m_detailView->setImage(originalImage);
        m_detailView->show();
        m_detailView->raise();
    }
}

void PostApplication::onPageChanged(int index)
{
    if (index == 2) { // 发表页面
        m_stack->setCurrentWidget(m_createPage);
    } else if (index == 1) { // 关注页面
        m_stack->setCurrentIndex(1);
    } else if (index == 0) { // 首页
        m_stack->setCurrentIndex(0);
    }
}
