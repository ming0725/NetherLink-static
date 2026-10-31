// PostDetailView.cpp
#include <utility>

#include <QDate>
#include <QFontMetrics>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
#include <QtMath>

#include "features/post/model/PostDetailListModel.h"
#include "features/post/ui/PostSessionController.h"
#include "shared/services/AppFonts.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/ImageViewer.h"
#include "shared/ui/popup/InWindowPopupDialogs.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"
#include "PostCommentDelegate.h"
#include "PostDetailListView.h"
#include "PostDetailView.h"
#include "PostTypography.h"

namespace {

constexpr int kPreferredSidePanelWidth = 380;
constexpr int kMinSidePanelWidth = 340;
constexpr int kMinImageWidth = 220;
constexpr int kFallbackImageWidth = 3;
constexpr int kFallbackImageHeight = 4;
constexpr int kCommentExpandAnimationDurationMs = 420;
constexpr int kLoadingAnimationFrameMs = 16;
constexpr int kLoadingCommentPreviewCount = 3;
constexpr int kImageChromeAnimationDurationMs = 210;
constexpr int kImageSlideAnimationDurationMs = 260;
constexpr int kImageArrowDiameter = 30;
constexpr int kImageArrowInset = 14;
constexpr int kImageArrowSlideDistance = 10;
constexpr int kImageCounterHeight = 24;
constexpr int kImageCounterTopInset = 14;
constexpr int kImageCounterRightInset = 14;
constexpr qreal kImageArrowChevronAngleDegrees = 100.0;
constexpr qreal kImageArrowChevronHalfHeight = 5.0;
const QString kCommentIconSource = QStringLiteral(":/resources/icon/selected_message.png");

QPixmap avatarPlaceholderPixmap(int size, qreal dpr)
{
    const QSize pixelSize(qMax(1, qRound(size * dpr)),
                          qMax(1, qRound(size * dpr)));
    QPixmap pixmap(pixelSize);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    MediaPlaceholderRenderer::drawAvatar(&painter, QRect(0, 0, size, size));
    return pixmap;
}

QString likeIconSource(bool liked)
{
    if (liked) {
        return QStringLiteral(":/resources/icon/full_heart.png");
    }
    return ThemeManager::instance().isDark()
            ? QStringLiteral(":/resources/icon/empty_heart_darkmode.png")
            : QStringLiteral(":/resources/icon/heart.png");
}

QSize normalizedImageSize(const QSize& size)
{
    if (size.isValid() && size.width() > 0 && size.height() > 0) {
        return size;
    }
    return QSize(kFallbackImageWidth, kFallbackImageHeight);
}

qreal chevronHalfWidthForAngle(qreal angleDegrees, qreal halfHeight)
{
    const qreal boundedAngle = qBound(1.0, angleDegrees, 178.0);
    const qreal horizontalSpan = halfHeight / qTan(qDegreesToRadians(boundedAngle / 2.0));
    return horizontalSpan / 2.0;
}

int sidePanelWidthForTotalWidth(int totalWidth)
{
    return qMin(kPreferredSidePanelWidth,
                qMax(kMinSidePanelWidth, totalWidth - kMinImageWidth));
}

class IconActionButton final : public QPushButton
{
public:
    explicit IconActionButton(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
        setFixedSize(32, 32);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFlat(true);
        setAttribute(Qt::WA_Hover);
    }

    void setIconSource(const QString& source)
    {
        if (m_iconSource == source) {
            return;
        }

        m_iconSource = source;
        update();
    }

protected:
    bool event(QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::HoverEnter:
            m_hovered = true;
            update();
            break;
        case QEvent::HoverLeave:
            m_hovered = false;
            m_pressed = false;
            update();
            break;
        case QEvent::MouseButtonPress:
            if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
                m_pressed = true;
                update();
            }
            break;
        case QEvent::MouseButtonRelease:
            m_pressed = false;
            update();
            break;
        default:
            break;
        }
        return QPushButton::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (m_hovered || m_pressed) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_pressed
                             ? ThemeManager::instance().color(ThemeColor::ControlPressed)
                             : ThemeManager::instance().color(ThemeColor::ControlHover));
            painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 8, 8);
        }

        const QSize targetIconSize = iconSize().isValid() ? iconSize() : QSize(18, 18);
        const QPixmap iconPixmap = ImageService::instance().scaled(m_iconSource,
                                                                   targetIconSize,
                                                                   Qt::KeepAspectRatio,
                                                                   painter.device()->devicePixelRatioF());
        if (!iconPixmap.isNull()) {
            const QRect target((width() - targetIconSize.width()) / 2,
                               (height() - targetIconSize.height()) / 2,
                               targetIconSize.width(),
                               targetIconSize.height());
            painter.drawPixmap(target, iconPixmap);
        }
    }

private:
    QString m_iconSource;
    bool m_hovered = false;
    bool m_pressed = false;
};

void setWidgetTextColor(QWidget* widget, const QColor& color)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::WindowText, color);
    palette.setColor(QPalette::Text, color);
    widget->setPalette(palette);
}

void disableContextMenu(QWidget* widget)
{
    if (widget) {
        widget->setContextMenuPolicy(Qt::NoContextMenu);
    }
}

QString postDateText(const QDateTime& time)
{
    if (!time.isValid()) {
        return {};
    }
    const QDate date = time.date();
    const QDate today = QDate::currentDate();
    if (date == today) {
        return time.toString(QStringLiteral("hh:mm"));
    }
    if (date == today.addDays(-1)) {
        return QStringLiteral("昨天 %1").arg(time.toString(QStringLiteral("hh:mm")));
    }
    if (date <= today.addYears(-1)) {
        return time.toString(QStringLiteral("yyyy-MM-dd"));
    }
    return time.toString(QStringLiteral("MM-dd"));
}

} // namespace

PostDetailView::PostDetailView(QWidget* parent)
    : QWidget(parent)
    , m_loadingAnimationTimer(new QTimer(this))
{
    setupUI();
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
    disableContextMenu(this);
    connect(&ImageService::instance(), &ImageService::previewReady, this, [this]() {
        const QPixmap avatar = ImageService::instance().circularAvatar(m_state.authorAvatarPath,
                                                                       32,
                                                                       devicePixelRatioF());
        m_authorAvatar->setPixmap(avatar.isNull()
                                  ? avatarPlaceholderPixmap(32, devicePixelRatioF())
                                  : avatar);
    });
}

