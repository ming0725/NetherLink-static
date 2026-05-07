// PostApplication.h
#pragma once

#include <QWidget>
#include <QMetaObject>
#include <QParallelAnimationGroup>
#include <QStackedWidget>
#include <QVariantAnimation>

#include <optional>

#include "shared/types/RepositoryTypes.h"
class PostFeedPage;
class PostApplicationBar;
class PostDetailView;
class PostOverlay;

class PostApplication : public QWidget {
Q_OBJECT
public:
    explicit PostApplication(QWidget* parent = nullptr);
    void setSystemFloatingBarsSuppressed(bool suppressed);
private slots:
    void onPostClickedWithGeometry(const PostSummary& summary, const QRect& sourceGeometry);
    void onPostDetailReady(const QString& requestId, const PostDetailData& detail);
protected:
    void resizeEvent(QResizeEvent* ev) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;
private:
    enum class TransitionPhase {
        Idle,
        Opening,
        Closing
    };

    struct OpenPostSession {
        QString postId;
        QString detailRequestId;
        QRect sourceGeometry;

        void clear()
        {
            postId.clear();
            detailRequestId.clear();
            sourceGeometry = {};
        }
    };

    void onPostUpdated(const PostSummary& summary);
    bool hasModalLayerActive() const;
    void fadeOverlay(qreal startOpacity, qreal endOpacity, bool hideAfter);
    void fadeBar(qreal startOpacity, qreal endOpacity, bool hideAfter);
    QRect detailRectForCurrentPost() const;
    void updateLayerOrder();
    void removeTransitionImage();
    void stopActiveTransition();
    void refreshDetailSnapshotFromView(qreal opacity);
    void clearDetailSnapshot();
    void clearDetailView();
    void startCloseAnimation();
    void revealDetailViewAfterLoad();
    void applyPendingPostDetail();
    QWidget* createPlaceholderPage() const;
    void replaceStackPage(int index, QWidget* page);
    void ensurePageLoaded(int index);
    PostApplicationBar*   m_bar;
    PostOverlay* m_overlay = nullptr;
    QVariantAnimation* m_overlayFadeAnimation = nullptr;
    QVariantAnimation* m_barFadeAnimation = nullptr;
    QParallelAnimationGroup* m_transitionAnimation = nullptr;
    QMetaObject::Connection m_overlayFadeFinishedConnection;
    QMetaObject::Connection m_barFadeFinishedConnection;
    QStackedWidget*       m_stack;
    PostFeedPage* m_homeFeedPage = nullptr;
    PostDetailView* m_detailView;
    QWidget* m_detailSnapshot = nullptr;
    QVariantAnimation* m_detailSnapshotFadeAnimation = nullptr;
    QWidget* m_transitionImage = nullptr;
    OpenPostSession m_openPostSession;
    std::optional<PostDetailData> m_pendingPostDetail;
    TransitionPhase m_transitionPhase = TransitionPhase::Idle;
    bool m_initialPageLoadScheduled = false;
    bool m_systemFloatingBarsSuppressed = false;
    bool m_barVisibleBeforeSystemSuppression = false;
    qreal m_barOpacityBeforeSystemSuppression = 1.0;
#ifdef Q_OS_MACOS
    static constexpr int kContentTopInset = 20;
#else
    static constexpr int kContentTopInset = 32;
#endif
};
