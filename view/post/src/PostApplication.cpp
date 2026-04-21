// PostApplication.cpp
#include "PostApplication.h"
#include "PostApplicationBar.h"
#include "PostFeedPage.h"
#include "DefaultPage.h"
#include "CurrentUser.h"
#include "PostDetailView.h"
#include "PostCreatePage.h"
#include "PostOverlay.h"
#include "PostRepository.h"
#include "ImageService.h"
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPainter>
#include <QResizeEvent>

namespace {

class TransitionImageWidget final : public QWidget
{
public:
    explicit TransitionImageWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setPixmap(const QPixmap& pixmap)
    {
        m_pixmap = pixmap;
        update();
    }

    void setRevealProgress(qreal progress)
    {
        const qreal bounded = qBound(0.0, progress, 1.0);
        if (!qFuzzyCompare(m_revealProgress, bounded)) {
            m_revealProgress = bounded;
            update();
        }
    }

    qreal revealProgress() const
    {
        return m_revealProgress;
    }

    QPixmap pixmap() const
    {
        return m_pixmap;
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        if (m_pixmap.isNull()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        const QSizeF sourceSize = QSizeF(m_pixmap.width() / m_pixmap.devicePixelRatio(),
                                         m_pixmap.height() / m_pixmap.devicePixelRatio());
        const QRectF bounds = rect();

        const QSizeF fitted = sourceSize.scaled(bounds.size(), Qt::KeepAspectRatio);
        const QRectF fitTarget((bounds.width() - fitted.width()) / 2.0,
                               (bounds.height() - fitted.height()) / 2.0,
                               fitted.width(),
                               fitted.height());

        QRectF cropSource(QPointF(0, 0), sourceSize);
        if (bounds.width() > 0.0 && bounds.height() > 0.0 &&
            sourceSize.width() > 0.0 && sourceSize.height() > 0.0) {
            const qreal sourceAspect = sourceSize.width() / sourceSize.height();
            const qreal targetAspect = bounds.width() / bounds.height();
            if (sourceAspect > targetAspect) {
                const qreal croppedWidth = sourceSize.height() * targetAspect;
                cropSource = QRectF((sourceSize.width() - croppedWidth) / 2.0,
                                    0.0,
                                    croppedWidth,
                                    sourceSize.height());
            } else {
                const qreal croppedHeight = sourceSize.width() / targetAspect;
                cropSource = QRectF(0.0,
                                    (sourceSize.height() - croppedHeight) / 2.0,
                                    sourceSize.width(),
                                    croppedHeight);
            }
        }

        const auto lerp = [this](qreal a, qreal b) {
            return a + (b - a) * m_revealProgress;
        };
        const QRectF drawSource(lerp(cropSource.x(), 0.0),
                                lerp(cropSource.y(), 0.0),
                                lerp(cropSource.width(), sourceSize.width()),
                                lerp(cropSource.height(), sourceSize.height()));
        const QRectF drawTarget(lerp(0.0, fitTarget.x()),
                                lerp(0.0, fitTarget.y()),
                                lerp(bounds.width(), fitTarget.width()),
                                lerp(bounds.height(), fitTarget.height()));
        painter.drawPixmap(drawTarget, m_pixmap, drawSource);
    }

private:
    QPixmap m_pixmap;
    qreal m_revealProgress = 0.0;
};

TransitionImageWidget* transitionImageWidget(QWidget* widget)
{
    return dynamic_cast<TransitionImageWidget*>(widget);
}

} // namespace

PostApplication::PostApplication(QWidget* parent)
        : QWidget(parent)
        , m_bar(new PostApplicationBar(this))
        , m_stack(new QStackedWidget(this))
        , m_detailView(nullptr)
        , m_createPage(new PostCreatePage(this))
{
    m_bar->enableBlur(false);
    auto* postFeedPage = new PostFeedPage(this);
    auto* followFeedPage = new PostFeedPage(this);
    m_stack->addWidget(postFeedPage);
    m_stack->addWidget(followFeedPage);
    m_stack->addWidget(m_createPage);  // 添加创建页面到堆栈
    m_stack->setCurrentIndex(0);
    connect(postFeedPage, &PostFeedPage::postClickedWithGeometry,
            this, &PostApplication::onPostClickedWithGeometry);
    connect(followFeedPage, &PostFeedPage::postClickedWithGeometry,
            this, &PostApplication::onPostClickedWithGeometry);
    connect(m_bar, &PostApplicationBar::pageClicked, this, &PostApplication::onPageChanged);
    connect(&PostRepository::instance(), &PostRepository::postDetailReady,
            this, &PostApplication::onPostDetailReady);

    if (!m_overlay) {
        m_overlay = new PostOverlay(this);
        m_overlay->setOverlayOpacity(0.0);
        m_overlay->hide();
        m_overlay->installEventFilter(this);
        m_overlayFadeAnimation = new QVariantAnimation(this);
        m_overlayFadeAnimation->setDuration(220);
        m_overlayFadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
        connect(m_overlayFadeAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            if (m_overlay) {
                m_overlay->setOverlayOpacity(value.toReal());
            }
        });
    }
    m_overlay->setGeometry(rect());
    m_overlay->lower();
    updateLayerOrder();
}