void PostDetailView::setController(PostSessionController* controller)
{
    if (m_controller == controller) {
        return;
    }

    if (m_commentsLoadedConnection) {
        disconnect(m_commentsLoadedConnection);
        m_commentsLoadedConnection = {};
    }

    m_controller = controller;
    if (!m_controller) {
        return;
    }

    m_commentsLoadedConnection = connect(m_controller, &PostSessionController::postCommentsLoaded,
                                         this, &PostDetailView::onCommentsReady);
}

QRect PostDetailView::fittedImageRect(const QRect& bounds, const QSize& imageSize) const
{
    const QSize normalized = normalizedImageSize(imageSize);
    const QSize fitted = normalized.scaled(bounds.size(), Qt::KeepAspectRatio);
    return QRect(bounds.x() + (bounds.width() - fitted.width()) / 2,
                 bounds.y() + (bounds.height() - fitted.height()) / 2,
                 fitted.width(),
                 fitted.height());
}

QString PostDetailView::currentImageSource() const
{
    if (m_state.currentImageIndex >= 0 && m_state.currentImageIndex < m_state.imageSources.size()) {
        return m_state.imageSources.at(m_state.currentImageIndex);
    }
    return {};
}

QSize PostDetailView::currentImageSize() const
{
    if (m_state.currentImageIndex >= 0 && m_state.currentImageIndex < m_state.imageSizes.size()) {
        return m_state.imageSizes.at(m_state.currentImageIndex);
    }
    return {};
}

QString PostDetailView::firstImageSource() const
{
    return m_state.imageSources.isEmpty() ? QString() : m_state.imageSources.first();
}

QSize PostDetailView::firstImageSize() const
{
    return m_state.imageSizes.isEmpty() ? QSize() : m_state.imageSizes.first();
}

int PostDetailView::imageCount() const
{
    return m_state.imageSources.size();
}

QRect PostDetailView::imageCounterRect() const
{
    const QString text = QStringLiteral("%1/%2")
            .arg(qBound(1, m_state.currentImageIndex + 1, qMax(1, imageCount())))
            .arg(qMax(1, imageCount()));
    QFont font = AppFonts::applicationPixelSizedFont(12, true);
    const int textWidth = QFontMetrics(font).horizontalAdvance(text);
    const int counterWidth = qMax(42, textWidth + 20);
    const QRect frame = imageRect();
    return QRect(frame.right() - kImageCounterRightInset - counterWidth + 1,
                 frame.top() + kImageCounterTopInset,
                 counterWidth,
                 kImageCounterHeight);
}

QRect PostDetailView::imageArrowRect(bool previous, qreal progress) const
{
    const QRect frame = imageRect();
    if (!frame.isValid()) {
        return {};
    }

    const qreal boundedProgress = qBound(0.0, progress, 1.0);
    const int finalX = previous
            ? frame.left() + kImageArrowInset
            : frame.right() - kImageArrowInset - kImageArrowDiameter + 1;
    const int hiddenOffset = previous ? -kImageArrowSlideDistance : kImageArrowSlideDistance;
    const int x = qRound(finalX + hiddenOffset * (1.0 - boundedProgress));
    return QRect(x,
                 frame.top() + (frame.height() - kImageArrowDiameter) / 2,
                 kImageArrowDiameter,
                 kImageArrowDiameter);
}

