// PostApplication.cpp
#include "PostApplication.h"
#include "PostApplicationBar.h"
#include "PostFeedPage.h"
#include "PostDetailView.h"
#include "PostOverlay.h"
#include "features/post/data/PostRepository.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

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

class DetailSnapshotWidget final : public QWidget
{
public:
    explicit DetailSnapshotWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setSnapshot(const QPixmap& pixmap)
    {
        m_pixmap = pixmap;
        update();
    }

    void setVisualOpacity(qreal opacity)
    {
        const qreal bounded = qBound(0.0, opacity, 1.0);
        if (!qFuzzyCompare(m_opacity, bounded)) {
            m_opacity = bounded;
            update();
        }
    }

    qreal visualOpacity() const
    {
        return m_opacity;
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        if (m_pixmap.isNull() || m_opacity <= 0.0) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setOpacity(m_opacity);
        painter.drawPixmap(rect(), m_pixmap);
    }

private:
    QPixmap m_pixmap;
    qreal m_opacity = 1.0;
};

TransitionImageWidget* transitionImageWidget(QWidget* widget)
{
    return dynamic_cast<TransitionImageWidget*>(widget);
}

DetailSnapshotWidget* detailSnapshotWidget(QWidget* widget)
{
    return dynamic_cast<DetailSnapshotWidget*>(widget);
}

QPixmap renderWidgetSnapshot(QWidget* widget)
{
    if (!widget || !widget->size().isValid()) {
        return {};
    }

    const bool wasVisible = widget->isVisible();
    if (!wasVisible) {
        widget->ensurePolished();
        widget->show();
        widget->repaint();
    }

    const QPixmap pixmap = widget->grab();

    if (!wasVisible) {
        widget->hide();
    }

    return pixmap;
}

} // namespace

PostApplication::PostApplication(QWidget* parent)
        : QWidget(parent)
        , m_bar(new PostApplicationBar(this))
        , m_stack(new QStackedWidget(this))
        , m_detailView(nullptr)
{
#ifdef Q_OS_WIN
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::WindowBackground));
    setPalette(palette);
#endif

    m_stack->addWidget(createPlaceholderPage());
    m_stack->setCurrentIndex(0);
    connect(&PostRepository::instance(), &PostRepository::postDetailReady,
            this, &PostApplication::onPostDetailReady);
    connect(&PostRepository::instance(), &PostRepository::postUpdated,
            this, &PostApplication::onPostUpdated);

    m_barFadeAnimation = new QVariantAnimation(this);
    m_barFadeAnimation->setDuration(220);
    m_barFadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_barFadeAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        if (m_bar) {
            m_bar->setVisualOpacity(value.toReal());
        }
    });

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
    m_bar->setVisualOpacity(1.0);
    updateLayerOrder();

    if (!m_initialPageLoadScheduled) {
        m_initialPageLoadScheduled = true;
        QTimer::singleShot(0, this, [this]() {
            ensurePageLoaded(0);
        });
    }
}

void PostApplication::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
}

void PostApplication::setSystemFloatingBarsSuppressed(bool suppressed)
{
    if (!m_bar || !m_bar->usesNativeBar() || m_systemFloatingBarsSuppressed == suppressed) {
        return;
    }

    m_systemFloatingBarsSuppressed = suppressed;
    if (suppressed) {
        m_barVisibleBeforeSystemSuppression = !m_bar->isHidden();
        m_barOpacityBeforeSystemSuppression = m_bar->visualOpacity();
        m_bar->hide();
        return;
    }

    if (m_barVisibleBeforeSystemSuppression && !hasModalLayerActive()) {
        m_bar->setVisualOpacity(m_barOpacityBeforeSystemSuppression);
        m_bar->show();
    }
    m_barVisibleBeforeSystemSuppression = false;
    m_barOpacityBeforeSystemSuppression = 1.0;
    updateLayerOrder();
}

void PostApplication::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    const int w = width();
    const int h = height();
    const int barW = m_bar->width();
    const int barH = m_bar->height();
    const int bottomMargin = 15;
#ifdef Q_OS_MACOS
    const int stackBottomSafeInset = 0;
#else
    const int stackBottomSafeInset = m_bar->usesNativeBar() ? (barH + bottomMargin + 10) : 0;