void PostApplication::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    const int w = width();
    const int h = height();
    // 1. 让 stack 铺满整个区域
    m_stack->setGeometry(0, kContentTopInset, w, h - kContentTopInset);
    // 2. 计算 bar 的居中底部位置
    const int barW = m_bar->width();
    const int barH = m_bar->height();
    const int bottomMargin = 15;
    const int x = (w - barW) / 2;
    const int y = h - barH - bottomMargin;
    m_bar->setGeometry(x, y, barW, barH);
    m_overlay->setGeometry(rect());

    if (m_detailView) {
        m_detailView->setGeometry(detailRectForCurrentPost());
    }
    if (m_transitionImage && m_detailView && m_transitionPhase == TransitionPhase::Idle) {
        m_transitionImage->setGeometry(QRect(detailRectForCurrentPost().topLeft() + m_detailView->paintedImageRect().topLeft(),
                                             m_detailView->paintedImageRect().size()));
    }
    updateLayerOrder();
}

bool PostApplication::eventFilter(QObject *obj, QEvent *ev) {
    if (obj == m_overlay && ev->type() == QEvent::MouseButtonPress) {
        if (m_detailView) {
            startCloseAnimation();
        } else {
            fadeOverlay(m_overlay ? m_overlay->overlayOpacity() : 1.0, 0.0, true);
        }
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}

void PostApplication::onPostClickedWithGeometry(const PostSummary& summary, const QRect& sourceGeometry)
{
    stopActiveTransition();
    m_openPostId = summary.postId;
    m_openDetailRequestId.clear();
    m_openSourceGeometry = QRect(mapFromGlobal(sourceGeometry.topLeft()), sourceGeometry.size());
    removeTransitionImage();
    clearDetailView();

    m_overlay->setGeometry(rect());
    m_overlay->lower();
    m_overlay->show();
    fadeOverlay(m_overlay ? m_overlay->overlayOpacity() : 0.0, 1.0, false);

    m_detailView = new PostDetailView(this);
    m_detailView->setPreviewSummary(summary);
    m_detailView->setImageVisible(false);
    connect(m_detailView, &PostDetailView::likeClicked, this, [this](bool liked) {
        if (!m_openPostId.isEmpty()) {
            PostRepository::instance().setPostLiked(m_openPostId, liked);
        }
    });
    m_detailView->setGeometry(detailRectForCurrentPost());

    m_detailOpacityEffect = new QGraphicsOpacityEffect(m_detailView);
    m_detailOpacityEffect->setOpacity(0.0);
    m_detailView->setGraphicsEffect(m_detailOpacityEffect);
    m_detailView->show();

    const QRect targetImageGeometry(m_detailView->geometry().topLeft() + m_detailView->paintedImageRect().topLeft(),
                                    m_detailView->paintedImageRect().size());

    auto* transitionImage = new TransitionImageWidget(this);
    QPixmap transitionPixmap = ImageService::instance().pixmap(summary.thumbnailImagePath);
    if (transitionPixmap.isNull()) {
        transitionPixmap = ImageService::instance().centerCrop(summary.thumbnailImagePath,
                                                               m_openSourceGeometry.size(),
                                                               12,
                                                               devicePixelRatioF());
    }
    transitionImage->setPixmap(transitionPixmap);
    transitionImage->setRevealProgress(0.0);
    m_transitionImage = transitionImage;
    m_transitionImage->setGeometry(m_openSourceGeometry);
    m_transitionImage->show();
    updateLayerOrder();

    auto* group = new QParallelAnimationGroup(this);
    m_transitionAnimation = group;
    m_transitionPhase = TransitionPhase::Opening;

    auto* transitionAnim = new QPropertyAnimation(m_transitionImage, "geometry", group);
    transitionAnim->setDuration(260);
    transitionAnim->setStartValue(m_openSourceGeometry);
    transitionAnim->setEndValue(targetImageGeometry);
    transitionAnim->setEasingCurve(QEasingCurve::OutCubic);
    group->addAnimation(transitionAnim);

    auto* revealAnim = new QVariantAnimation(group);
    revealAnim->setDuration(260);
    revealAnim->setStartValue(0.0);
    revealAnim->setEndValue(1.0);
    revealAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(revealAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        if (auto* transitionWidget = transitionImageWidget(m_transitionImage)) {
            transitionWidget->setRevealProgress(value.toReal());
        }
    });
    group->addAnimation(revealAnim);

    auto* detailFade = new QVariantAnimation(group);
    detailFade->setDuration(260);
    detailFade->setStartValue(0.0);
    detailFade->setEndValue(1.0);
    detailFade->setEasingCurve(QEasingCurve::OutCubic);
    connect(detailFade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        if (m_detailOpacityEffect) {
            m_detailOpacityEffect->setOpacity(value.toReal());
        }
    });
    group->addAnimation(detailFade);

    connect(group, &QParallelAnimationGroup::finished, this, [this, group]() {
        m_transitionAnimation = nullptr;
        m_transitionPhase = TransitionPhase::Idle;
        if (m_detailView) {
            m_detailView->setImageVisible(true);
        }
        if (m_detailOpacityEffect) {
            m_detailOpacityEffect->setOpacity(1.0);
        }
        removeTransitionImage();
        updateLayerOrder();
        group->deleteLater();
    });
    group->start();

    m_openDetailRequestId = PostRepository::instance().requestPostDetailAsync({summary.postId});
}