void PostDetailView::setupUI()
{
    m_panelContainer = new QWidget(this);
    m_panelContainer->setAttribute(Qt::WA_TranslucentBackground);
    disableContextMenu(m_panelContainer);

    m_authorAvatar = new QLabel(m_panelContainer);
    m_authorAvatar->setFixedSize({40, 40});
    disableContextMenu(m_authorAvatar);

    m_commentLineEdit = new IconLineEdit(m_panelContainer);
    m_commentLineEdit->setIcon(QStringLiteral(":/resources/icon/selected_message.png"));
    m_commentLineEdit->getLineEdit()->setPlaceholderText("说点什么吧...");
    disableContextMenu(m_commentLineEdit);
    disableContextMenu(m_commentLineEdit->getLineEdit());
    connect(m_commentLineEdit, &QLineEdit::returnPressed,
            this, &PostDetailView::submitCommentText);

    m_authorName = new QLabel(m_panelContainer);
    disableContextMenu(m_authorName);
    QFont nameFont = AppFonts::applicationPixelSizedFont(PostTypography::kDetailAuthorNameFontPx, true);
    m_authorName->setFont(nameFont);
    setWidgetTextColor(m_authorName, ThemeManager::instance().color(ThemeColor::PrimaryText));

    auto* followButton = new StatefulPushButton("关注", m_panelContainer);
    followButton->setRadius(8);
    followButton->setPrimaryStyle();
    m_followBtn = followButton;
    m_followBtn->setFixedSize(80, 32);
    disableContextMenu(m_followBtn);

    m_contentList = new PostDetailListView(m_panelContainer);
    m_detailModel = new PostDetailListModel(this);
    m_commentDelegate = new PostCommentDelegate(this);
    m_contentList->setModel(m_detailModel);
    m_contentList->setPostCommentDelegate(m_commentDelegate);
    m_contentList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_contentList->setSpacing(0);
    m_contentList->setWheelStepPixels(64);
    m_contentList->setScrollBarInsets(8, 4);
    m_contentList->setBackgroundRole(ThemeColor::PanelBackground);
    disableContextMenu(m_contentList);
    disableContextMenu(m_contentList->viewport());
    m_loadingAnimationTimer->setInterval(kLoadingAnimationFrameMs);
    connect(m_loadingAnimationTimer, &QTimer::timeout, this, [this]() {
        if (!m_detailModel || !m_detailModel->hasLoadingPreview()) {
            stopLoadingPreviewAnimation();
            return;
        }
        if (m_contentList && m_contentList->viewport()) {
            m_contentList->viewport()->update();
        }
    });

    connect(m_detailModel, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex& topLeft,
                         const QModelIndex& bottomRight,
                         const QList<int>& roles) {
        if (m_contentList) {
            const bool needsLayout = roles.isEmpty()
                    || topLeft.row() == 0
                    || roles.contains(PostDetailListModel::PostBodyRevisionRole)
                    || roles.contains(PostDetailListModel::CommentLayoutRole);
            if (needsLayout) {
                m_contentList->doItemsLayout();
            }

            const bool shiftsFollowingItems = roles.isEmpty()
                    || topLeft.row() == 0
                    || roles.contains(PostDetailListModel::PostBodyRevisionRole)
                    || roles.contains(PostDetailListModel::CommentLayoutRole);
            if (shiftsFollowingItems) {
                const QRect topRect = m_contentList->visualRect(topLeft);
                const int dirtyTop = topRect.isValid() ? qMax(0, topRect.top()) : 0;
                m_contentList->viewport()->update(QRect(0,
                                                        dirtyTop,
                                                        m_contentList->viewport()->width(),
                                                        m_contentList->viewport()->height() - dirtyTop));
            } else {
                const QRect dirty = m_contentList->visualRect(topLeft)
                        .united(m_contentList->visualRect(bottomRight));
                if (dirty.isValid()) {
                    m_contentList->viewport()->update(dirty);
                }
            }
        }
    });
    connect(m_detailModel, &QAbstractItemModel::rowsInserted, this, [this]() {
        if (m_contentList) {
            m_contentList->doItemsLayout();
            scheduleMaybeLoadMoreComments();
        }
    });

    m_likeBtn = new IconActionButton(m_panelContainer);
    disableContextMenu(m_likeBtn);
    static_cast<IconActionButton*>(m_likeBtn)->setIconSource(likeIconSource(m_state.isLiked));
    m_likeBtn->setIconSize(QSize(18, 18));

    m_likeCount = new QLabel("666", m_panelContainer);
    disableContextMenu(m_likeCount);
    setWidgetTextColor(m_likeCount, ThemeManager::instance().color(ThemeColor::SecondaryText));

    m_commentBtn = new IconActionButton(m_panelContainer);
    disableContextMenu(m_commentBtn);
    static_cast<IconActionButton*>(m_commentBtn)->setIconSource(kCommentIconSource);
    m_commentBtn->setIconSize(QSize(18, 18));

    m_commentCount = new QLabel("0", m_panelContainer);
    disableContextMenu(m_commentCount);
    setWidgetTextColor(m_commentCount, ThemeManager::instance().color(ThemeColor::SecondaryText));

    connect(m_followBtn, &QPushButton::clicked, this, [this]() {
        const bool nextFollowed = !m_state.isFollowed;
        if (!nextFollowed) {
            const InWindowPopup::Button result = InWindowPopup::question(
                    this,
                    QStringLiteral("取消关注"),
                    QStringLiteral("确认不再关注 %1 吗？").arg(m_state.authorName));
            if (result != InWindowPopup::Button::Yes) {
                syncFollowUi();
                return;
            }
        }

        if (m_controller && !m_controller->setAuthorFollowed(m_state.authorId, nextFollowed)) {
            syncFollowUi();
            return;
        }

        m_state.isFollowed = nextFollowed;
        syncFollowUi();
        emit followClicked(m_state.isFollowed);
    });

    connect(m_likeBtn, &QPushButton::clicked, this, [this]() {
        emit likeClicked(!m_state.isLiked);
    });
    connect(m_commentBtn, &QPushButton::clicked, this, [this]() {
        if (!m_replyTarget.commentId.isEmpty()) {
            clearReplyTarget();
        }
        m_commentLineEdit->setFocus();
    });
    connect(m_contentList, &PostDetailListView::commentLikeRequested, this, [this](const QString& commentId, bool liked) {
        if ((m_controller && m_controller->setCommentLiked(commentId, liked))
            || m_detailModel->commentById(commentId)) {
            m_detailModel->updateCommentLike(commentId, liked);
        }
    });
    connect(m_contentList, &PostDetailListView::replyLikeRequested, this, [this](const QString& commentId,
                                                                                 const QString& replyId,
                                                                                 bool liked) {
        if ((m_controller && m_controller->setReplyLiked(replyId, liked))
            || m_detailModel->commentById(commentId)) {
            m_detailModel->updateReplyLike(commentId, replyId, liked);
        }
    });
    connect(m_contentList, &PostDetailListView::replyToCommentRequested,
            this, [this](const QString& commentId) {
                setReplyTarget(commentId);
            });
    connect(m_contentList, &PostDetailListView::replyToReplyRequested,
            this, [this](const QString& commentId, const QString& replyId) {
                setReplyTarget(commentId, replyId);
            });
    connect(m_contentList, &PostDetailListView::commentExpandRequested,
            this, &PostDetailView::animateCommentExpansion);
    connect(m_contentList, &PostDetailListView::replyExpandRequested,
            this, &PostDetailView::animateReplyExpansion);
    connect(m_contentList, &PostDetailListView::moreRepliesRequested,
            this, &PostDetailView::animateMoreReplies);
    connect(m_contentList->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &PostDetailView::maybeLoadMoreComments);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &PostDetailView::applyTheme);
    m_panelContainer->installEventFilter(this);
    m_panelContainer->setMouseTracking(true);
    m_panelContainer->setAttribute(Qt::WA_Hover);
    for (QWidget* child : m_panelContainer->findChildren<QWidget*>()) {
        child->installEventFilter(this);
        child->setMouseTracking(true);
        child->setAttribute(Qt::WA_Hover);
    }
    applyTheme();
}

void PostDetailView::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    updateLayout();
}

bool PostDetailView::eventFilter(QObject* watched, QEvent* event)
{
    const bool fromPanel = watched == m_panelContainer
            || (m_panelContainer && m_panelContainer->isAncestorOf(qobject_cast<QWidget*>(watched)));
    if (fromPanel
        && (event->type() == QEvent::Enter
            || event->type() == QEvent::HoverEnter
            || event->type() == QEvent::MouseMove)) {
        setImageHoverActive(false);
    }

    return QWidget::eventFilter(watched, event);
}