#endif
    const int stackHeight = qMax(0, h - kContentTopInset - stackBottomSafeInset);

    // macOS keeps the bar floating over the feed; other platforms can still reserve space.
    m_stack->setGeometry(0, kContentTopInset, w, stackHeight);
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
    m_openPostSession.postId = summary.postId;
    m_openPostSession.detailRequestId.clear();
    m_openPostSession.sourceGeometry = QRect(mapFromGlobal(sourceGeometry.topLeft()), sourceGeometry.size());
    m_pendingPostDetail.reset();
    removeTransitionImage();
    clearDetailView();

    m_overlay->setGeometry(rect());
    m_overlay->lower();
    m_overlay->show();
    fadeOverlay(m_overlay ? m_overlay->overlayOpacity() : 0.0, 1.0, false);
    fadeBar(m_bar ? m_bar->visualOpacity() : 1.0, 0.0, true);

    m_detailView = new PostDetailView(this);
    m_detailView->setPreviewSummary(summary);
    m_detailView->setImageVisible(false);
    connect(m_detailView, &PostDetailView::likeClicked, this, [this](bool liked) {
        if (!m_openPostSession.postId.isEmpty()) {
            PostRepository::instance().setPostLiked(m_openPostSession.postId, liked);
        }
    });
    m_detailView->setGeometry(detailRectForCurrentPost());
    m_detailView->show();
    refreshDetailSnapshotFromView(0.0);
    m_detailView->hide();

    const QRect targetImageGeometry(m_detailView->geometry().topLeft() + m_detailView->paintedImageRect().topLeft(),
                                    m_detailView->paintedImageRect().size());

    auto* transitionImage = new TransitionImageWidget(this);
    QPixmap transitionPixmap = ImageService::instance().pixmap(summary.thumbnailImagePath);
    if (transitionPixmap.isNull()) {
        transitionPixmap = ImageService::instance().centerCrop(summary.thumbnailImagePath,
                                                               m_openPostSession.sourceGeometry.size(),
                                                               12,
                                                               devicePixelRatioF());
    }
    transitionImage->setPixmap(transitionPixmap);
    transitionImage->setRevealProgress(0.0);
    m_transitionImage = transitionImage;
    m_transitionImage->setGeometry(m_openPostSession.sourceGeometry);
    m_transitionImage->show();
    updateLayerOrder();

    auto* group = new QParallelAnimationGroup(this);
    m_transitionAnimation = group;
    m_transitionPhase = TransitionPhase::Opening;

    auto* transitionAnim = new QPropertyAnimation(m_transitionImage, "geometry", group);
    transitionAnim->setDuration(260);
    transitionAnim->setStartValue(m_openPostSession.sourceGeometry);
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
        if (auto* snapshot = detailSnapshotWidget(m_detailSnapshot)) {
            snapshot->setVisualOpacity(value.toReal());
        }
    });
    group->addAnimation(detailFade);

    connect(group, &QParallelAnimationGroup::finished, this, [this, group]() {
        m_transitionAnimation = nullptr;
        m_transitionPhase = TransitionPhase::Idle;
        if (m_detailView && m_openPostSession.detailRequestId.isEmpty()) {
            revealDetailViewAfterLoad();
        } else if (m_openPostSession.detailRequestId.isEmpty()) {
            removeTransitionImage();
        }
        updateLayerOrder();
        group->deleteLater();
    });
    group->start();

    m_openPostSession.detailRequestId = PostRepository::instance().requestPostDetailAsync({summary.postId});
}

void PostApplication::onPostDetailReady(const QString& requestId, const PostDetailData& detail)
{
    if (!m_detailView
        || requestId != m_openPostSession.detailRequestId
        || detail.postId != m_openPostSession.postId) {
        return;
    }

    m_pendingPostDetail = detail;
    m_openPostSession.detailRequestId.clear();
    if (m_transitionPhase == TransitionPhase::Opening) {
        return;
    }

    revealDetailViewAfterLoad();
}

void PostApplication::onPostUpdated(const PostSummary& summary)
{
    if (!m_detailView || summary.postId != m_openPostSession.postId) {
        return;
    }

    m_detailView->updatePostSummary(summary);
    if (m_transitionPhase == TransitionPhase::Opening) {
        const qreal opacity = detailSnapshotWidget(m_detailSnapshot)
                ? detailSnapshotWidget(m_detailSnapshot)->visualOpacity()
                : 1.0;
        refreshDetailSnapshotFromView(opacity);
        if (m_detailView) {
            m_detailView->hide();
        }
    }
}