void PostApplication::onPostDetailReady(const QString& requestId, const PostDetailData& detail)
{
    if (!m_detailView || requestId != m_openDetailRequestId || detail.postId != m_openPostId) {
        return;
    }

    m_detailView->setPostData(detail);
    m_openDetailRequestId.clear();
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

void PostApplication::fadeOverlay(qreal startOpacity, qreal endOpacity, bool hideAfter)
{
    if (!m_overlay || !m_overlayFadeAnimation) {
        if (hideAfter && m_overlay) {
            m_overlay->hide();
        }
        return;
    }

    m_overlayFadeAnimation->stop();
    if (m_overlayFadeFinishedConnection) {
        disconnect(m_overlayFadeFinishedConnection);
        m_overlayFadeFinishedConnection = {};
    }

    m_overlay->setOverlayOpacity(startOpacity);
    if (!m_overlay->isVisible()) {
        m_overlay->show();
    }

    m_overlayFadeAnimation->setStartValue(startOpacity);
    m_overlayFadeAnimation->setEndValue(endOpacity);
    if (hideAfter) {
        m_overlayFadeFinishedConnection = connect(m_overlayFadeAnimation, &QVariantAnimation::finished, this, [this]() {
            if (m_overlay) {
                m_overlay->hide();
                m_overlay->setOverlayOpacity(0.0);
            }
        });
    }
    m_overlayFadeAnimation->start();
}

QRect PostApplication::detailRectForCurrentPost() const
{
    if (!m_detailView) {
        return {};
    }

    const int horizontalMargin = 40;
    const int verticalMargin = 40;
    const QSize available(qMax(0, width() - horizontalMargin * 2),
                          qMax(0, height() - verticalMargin * 2));
    const QSize preferred = m_detailView->preferredSize(available);
    return QRect((width() - preferred.width()) / 2,
                 (height() - preferred.height()) / 2,
                 preferred.width(),
                 preferred.height());
}

void PostApplication::updateLayerOrder()
{
    if (!m_stack || !m_bar) {
        return;
    }

    const bool modalActive = (m_overlay && m_overlay->isVisible()) || m_detailView || m_transitionImage
            || m_transitionPhase != TransitionPhase::Idle;

    if (!modalActive) {
        m_stack->lower();
        m_bar->raise();
        return;
    }

    m_stack->lower();
    m_bar->raise();
    if (m_overlay) {
        if (!m_overlay->isVisible()) {
            m_overlay->show();
        }
        m_overlay->raise();
        m_bar->stackUnder(m_overlay);
    }
    if (m_detailView) {
        m_detailView->raise();
    }
    if (m_transitionImage) {
        m_transitionImage->raise();
    }
}

void PostApplication::removeTransitionImage()
{
    if (!m_transitionImage) {
        return;
    }

    m_transitionImage->deleteLater();
    m_transitionImage = nullptr;
}

void PostApplication::stopActiveTransition()
{
    m_openDetailRequestId.clear();

    if (m_overlayFadeAnimation) {
        m_overlayFadeAnimation->stop();
    }
    if (m_overlayFadeFinishedConnection) {
        disconnect(m_overlayFadeFinishedConnection);
        m_overlayFadeFinishedConnection = {};
    }

    if (!m_transitionAnimation) {
        m_transitionPhase = TransitionPhase::Idle;
        return;
    }

    QParallelAnimationGroup* animation = m_transitionAnimation;
    m_transitionAnimation = nullptr;
    m_transitionPhase = TransitionPhase::Idle;
    animation->stop();
    animation->deleteLater();
}

void PostApplication::clearDetailView()
{
    if (!m_detailView) {
        m_detailOpacityEffect = nullptr;
        return;
    }

    m_detailView->deleteLater();
    m_detailView = nullptr;
    m_detailOpacityEffect = nullptr;
    updateLayerOrder();
}

void PostApplication::startCloseAnimation()
{
    if (!m_detailView) {
        return;
    }

    const QRect startImageGeometry = m_transitionImage
            ? m_transitionImage->geometry()
            : QRect(m_detailView->geometry().topLeft() + m_detailView->paintedImageRect().topLeft(),
                    m_detailView->paintedImageRect().size());
    const qreal startRevealProgress = transitionImageWidget(m_transitionImage)
            ? transitionImageWidget(m_transitionImage)->revealProgress()
            : 1.0;
    QPixmap transitionPixmap = m_detailView->transitionPixmap();
    if (transitionPixmap.isNull() && transitionImageWidget(m_transitionImage)) {
        transitionPixmap = transitionImageWidget(m_transitionImage)->pixmap();
    }
    const qreal startDetailOpacity = m_detailOpacityEffect ? m_detailOpacityEffect->opacity() : 1.0;
    const qreal startOverlayOpacity = m_overlay ? m_overlay->overlayOpacity() : 1.0;

    stopActiveTransition();
    m_detailView->setImageVisible(false);

    if (!transitionImageWidget(m_transitionImage)) {
        auto* transitionImage = new TransitionImageWidget(this);
        transitionImage->setPixmap(transitionPixmap);
        transitionImage->setRevealProgress(startRevealProgress);
        m_transitionImage = transitionImage;
    } else {
        transitionImageWidget(m_transitionImage)->setPixmap(transitionPixmap);
        transitionImageWidget(m_transitionImage)->setRevealProgress(startRevealProgress);
    }
    m_transitionImage->setGeometry(startImageGeometry);
    m_transitionImage->show();
    updateLayerOrder();

    auto* group = new QParallelAnimationGroup(this);
    m_transitionAnimation = group;
    m_transitionPhase = TransitionPhase::Closing;

    auto* transitionAnim = new QPropertyAnimation(m_transitionImage, "geometry", group);
    transitionAnim->setDuration(240);
    transitionAnim->setStartValue(startImageGeometry);
    transitionAnim->setEndValue(m_openSourceGeometry);
    transitionAnim->setEasingCurve(QEasingCurve::InOutQuad);
    group->addAnimation(transitionAnim);

    auto* revealAnim = new QVariantAnimation(group);
    revealAnim->setDuration(240);
    revealAnim->setStartValue(startRevealProgress);
    revealAnim->setEndValue(0.0);
    revealAnim->setEasingCurve(QEasingCurve::InOutQuad);
    connect(revealAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        if (auto* transitionWidget = transitionImageWidget(m_transitionImage)) {
            transitionWidget->setRevealProgress(value.toReal());
        }
    });
    group->addAnimation(revealAnim);

    if (m_detailOpacityEffect) {
        auto* detailFade = new QVariantAnimation(group);
        detailFade->setDuration(200);
        detailFade->setStartValue(startDetailOpacity);
        detailFade->setEndValue(0.0);
        detailFade->setEasingCurve(QEasingCurve::InOutQuad);
        connect(detailFade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            if (m_detailOpacityEffect) {
                m_detailOpacityEffect->setOpacity(value.toReal());
            }
        });
        group->addAnimation(detailFade);
    }

    if (m_overlay) {
        auto* overlayAnim = new QVariantAnimation(group);
        overlayAnim->setDuration(220);
        overlayAnim->setStartValue(startOverlayOpacity);
        overlayAnim->setEndValue(0.0);
        overlayAnim->setEasingCurve(QEasingCurve::InOutQuad);
        connect(overlayAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            if (m_overlay) {
                m_overlay->setOverlayOpacity(value.toReal());
            }
        });
        group->addAnimation(overlayAnim);
    }

    connect(group, &QAbstractAnimation::finished, this, [this, group]() {
        m_transitionAnimation = nullptr;
        m_transitionPhase = TransitionPhase::Idle;
        removeTransitionImage();
        clearDetailView();
        m_openPostId.clear();
        if (m_overlay) {
            m_overlay->hide();
            m_overlay->setOverlayOpacity(0.0);
        }
        updateLayerOrder();
        group->deleteLater();
    });

    group->start();
}