void PostDetailView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && imageCount() > 1) {
        if (imageArrowRect(true).contains(event->pos())) {
            showPreviousImage();
            event->accept();
            return;
        }
        if (imageArrowRect(false).contains(event->pos())) {
            showNextImage();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && paintedImageRect().contains(event->pos())) {
        openPostImageViewer();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void PostDetailView::mouseMoveEvent(QMouseEvent* event)
{
    updateImageHoverState(event->pos());
    QWidget::mouseMoveEvent(event);
}

void PostDetailView::leaveEvent(QEvent* event)
{
    setImageHoverActive(false);
    QWidget::leaveEvent(event);
}

QSize PostDetailView::preferredSize(const QSize& availableBounds) const
{
    return availableBounds;
}

QWidget* PostDetailView::panelWidget() const
{
    return m_panelContainer;
}

QRect PostDetailView::imageRect() const
{
    const int sideWidth = sidePanelWidthForTotalWidth(width());
    return QRect(0, 0, qMax(0, width() - sideWidth), height());
}

QRect PostDetailView::paintedImageRect() const
{
    const QSize size = currentImageSource().isEmpty() ? m_state.previewImageSize : currentImageSize();
    return fittedImageRect(imageRect(), size);
}

QRect PostDetailView::transitionImageRect() const
{
    const QSize size = firstImageSource().isEmpty() ? m_state.previewImageSize : firstImageSize();
    return fittedImageRect(imageRect(), size);
}

void PostDetailView::setImageVisible(bool visible)
{
    if (m_state.imageVisible == visible) {
        return;
    }
    m_state.imageVisible = visible;
    if (!visible) {
        setImageHoverActive(false);
        stopImageSlideAnimation();
        m_state.previousImageIndex = -1;
        m_state.imageSlideDirection = 0;
        m_state.imageSlideProgress = 1.0;
    }
    update(imageRect());
}

QPixmap PostDetailView::transitionPixmap() const
{
    const QString source = firstImageSource().isEmpty() ? m_state.previewImageSource : firstImageSource();
    if (source.isEmpty()) {
        return {};
    }

    return ImageService::instance().pixmap(source);
}

void PostDetailView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    QPainterPath outerPath;
    outerPath.addRoundedRect(rect(), 12, 12);
    p.fillPath(outerPath, ThemeManager::instance().color(ThemeColor::PanelBackground));
    p.setClipPath(outerPath);

    const QRect detailImageRect = imageRect();
    p.fillRect(detailImageRect, ThemeManager::instance().color(ThemeColor::ImagePlaceholder));
    const int topH = 60;
    const int bottomH = 60;
    p.setPen(ThemeManager::instance().color(ThemeColor::Divider));
    p.drawLine(QPoint(detailImageRect.right() + 1, topH - 1), QPoint(width() - 1, topH - 1));
    p.drawLine(QPoint(detailImageRect.right() + 1, height() - 1), QPoint(width() - 1, height() - 1));

    const qreal dpr = p.device()->devicePixelRatioF();
    auto drawImageInViewport = [&](const QRect& targetRect, const QPixmap& image, qreal opacity) {
        if (image.isNull() || opacity <= 0.0) {
            return;
        }

        p.save();
        p.setClipPath(outerPath);
        p.setClipRect(detailImageRect, Qt::IntersectClip);
        p.setOpacity(opacity);
        p.drawPixmap(targetRect, image);
        p.restore();
    };

    auto drawImageAtIndex = [&](int index, int xOffset, qreal opacity) {
        if (index < 0 || index >= imageCount() || opacity <= 0.0) {
            return;
        }

        const QString source = m_state.imageSources.at(index);
        const QSize sourceSize = index < m_state.imageSizes.size() ? m_state.imageSizes.at(index) : QSize();
        if (source.isEmpty()) {
            return;
        }

        const QRect fittedRect = fittedImageRect(detailImageRect, sourceSize).translated(xOffset, 0);
        const QPixmap image = ImageService::instance().scaled(source,
                                                             fittedRect.size(),
                                                             Qt::KeepAspectRatio,
                                                             dpr);
        drawImageInViewport(fittedRect, image, opacity);
    };

    if (m_state.imageVisible) {
        const bool showingFirstImage = m_state.currentImageIndex == 0 || currentImageSource().isEmpty();
        const bool sliding = m_state.previousImageIndex >= 0 && m_state.imageSlideProgress < 1.0;
        if (!sliding && showingFirstImage && !m_state.previewImageSource.isEmpty()) {
            const QRect previewRect = fittedImageRect(detailImageRect, m_state.previewImageSize);
            const QPixmap preview = ImageService::instance().scaled(m_state.previewImageSource,
                                                                    previewRect.size(),
                                                                    Qt::KeepAspectRatio,
                                                                    dpr);
            drawImageInViewport(previewRect, preview, 1.0);
        }

        if (sliding) {
            const int travel = detailImageRect.width();
            const int direction = m_state.imageSlideDirection >= 0 ? 1 : -1;
            const int previousOffset = qRound(-direction * travel * m_state.imageSlideProgress);
            const int currentOffset = qRound(direction * travel * (1.0 - m_state.imageSlideProgress));
            drawImageAtIndex(m_state.previousImageIndex, previousOffset, 1.0);
            drawImageAtIndex(m_state.currentImageIndex, currentOffset, 1.0);
        } else {
            drawImageAtIndex(m_state.currentImageIndex, 0, m_state.fullImageOpacity);
        }
    }

    if (m_state.imageVisible && imageCount() > 1 && m_state.imageHoverProgress > 0.0) {
        const qreal progress = qBound(0.0, m_state.imageHoverProgress, 1.0);
        const int bgAlpha = qRound(128 * progress);
        const int iconAlpha = qRound(235 * progress);

        auto drawArrow = [&](bool previous) {
            const QRect arrowRect = imageArrowRect(previous, progress);
            if (!arrowRect.isValid()) {
                return;
            }

            p.save();
            p.setOpacity(progress);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(20, 20, 20, bgAlpha));
            p.drawEllipse(arrowRect);

            QPen pen(QColor(255, 255, 255, iconAlpha), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            const QPointF center = QRectF(arrowRect).center();
            const qreal halfWidth = chevronHalfWidthForAngle(kImageArrowChevronAngleDegrees,
                                                             kImageArrowChevronHalfHeight);
            const qreal halfHeight = kImageArrowChevronHalfHeight;
            const qreal tipX = center.x() + (previous ? -halfWidth : halfWidth);
            const qreal tailX = center.x() + (previous ? halfWidth : -halfWidth);
            const QPointF tip(tipX, center.y());
            const QPointF top(tailX, center.y() - halfHeight);
            const QPointF bottom(tailX, center.y() + halfHeight);
            p.drawLine(top, tip);
            p.drawLine(tip, bottom);
            p.restore();
        };

        drawArrow(true);
        drawArrow(false);

        const QString counterText = QStringLiteral("%1/%2").arg(m_state.currentImageIndex + 1).arg(imageCount());
        QFont counterFont = AppFonts::applicationPixelSizedFont(12, true);
        p.save();
        p.setOpacity(progress);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 20, 20, bgAlpha));
        const QRect counterRect = imageCounterRect();
        p.drawRoundedRect(counterRect, 8, 8);
        p.setFont(counterFont);
        p.setPen(QColor(255, 255, 255, iconAlpha));
        p.drawText(counterRect, Qt::AlignCenter, counterText);
        p.restore();
    }
}