QWidget* PostApplication::createPlaceholderPage() const
{
    auto* page = new QWidget(m_stack);
#ifdef Q_OS_WIN
    page->setAutoFillBackground(true);
    QPalette palette = page->palette();
    palette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::WindowBackground));
    page->setPalette(palette);
#else
    page->setAttribute(Qt::WA_TranslucentBackground);
    page->setAutoFillBackground(false);
#endif
    return page;
}

void PostApplication::replaceStackPage(int index, QWidget* page)
{
    if (!page || index < 0 || index >= m_stack->count()) {
        return;
    }

    QWidget* existing = m_stack->widget(index);
    if (existing == page) {
        return;
    }

    const bool wasCurrent = (m_stack->currentIndex() == index);

    m_stack->removeWidget(existing);
    m_stack->insertWidget(index, page);
    if (wasCurrent) {
        m_stack->setCurrentWidget(page);
    }
    existing->deleteLater();
}

void PostApplication::ensurePageLoaded(int index)
{
    switch (index) {
    case 0:
        if (!m_homeFeedPage) {
            m_homeFeedPage = new PostFeedPage(this);
            connect(m_homeFeedPage, &PostFeedPage::postClickedWithGeometry,
                    this, &PostApplication::onPostClickedWithGeometry);
            m_homeFeedPage->ensureInitialized();
            replaceStackPage(index, m_homeFeedPage);
        }
        break;
    default:
        break;
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

void PostApplication::fadeBar(qreal startOpacity, qreal endOpacity, bool hideAfter)
{
    if (!m_bar || !m_barFadeAnimation) {
        return;
    }

    if (m_systemFloatingBarsSuppressed) {
        m_bar->hide();
        return;
    }

    m_barFadeAnimation->stop();
    if (m_barFadeFinishedConnection) {
        disconnect(m_barFadeFinishedConnection);
        m_barFadeFinishedConnection = {};
    }

    m_bar->setVisualOpacity(startOpacity);
    if (!m_bar->isVisible()) {
        m_bar->show();
        updateLayerOrder();
    }

    m_barFadeAnimation->setStartValue(startOpacity);
    m_barFadeAnimation->setEndValue(endOpacity);
    if (hideAfter) {
        m_barFadeFinishedConnection = connect(m_barFadeAnimation, &QVariantAnimation::finished, this, [this]() {
            if (m_bar) {
                m_bar->hide();
                m_bar->setVisualOpacity(0.0);
                updateLayerOrder();
            }
        });
    }
    m_barFadeAnimation->start();
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

    const bool modalActive = hasModalLayerActive();

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
    if (m_detailSnapshot) {
        m_detailSnapshot->raise();
    }
    if (m_transitionImage) {
        m_transitionImage->raise();
    }
}

bool PostApplication::hasModalLayerActive() const
{
    return (m_overlay && m_overlay->isVisible()) || m_detailView || m_transitionImage
            || m_transitionPhase != TransitionPhase::Idle;
}

void PostApplication::removeTransitionImage()
{
    if (!m_transitionImage) {
        return;
    }

    m_transitionImage->deleteLater();
    m_transitionImage = nullptr;
}

void PostApplication::refreshDetailSnapshotFromView(qreal opacity)
{
    if (!m_detailView) {
        clearDetailSnapshot();
        return;
    }

    const QPixmap detailSnapshot = renderWidgetSnapshot(m_detailView);
    if (detailSnapshot.isNull()) {
        clearDetailSnapshot();
        return;
    }

    auto* snapshot = detailSnapshotWidget(m_detailSnapshot);
    if (!snapshot) {
        snapshot = new DetailSnapshotWidget(this);
        m_detailSnapshot = snapshot;
    }

    snapshot->setGeometry(m_detailView->geometry());
    snapshot->setSnapshot(detailSnapshot);
    snapshot->setVisualOpacity(opacity);
    snapshot->show();
}

void PostApplication::clearDetailSnapshot()
{
    if (!m_detailSnapshot) {
        return;
    }

    m_detailSnapshot->deleteLater();
    m_detailSnapshot = nullptr;
}

void PostApplication::stopActiveTransition()
{
    m_openPostSession.detailRequestId.clear();
    m_pendingPostDetail.reset();

    if (m_detailSnapshotFadeAnimation) {
        m_detailSnapshotFadeAnimation->stop();
        m_detailSnapshotFadeAnimation->deleteLater();
        m_detailSnapshotFadeAnimation = nullptr;
    }

    if (m_overlayFadeAnimation) {
        m_overlayFadeAnimation->stop();
    }
    if (m_overlayFadeFinishedConnection) {
        disconnect(m_overlayFadeFinishedConnection);
        m_overlayFadeFinishedConnection = {};
    }
    if (m_barFadeAnimation) {
        m_barFadeAnimation->stop();
    }
    if (m_barFadeFinishedConnection) {
        disconnect(m_barFadeFinishedConnection);
        m_barFadeFinishedConnection = {};
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
    clearDetailSnapshot();

    if (!m_detailView) {
        return;
    }

    m_detailView->deleteLater();
    m_detailView = nullptr;
    updateLayerOrder();
}

void PostApplication::revealDetailViewAfterLoad()
{
    if (!m_detailView) {
        removeTransitionImage();
        return;
    }

    m_detailView->setImageVisible(true);
    m_detailView->show();

    auto* snapshot = detailSnapshotWidget(m_detailSnapshot);
    if (snapshot) {
        if (m_detailSnapshotFadeAnimation) {
            m_detailSnapshotFadeAnimation->stop();
            m_detailSnapshotFadeAnimation->deleteLater();
            m_detailSnapshotFadeAnimation = nullptr;
        }

        m_detailSnapshotFadeAnimation = new QVariantAnimation(this);
        m_detailSnapshotFadeAnimation->setDuration(120);
        m_detailSnapshotFadeAnimation->setStartValue(snapshot->visualOpacity());
        m_detailSnapshotFadeAnimation->setEndValue(0.0);
        m_detailSnapshotFadeAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_detailSnapshotFadeAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            if (auto* currentSnapshot = detailSnapshotWidget(m_detailSnapshot)) {
                currentSnapshot->setVisualOpacity(value.toReal());
            }
        });
        connect(m_detailSnapshotFadeAnimation, &QVariantAnimation::finished, this, [this]() {
            if (m_detailSnapshotFadeAnimation) {
                m_detailSnapshotFadeAnimation->deleteLater();
                m_detailSnapshotFadeAnimation = nullptr;
            }
            clearDetailSnapshot();
            removeTransitionImage();
            updateLayerOrder();
            applyPendingPostDetail();
        });
        m_detailSnapshotFadeAnimation->start();
        return;
    }

    clearDetailSnapshot();
    QTimer::singleShot(0, this, [this]() {
        removeTransitionImage();
        updateLayerOrder();
        applyPendingPostDetail();
    });
}

void PostApplication::applyPendingPostDetail()
{
    if (!m_detailView || !m_pendingPostDetail.has_value()) {
        return;
    }

    const PostDetailData detail = *m_pendingPostDetail;
    m_pendingPostDetail.reset();
    QTimer::singleShot(0, this, [this, detail]() {
        if (!m_detailView || detail.postId != m_openPostSession.postId) {
            return;
        }
        m_detailView->setPostData(detail);
        updateLayerOrder();
    });
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
    const qreal startOverlayOpacity = m_overlay ? m_overlay->overlayOpacity() : 1.0;

    stopActiveTransition();
    m_detailView->setImageVisible(false);
    refreshDetailSnapshotFromView(1.0);
    m_detailView->hide();
    fadeBar(m_bar ? m_bar->visualOpacity() : 0.0, 1.0, false);

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
    transitionAnim->setEndValue(m_openPostSession.sourceGeometry);
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

    if (detailSnapshotWidget(m_detailSnapshot)) {
        auto* detailFade = new QVariantAnimation(group);
        detailFade->setDuration(200);
        detailFade->setStartValue(1.0);
        detailFade->setEndValue(0.0);
        detailFade->setEasingCurve(QEasingCurve::InOutQuad);
        connect(detailFade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            if (auto* snapshot = detailSnapshotWidget(m_detailSnapshot)) {
                snapshot->setVisualOpacity(value.toReal());
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
        m_openPostSession.clear();
        if (m_overlay) {
            m_overlay->hide();
            m_overlay->setOverlayOpacity(0.0);
        }
        updateLayerOrder();
        group->deleteLater();
    });

    group->start();
}
