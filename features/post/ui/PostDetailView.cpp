// PostDetailView.cpp
#include <QDate>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>

#include "features/post/model/PostDetailListModel.h"
#include "features/post/ui/PostSessionController.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/StatefulPushButton.h"
#include "PostCommentDelegate.h"
#include "PostDetailListView.h"
#include "PostDetailView.h"

namespace {

constexpr int kPreferredSidePanelWidth = 380;
constexpr int kMinSidePanelWidth = 340;
constexpr int kMinImageWidth = 220;
constexpr int kFallbackImageWidth = 3;
constexpr int kFallbackImageHeight = 4;

QSize normalizedImageSize(const QSize& size)
{
    if (size.isValid() && size.width() > 0 && size.height() > 0) {
        return size;
    }
    return QSize(kFallbackImageWidth, kFallbackImageHeight);
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
        const QPixmap iconPixmap = icon().pixmap(targetIconSize);
        if (!iconPixmap.isNull()) {
            const QRect target((width() - targetIconSize.width()) / 2,
                               (height() - targetIconSize.height()) / 2,
                               targetIconSize.width(),
                               targetIconSize.height());
            painter.drawPixmap(target, iconPixmap);
        }
    }

private:
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
{
    setupUI();
    setAttribute(Qt::WA_TranslucentBackground);
    disableContextMenu(this);
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
    QFont nameFont;
    nameFont.setStyleStrategy(QFont::PreferAntialias);
    nameFont.setPointSize(13);
    nameFont.setBold(true);
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

    connect(m_detailModel, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex& topLeft,
                         const QModelIndex& bottomRight,
                         const QList<int>& roles) {
        if (m_contentList) {
            const bool needsLayout = roles.isEmpty()
                    || topLeft.row() == 0
                    || roles.contains(PostDetailListModel::PostBodyRevisionRole)
                    || roles.contains(PostDetailListModel::CommentLayoutRole)
                    || roles.contains(PostDetailListModel::CommentEngagementRole);
            if (needsLayout) {
                m_contentList->doItemsLayout();
            }

            const QRect dirty = m_contentList->visualRect(topLeft)
                    .united(m_contentList->visualRect(bottomRight));
            if (dirty.isValid()) {
                m_contentList->viewport()->update(dirty);
            }
            scheduleMaybeLoadMoreComments();
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
    m_likeBtn->setIcon(QIcon(ThemeManager::instance().isDark()
                             ? ":/resources/icon/empty_heart_darkmode.png"
                             : ":/resources/icon/heart.png"));
    m_likeBtn->setIconSize(QSize(18, 18));

    m_likeCount = new QLabel("666", m_panelContainer);
    disableContextMenu(m_likeCount);
    setWidgetTextColor(m_likeCount, ThemeManager::instance().color(ThemeColor::SecondaryText));

    m_commentBtn = new IconActionButton(m_panelContainer);
    disableContextMenu(m_commentBtn);
    m_commentBtn->setIcon(QIcon(":/resources/icon/selected_message.png"));
    m_commentBtn->setIconSize(QSize(18, 18));

    m_commentCount = new QLabel("0", m_panelContainer);
    disableContextMenu(m_commentCount);
    setWidgetTextColor(m_commentCount, ThemeManager::instance().color(ThemeColor::SecondaryText));

    connect(m_followBtn, &QPushButton::clicked, this, [this]() {
        m_state.isFollowed = !m_state.isFollowed;
        m_followBtn->setText(m_state.isFollowed ? "已关注" : "关注");
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
            m_detailModel, &PostDetailListModel::setCommentExpanded);
    connect(m_contentList, &PostDetailListView::replyExpandRequested,
            m_detailModel, &PostDetailListModel::setReplyExpanded);
    connect(m_contentList, &PostDetailListView::moreRepliesRequested,
            m_detailModel, &PostDetailListModel::showMoreReplies);
    connect(m_contentList->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &PostDetailView::maybeLoadMoreComments);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &PostDetailView::applyTheme);
    applyTheme();
}

void PostDetailView::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    updateLayout();
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
    return fittedImageRect(imageRect(),
                           m_state.fullImageSource.isEmpty() ? m_state.previewImageSize : m_state.fullImageSize);
}

void PostDetailView::setImageVisible(bool visible)
{
    if (m_state.imageVisible == visible) {
        return;
    }
    m_state.imageVisible = visible;
    update(imageRect());
}

QPixmap PostDetailView::transitionPixmap() const
{
    const QString source = m_state.fullImageSource.isEmpty() ? m_state.previewImageSource : m_state.fullImageSource;
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
    if (m_state.imageVisible && !m_state.previewImageSource.isEmpty()) {
        const QRect previewRect = fittedImageRect(detailImageRect, m_state.previewImageSize);
        const QPixmap preview = ImageService::instance().scaled(m_state.previewImageSource,
                                                                previewRect.size(),
                                                                Qt::KeepAspectRatio,
                                                                dpr);
        if (!preview.isNull()) {
            p.drawPixmap(previewRect, preview);
        }
    }

    if (m_state.imageVisible && !m_state.fullImageSource.isEmpty()) {
        const QRect fullRect = fittedImageRect(detailImageRect, m_state.fullImageSize);
        const QPixmap fullImage = ImageService::instance().scaled(m_state.fullImageSource,
                                                                  fullRect.size(),
                                                                  Qt::KeepAspectRatio,
                                                                  dpr);
        if (!fullImage.isNull() && m_state.fullImageOpacity > 0.0) {
            p.save();
            p.setOpacity(m_state.fullImageOpacity);
            p.drawPixmap(fullRect, fullImage);
            p.restore();
        }
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
        followButton->setPrimaryStyle();
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
    m_state.likeCount = data.likeCount;
    m_state.commentCount = data.commentCount;

    if (!data.imagePaths.isEmpty()) {
        m_state.fullImageSource = data.imagePaths.first();
        m_state.fullImageSize = ImageService::instance().sourceSize(m_state.fullImageSource);
    } else {
        m_state.fullImageSource.clear();
        m_state.fullImageSize = {};
    }
    m_state.fullImageOpacity = 0.0;

    if (postChanged) {
        clearReplyTarget();
        m_commentsRequestId.clear();
        m_pendingCommentsOffset = -1;
        m_loadingComments = false;
        m_pendingLoadMoreCheck = false;
        m_detailModel->resetForPost(m_state.postId);
    }
    syncUiFromState();
    loadInitialComments();

    auto* imageFade = new QVariantAnimation(this);
    imageFade->setDuration(180);
    imageFade->setStartValue(m_state.fullImageOpacity);
    imageFade->setEndValue(1.0);
    imageFade->setEasingCurve(QEasingCurve::OutCubic);
    connect(imageFade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_state.fullImageOpacity = value.toReal();
        update(imageRect());
    });
    imageFade->start(QAbstractAnimation::DeleteWhenStopped);

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
    m_state.likeCount = summary.likeCount;
    m_state.commentCount = summary.commentCount;

    if (resetDetailContent) {
        m_state.content = QStringLiteral("加载中...");
        m_state.contentCreatedAt = {};
        m_state.fullImageSource.clear();
        m_state.fullImageSize = {};
        m_state.fullImageOpacity = 0.0;
        if (m_detailModel) {
            clearReplyTarget();
            m_commentsRequestId.clear();
            m_pendingCommentsOffset = -1;
            m_loadingComments = false;
            m_pendingLoadMoreCheck = false;
            m_detailModel->resetForPost(m_state.postId);
        }
    }

    syncUiFromState();
}

void PostDetailView::syncUiFromState()
{
    m_authorName->setText(m_state.authorName);
    m_authorAvatar->setPixmap(ImageService::instance().circularAvatar(m_state.authorAvatarPath, 32));
    m_followBtn->setText(m_state.isFollowed ? "已关注" : "关注");
    const QString contentDateText = postDateText(m_state.contentCreatedAt);
    if (m_detailModel) {
        m_detailModel->setPostBody(m_state.title, m_state.content, contentDateText);
    }
    syncEngagementUi();
    updateLayout();
    update();
}

void PostDetailView::syncEngagementUi()
{
    m_likeBtn->setIcon(QIcon(m_state.isLiked ? ":/resources/icon/full_heart.png"
                                             : (ThemeManager::instance().isDark()
                                                ? ":/resources/icon/empty_heart_darkmode.png"
                                                : ":/resources/icon/heart.png")));
    m_likeCount->setText(QString::number(m_state.likeCount));
    m_commentCount->setText(QString::number(m_state.commentCount));
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