void PostDetailView::updateLayout()
{
    const QRect detailImageRect = imageRect();
    const int rightX = detailImageRect.right() + 1;
    const int rightW = qMax(0, width() - rightX);
    const int h = height();
    m_panelContainer->setGeometry(rightX, 0, rightW, h);

    const int topH = 60;
    const int avatarX = 20;
    const int avatarY = 10;
    m_authorAvatar->setGeometry(avatarX, avatarY, 40, 40);
    m_authorName->setGeometry(avatarX + 50, avatarY, qMax(120, rightW - 170), 40);
    m_followBtn->setGeometry(rightW - 100, avatarY + 4, 80, 32);

    const int bottomH = 60;
    const int bottomY = h - bottomH;
    const int btnY = bottomY + (bottomH - 32) / 2;

    int x = 20;
    m_commentLineEdit->setGeometry(x, btnY, 160, 32);
    x += 172;
    m_likeBtn->setGeometry(x, btnY, 32, 32);
    x += 40;
    m_likeCount->setGeometry(x - 10, btnY, 50, 32);
    x += 24;
    m_commentBtn->setGeometry(x, btnY, 32, 32);
    x += 40;
    m_commentCount->setGeometry(x - 10, btnY, 50, 32);
    m_contentList->setGeometry(0, topH, rightW, h - topH - bottomH);
    m_contentList->doItemsLayout();
}

void PostDetailView::applyTheme()
{
    setWidgetTextColor(m_authorName, ThemeManager::instance().color(ThemeColor::PrimaryText));
    setWidgetTextColor(m_likeCount, ThemeManager::instance().color(ThemeColor::SecondaryText));
    setWidgetTextColor(m_commentCount, ThemeManager::instance().color(ThemeColor::SecondaryText));
    if (auto* followButton = qobject_cast<StatefulPushButton*>(m_followBtn)) {
        Q_UNUSED(followButton);
        syncFollowUi();
    }
    if (m_commentDelegate) {
        m_commentDelegate->clearCaches();
    }
    m_contentList->setBackgroundRole(ThemeColor::PanelBackground);
    syncEngagementUi();
    m_likeBtn->update();
    m_commentBtn->update();
    m_contentList->viewport()->update();
    update();
}

void PostDetailView::setPreviewSummary(const PostSummary& summary)
{
    applySummaryState(summary, true);
}

void PostDetailView::setPostData(const PostDetailData& data)
{
    const bool postChanged = m_state.postId != data.postId;
    m_state.postId = data.postId;
    m_state.authorId = data.authorId;
    m_state.authorName = data.authorName;
    m_state.authorAvatarPath = data.authorAvatarPath;
    m_state.title = data.title;
    m_state.content = data.content;
    m_state.contentCreatedAt = data.contentCreatedAt;
    m_state.isLiked = data.isLiked;
    m_state.isFollowed = data.isFollowedAuthor;
    m_state.likeCount = data.likeCount;
    m_state.commentCount = data.commentCount;

    m_state.imageSources = data.imagePaths;
    m_state.imageSizes.clear();
    m_state.imageSizes.reserve(m_state.imageSources.size());
    for (const QString& source : std::as_const(m_state.imageSources)) {
        m_state.imageSizes.append(ImageService::instance().sourceSize(source));
    }
    m_state.currentImageIndex = 0;
    m_state.previousImageIndex = -1;
    m_state.imageSlideDirection = 0;
    m_state.imageSlideProgress = 1.0;
    m_state.fullImageOpacity = 0.0;
    m_state.imageHoverActive = false;
    m_state.imageHoverProgress = 0.0;
    if (m_imageHoverAnimation) {
        m_imageHoverAnimation->stop();
        m_imageHoverAnimation->deleteLater();
        m_imageHoverAnimation = nullptr;
    }
    stopImageSlideAnimation();

    const bool loadingPreviewActive = m_detailModel && m_detailModel->hasLoadingPreview();
    if (postChanged || loadingPreviewActive) {
        stopLoadingPreviewAnimation();
        stopCommentAnimations();
        clearReplyTarget();
        m_commentsRequestId.clear();
        m_pendingCommentsOffset = -1;
        m_loadingComments = false;
        m_pendingLoadMoreCheck = false;
        m_detailModel->resetForPost(m_state.postId);
    }
    syncUiFromState();
    loadInitialComments();
    requestPostImageViewerReplacement();
    preloadNextImage();

    stopImageFadeAnimation();
    auto* imageFade = new QVariantAnimation(this);
    m_imageFadeAnimation = imageFade;
    imageFade->setDuration(180);
    imageFade->setStartValue(m_state.fullImageOpacity);
    imageFade->setEndValue(1.0);
    imageFade->setEasingCurve(QEasingCurve::OutCubic);
    connect(imageFade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_state.fullImageOpacity = value.toReal();
        update(imageRect());
    });
    connect(imageFade, &QVariantAnimation::finished, this, [this, imageFade]() {
        if (m_imageFadeAnimation == imageFade) {
            m_imageFadeAnimation = nullptr;
        }
        imageFade->deleteLater();
    });
    imageFade->start();

    update();
}

void PostDetailView::updatePostSummary(const PostSummary& summary)
{
    if (summary.postId != m_state.postId) {
        return;
    }
    applySummaryState(summary, false);
}

void PostDetailView::applySummaryState(const PostSummary& summary, bool resetDetailContent)
{
    m_state.postId = summary.postId;
    m_state.authorId = summary.authorId;
    m_state.authorName = summary.authorName;
    m_state.authorAvatarPath = summary.authorAvatarPath;
    m_state.title = summary.title;
    m_state.previewImageSource = summary.thumbnailImagePath;
    m_state.previewImageSize = summary.thumbnailImageSize;
    m_state.isLiked = summary.isLiked;
    m_state.isFollowed = summary.isFollowedAuthor;
    m_state.likeCount = summary.likeCount;
    m_state.commentCount = summary.commentCount;

    if (resetDetailContent) {
        m_state.content.clear();
        m_state.contentCreatedAt = {};
        m_state.imageSources.clear();
        m_state.imageSizes.clear();
        m_state.currentImageIndex = 0;
        m_state.previousImageIndex = -1;
        m_state.imageSlideDirection = 0;
        m_state.imageSlideProgress = 1.0;
        m_state.fullImageOpacity = 0.0;
        m_state.imageHoverActive = false;
        m_state.imageHoverProgress = 0.0;
        if (m_imageHoverAnimation) {
            m_imageHoverAnimation->stop();
            m_imageHoverAnimation->deleteLater();
            m_imageHoverAnimation = nullptr;
        }
        stopImageSlideAnimation();
        if (m_detailModel) {
            stopCommentAnimations();
            stopImageFadeAnimation();
            clearReplyTarget();
            m_commentsRequestId.clear();
            m_pendingCommentsOffset = -1;
            m_loadingComments = false;
            m_pendingLoadMoreCheck = false;
            showLoadingPreview();
        }
    }

    syncUiFromState();
}

void PostDetailView::syncUiFromState()
{
    m_authorName->setText(m_state.authorName);
    const QPixmap avatar = ImageService::instance().circularAvatar(m_state.authorAvatarPath,
                                                                   32,
                                                                   devicePixelRatioF());
    m_authorAvatar->setPixmap(avatar.isNull()
                              ? avatarPlaceholderPixmap(32, devicePixelRatioF())
                              : avatar);
    syncFollowUi();
    const QString contentDateText = postDateText(m_state.contentCreatedAt);
    if (m_detailModel) {
        if (!m_detailModel->hasLoadingPreview()) {
            m_detailModel->setPostBody(m_state.title, m_state.content, contentDateText);
        }
    }
    syncEngagementUi();
    updateLayout();
    update();
}

void PostDetailView::syncFollowUi()
{
    if (!m_followBtn) {
        return;
    }

    m_followBtn->setText(m_state.isFollowed ? QStringLiteral("已关注")
                                            : QStringLiteral("关注"));
    if (auto* followButton = qobject_cast<StatefulPushButton*>(m_followBtn)) {
        if (m_state.isFollowed) {
            followButton->setDefaultStyle();
        } else {
            followButton->setPrimaryStyle();
        }
    }
}

void PostDetailView::syncEngagementUi()
{
    static_cast<IconActionButton*>(m_likeBtn)->setIconSource(likeIconSource(m_state.isLiked));
    m_likeCount->setText(QString::number(m_state.likeCount));
    m_commentCount->setText(QString::number(m_state.commentCount));
}

void PostDetailView::openPostImageViewer()
{
    if (!m_state.imageVisible) {
        return;
    }

    const QString initialSource = !currentImageSource().isEmpty()
            ? currentImageSource()
            : m_state.previewImageSource;
    if (initialSource.isEmpty()) {
        return;
    }

    auto* viewer = new ImageViewer(initialSource, window());
    m_postImageViewer = viewer;
    m_postImageViewerPostId = m_state.postId;
    requestPostImageViewerReplacement();
    viewer->show();
    viewer->raise();
    viewer->activateWindow();
}

void PostDetailView::requestPostImageViewerReplacement()
{
    const QString source = currentImageSource();
    if (!m_postImageViewer || m_postImageViewerPostId != m_state.postId || source.isEmpty()) {
        return;
    }

    const QPixmap fullPixmap = ImageService::instance().pixmap(source);
    if (!fullPixmap.isNull()) {
        m_postImageViewer->replaceImage(fullPixmap.toImage(), source);
    }
}

void PostDetailView::updateImageHoverState(const QPoint& pos)
{
    setImageHoverActive(m_state.imageVisible
                        && imageCount() > 1
                        && imageRect().contains(pos));
}

void PostDetailView::setImageHoverActive(bool active)
{
    if (m_state.imageHoverActive == active) {
        return;
    }

    m_state.imageHoverActive = active;
    if (m_imageHoverAnimation) {
        m_imageHoverAnimation->stop();
        m_imageHoverAnimation->deleteLater();
        m_imageHoverAnimation = nullptr;
    }

    auto* animation = new QVariantAnimation(this);
    m_imageHoverAnimation = animation;
    animation->setDuration(kImageChromeAnimationDurationMs);
    animation->setStartValue(m_state.imageHoverProgress);
    animation->setEndValue(active ? 1.0 : 0.0);
    animation->setEasingCurve(active ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
    connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_state.imageHoverProgress = value.toReal();
        update(imageRect());
    });
    connect(animation, &QVariantAnimation::finished, this, [this, animation, active]() {
        if (m_imageHoverAnimation == animation) {
            m_imageHoverAnimation = nullptr;
        }
        m_state.imageHoverProgress = active ? 1.0 : 0.0;
        update(imageRect());
        animation->deleteLater();
    });
    animation->start();
}

void PostDetailView::setCurrentImageIndex(int index, int direction)
{
    const int count = imageCount();
    if (count <= 0) {
        return;
    }

    const int boundedIndex = (index % count + count) % count;
    if (m_state.currentImageIndex == boundedIndex) {
        return;
    }

    stopImageFadeAnimation();
    stopImageSlideAnimation();
    m_state.previousImageIndex = m_state.currentImageIndex;
    m_state.currentImageIndex = boundedIndex;
    m_state.imageSlideDirection = direction < 0 ? -1 : 1;
    m_state.imageSlideProgress = 0.0;
    m_state.fullImageOpacity = 1.0;
    preloadNextImage();
    requestPostImageViewerReplacement();

    auto* slideAnimation = new QVariantAnimation(this);
    m_imageSlideAnimation = slideAnimation;
    slideAnimation->setDuration(kImageSlideAnimationDurationMs);
    slideAnimation->setStartValue(0.0);
    slideAnimation->setEndValue(1.0);
    slideAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(slideAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_state.imageSlideProgress = value.toReal();
        update(imageRect());
    });
    connect(slideAnimation, &QVariantAnimation::finished, this, [this, slideAnimation]() {
        if (m_imageSlideAnimation == slideAnimation) {
            m_imageSlideAnimation = nullptr;
        }
        m_state.previousImageIndex = -1;
        m_state.imageSlideDirection = 0;
        m_state.imageSlideProgress = 1.0;
        update(imageRect());
        slideAnimation->deleteLater();
    });
    slideAnimation->start();
}

void PostDetailView::showPreviousImage()
{
    setCurrentImageIndex(m_state.currentImageIndex - 1, -1);
}

void PostDetailView::showNextImage()
{
    setCurrentImageIndex(m_state.currentImageIndex + 1, 1);
}

void PostDetailView::preloadNextImage()
{
    const int count = imageCount();
    if (count <= 1) {
        return;
    }

    const int nextIndex = (m_state.currentImageIndex + 1) % count;
    const QString nextSource = m_state.imageSources.value(nextIndex);
    if (!nextSource.isEmpty()) {
        ImageService::instance().requestOriginalWarmup(nextSource);
    }
}

void PostDetailView::loadInitialComments()
{
    if (!m_controller || !m_detailModel || m_state.postId.isEmpty()) {
        return;
    }

    m_loadingComments = true;
    m_pendingCommentsOffset = 0;
    m_commentsRequestId = m_controller->requestPostComments(m_state.postId, 0, m_commentPageSize);
}

void PostDetailView::loadMoreComments()
{
    if (!m_controller || !m_detailModel || m_loadingComments || m_state.postId.isEmpty() || !m_detailModel->hasMoreComments()) {
        return;
    }

    m_loadingComments = true;
    m_pendingCommentsOffset = m_detailModel->commentCount();
    m_commentsRequestId = m_controller->requestPostComments(m_state.postId, m_pendingCommentsOffset, m_commentPageSize);
}

void PostDetailView::onCommentsReady(const QString& requestId, const PostCommentsPage& page)
{
    if (!m_detailModel
        || requestId != m_commentsRequestId
        || page.postId != m_state.postId
        || page.offset != m_pendingCommentsOffset) {
        return;
    }

    const bool initialPage = page.offset == 0;
    m_commentsRequestId.clear();
    m_pendingCommentsOffset = -1;
    m_loadingComments = false;

    if (initialPage) {
        m_detailModel->setComments(page.comments, page.hasMore);
    } else if (page.offset == m_detailModel->commentCount()) {
        m_detailModel->appendComments(page.comments, page.hasMore);
    }

    scheduleMaybeLoadMoreComments();
}

void PostDetailView::animateCommentExpansion(const QString& commentId)
{
    if (!m_detailModel || commentId.isEmpty() || m_detailModel->isCommentExpanded(commentId)) {
        return;
    }

    if (QPointer<QVariantAnimation> running = m_commentExpansionAnimations.value(commentId)) {
        running->stop();
        running->deleteLater();
    }

    const qreal startProgress = m_detailModel->commentExpansionProgress(commentId);
    m_detailModel->beginCommentExpansion(commentId);

    auto* animation = new QVariantAnimation(this);
    m_commentExpansionAnimations.insert(commentId, animation);
    animation->setStartValue(startProgress);
    animation->setEndValue(1.0);
    animation->setDuration(kCommentExpandAnimationDurationMs);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this, commentId](const QVariant& value) {
        if (!m_detailModel) {
            return;
        }
        m_detailModel->setCommentExpansionProgress(commentId, value.toReal());
    });
    connect(animation, &QVariantAnimation::finished, this, [this, commentId, animation]() {
        if (m_detailModel) {
            m_detailModel->setCommentExpansionProgress(commentId, 1.0);
        }
        m_commentExpansionAnimations.remove(commentId);
        animation->deleteLater();
        scheduleMaybeLoadMoreComments();
    });

    animation->start();
}

void PostDetailView::animateReplyExpansion(const QString& commentId, const QString& replyId)
{
    if (!m_detailModel
        || commentId.isEmpty()
        || replyId.isEmpty()
        || m_detailModel->isReplyExpanded(replyId)) {
        return;
    }

    if (QPointer<QVariantAnimation> running = m_replyExpansionAnimations.value(replyId)) {
        running->stop();
        running->deleteLater();
    }

    const qreal startProgress = m_detailModel->replyExpansionProgress(replyId);
    m_detailModel->beginReplyExpansion(commentId, replyId);

    auto* animation = new QVariantAnimation(this);
    m_replyExpansionAnimations.insert(replyId, animation);
    animation->setStartValue(startProgress);
    animation->setEndValue(1.0);
    animation->setDuration(kCommentExpandAnimationDurationMs);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this, commentId, replyId](const QVariant& value) {
        if (!m_detailModel) {
            return;
        }
        m_detailModel->setReplyExpansionProgress(commentId, replyId, value.toReal());
    });
    connect(animation, &QVariantAnimation::finished, this, [this, commentId, replyId, animation]() {
        if (m_detailModel) {
            m_detailModel->setReplyExpansionProgress(commentId, replyId, 1.0);
        }
        m_replyExpansionAnimations.remove(replyId);
        animation->deleteLater();
        scheduleMaybeLoadMoreComments();
    });

    animation->start();
}

void PostDetailView::animateMoreReplies(const QString& commentId)
{
    if (!m_detailModel || commentId.isEmpty()) {
        return;
    }

    if (m_moreReplyAnimations.value(commentId)) {
        return;
    }

    const QStringList revealedReplyIds = m_detailModel->beginShowMoreReplies(commentId);
    if (revealedReplyIds.isEmpty()) {
        return;
    }

    auto* animation = new QVariantAnimation(this);
    m_moreReplyAnimations.insert(commentId, animation);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setDuration(kCommentExpandAnimationDurationMs);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this, commentId, revealedReplyIds](const QVariant& value) {
        if (!m_detailModel) {
            return;
        }
        m_detailModel->setReplyRevealProgress(commentId, revealedReplyIds, value.toReal());
    });
    connect(animation, &QVariantAnimation::finished, this, [this, commentId, revealedReplyIds, animation]() {
        if (m_detailModel) {
            m_detailModel->setReplyRevealProgress(commentId, revealedReplyIds, 1.0);
        }
        m_moreReplyAnimations.remove(commentId);
        animation->deleteLater();
        scheduleMaybeLoadMoreComments();
    });

    animation->start();
}

void PostDetailView::stopCommentAnimations()
{
    for (const QPointer<QVariantAnimation>& animation : std::as_const(m_commentExpansionAnimations)) {
        if (animation) {
            animation->stop();
            animation->deleteLater();
        }
    }
    for (const QPointer<QVariantAnimation>& animation : std::as_const(m_replyExpansionAnimations)) {
        if (animation) {
            animation->stop();
            animation->deleteLater();
        }
    }
    for (const QPointer<QVariantAnimation>& animation : std::as_const(m_moreReplyAnimations)) {
        if (animation) {
            animation->stop();
            animation->deleteLater();
        }
    }
    m_commentExpansionAnimations.clear();
    m_replyExpansionAnimations.clear();
    m_moreReplyAnimations.clear();
}

void PostDetailView::stopImageFadeAnimation()
{
    if (!m_imageFadeAnimation) {
        return;
    }

    QVariantAnimation* animation = m_imageFadeAnimation;
    m_imageFadeAnimation = nullptr;
    animation->stop();
    animation->deleteLater();
}

void PostDetailView::stopImageSlideAnimation()
{
    if (!m_imageSlideAnimation) {
        return;
    }

    QVariantAnimation* animation = m_imageSlideAnimation;
    m_imageSlideAnimation = nullptr;
    animation->stop();
    animation->deleteLater();
}

void PostDetailView::setReplyTarget(const QString& commentId, const QString& replyId)
{
    if (!m_detailModel || commentId.isEmpty()) {
        return;
    }

    const PostComment* comment = m_detailModel->commentById(commentId);
    if (!comment) {
        return;
    }

    QString targetName = comment->authorName;
    if (!replyId.isEmpty()) {
        for (const PostCommentReply& reply : comment->replies) {
            if (reply.replyId == replyId) {
                targetName = reply.authorName;
                break;
            }
        }
    }

    m_replyTarget.commentId = commentId;
    m_replyTarget.replyId = replyId;
    m_commentLineEdit->setPlaceholderText(QStringLiteral("回复 %1...").arg(targetName));
    m_commentLineEdit->setFocus();
}

void PostDetailView::clearReplyTarget()
{
    m_replyTarget = {};
    m_commentLineEdit->setPlaceholderText(QStringLiteral("说点什么吧..."));
}

void PostDetailView::showLoadingPreview()
{
    if (!m_detailModel) {
        return;
    }

    m_detailModel->showLoadingPreview(m_state.postId, m_state.title, kLoadingCommentPreviewCount);
    if (m_contentList) {
        m_contentList->scrollToTop();
        m_contentList->doItemsLayout();
        if (m_contentList->viewport()) {
            m_contentList->viewport()->update();
        }
    }
    if (!m_loadingAnimationTimer->isActive()) {
        m_loadingAnimationTimer->start();
    }
}

void PostDetailView::stopLoadingPreviewAnimation()
{
    if (m_loadingAnimationTimer->isActive()) {
        m_loadingAnimationTimer->stop();
    }
}

void PostDetailView::submitCommentText()
{
    if (!m_controller || !m_detailModel || m_state.postId.isEmpty()) {
        return;
    }

    const QString text = m_commentLineEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    if (m_replyTarget.commentId.isEmpty()) {
        PostComment comment = m_controller->createLocalComment(m_state.postId, text);
        m_detailModel->insertComment(std::move(comment));
    } else {
        const PostComment* parentComment = m_detailModel->commentById(m_replyTarget.commentId);
        if (!parentComment) {
            clearReplyTarget();
            return;
        }

        QString targetUserId = parentComment->authorId;
        QString targetUserName = parentComment->authorName;
        if (!m_replyTarget.replyId.isEmpty()) {
            for (const PostCommentReply& reply : parentComment->replies) {
                if (reply.replyId == m_replyTarget.replyId) {
                    targetUserId = reply.authorId;
                    targetUserName = reply.authorName;
                    break;
                }
            }
        }

        PostCommentReply reply = m_controller->createLocalReply(m_state.postId,
                                                                m_replyTarget.commentId,
                                                                m_replyTarget.replyId,
                                                                targetUserId,
                                                                targetUserName,
                                                                text);
        m_detailModel->insertReply(m_replyTarget.commentId, m_replyTarget.replyId, std::move(reply));
    }

    ++m_state.commentCount;
    syncEngagementUi();
    if (m_controller) {
        m_controller->adjustCurrentPostCommentCount(1);
    }
    m_commentLineEdit->clear();
    clearReplyTarget();
}

void PostDetailView::maybeLoadMoreComments()
{
    if (!m_contentList || !m_detailModel || !m_detailModel->hasMoreComments() || m_loadingComments) {
        return;
    }

    QScrollBar* scrollBar = m_contentList->verticalScrollBar();
    if (!scrollBar) {
        return;
    }
    if (scrollBar->maximum() <= 0 || scrollBar->maximum() - scrollBar->value() <= 240) {
        loadMoreComments();
    }
}

void PostDetailView::scheduleMaybeLoadMoreComments()
{
    if (m_pendingLoadMoreCheck) {
        return;
    }

    m_pendingLoadMoreCheck = true;
    QTimer::singleShot(0, this, [this]() {
        m_pendingLoadMoreCheck = false;
        maybeLoadMoreComments();
    });
}
