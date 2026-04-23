// PostDetailView.cpp
#include <QDateTime>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include "app/state/CurrentUser.h"
#include "shared/services/ImageService.h"
#include "PostDetailView.h"
#include "features/friend/data/UserRepository.h"

namespace {

const QColor kDetailImagePlaceholder(0xF2, 0xF2, 0xF2);
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

} // namespace

void PostDetailScrollArea::layoutContent()
{
    if (!contentWidget || !m_titleLabel || !m_contentLabel) {
        return;
    }

    const int margin = 20;
    const int spacing = 20;
    int currentY = margin;

    QFontMetrics titleMetrics(m_titleLabel->font());
    const int titleWidth = viewport()->width() - 2 * margin;
    const int titleHeight = titleMetrics.boundingRect(0, 0, titleWidth, 0,
                                                      Qt::TextWordWrap, m_titleLabel->text()).height();
    m_titleLabel->setGeometry(margin, currentY, titleWidth, titleHeight);
    currentY += titleHeight + spacing;

    QFontMetrics contentMetrics(m_contentLabel->font());
    const int contentWidth = viewport()->width() - 2 * margin;
    const int contentHeight = contentMetrics.boundingRect(0, 0, contentWidth, 0,
                                                          Qt::TextWordWrap, m_contentLabel->text()).height();
    m_contentLabel->setGeometry(margin, currentY, contentWidth, contentHeight);
    currentY += contentHeight + spacing;

    for (QWidget* commentWidget : m_commentWidgets) {
        if (commentWidget && commentWidget->parent() == contentWidget) {
            commentWidget->setGeometry(margin, currentY, viewport()->width() - 2 * margin, 80);
            commentWidget->show();
            currentY += 80 + spacing;
        }
    }

    contentWidget->setFixedSize(viewport()->width(), currentY);
}

void PostDetailScrollArea::addCommentWidget(QWidget* commentWidget)
{
    if (!commentWidget) {
        return;
    }

    commentWidget->setParent(contentWidget);
    m_commentWidgets.append(commentWidget);
    refreshContentLayout();
}

PostDetailScrollArea::PostDetailScrollArea(QWidget* parent)
    : OverlayScrollArea(parent)
{
    contentWidget->setObjectName("PostDetailContentWidget");
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);
}

void PostDetailScrollArea::setLabels(QLabel* titleLabel, QLabel* contentLabel)
{
    m_titleLabel = titleLabel;
    m_contentLabel = contentLabel;
    refreshContentLayout();
}

void PostDetailScrollArea::relayout()
{
    refreshContentLayout();
}

PostDetailView::PostDetailView(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setAttribute(Qt::WA_TranslucentBackground);
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
    m_authorAvatar = new QLabel(this);
    m_authorAvatar->setFixedSize({40, 40});

    commentLineEdit = new IconLineEdit(this);
    commentLineEdit->setIcon(QStringLiteral(":/resources/icon/selected_message.png"));
    commentLineEdit->getLineEdit()->setPlaceholderText("说点什么吧...");

    m_authorName = new QLabel(this);
    QFont nameFont;
    nameFont.setStyleStrategy(QFont::PreferAntialias);
    nameFont.setPointSize(13);
    nameFont.setBold(true);
    m_authorName->setFont(nameFont);

    m_followBtn = new QPushButton("关注", this);
    m_followBtn->setFixedSize(80, 32);
    m_followBtn->setCursor(Qt::PointingHandCursor);

    m_contentArea = new PostDetailScrollArea(this);
    m_contentArea->setObjectName("ContentArea");
    m_contentArea->setStyleSheet("#ContentArea { border-top: 2px solid #f5f5f5; border-bottom: 2px solid #f5f5f5; border-left: none; border-right: none; }");

    m_titleLabel = new QLabel(m_contentArea->getContentWidget());
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont titleFont;
    titleFont.setStyleStrategy(QFont::PreferAntialias);
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    m_contentLabel = new QLabel(m_contentArea->getContentWidget());
    m_contentLabel->setWordWrap(true);
    m_contentLabel->setTextFormat(Qt::PlainText);
    m_contentLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont contentFont;
    contentFont.setStyleStrategy(QFont::PreferAntialias);
    contentFont.setPointSize(13);
    m_contentLabel->setFont(contentFont);

    m_contentArea->setLabels(m_titleLabel, m_contentLabel);

    m_likeBtn = new QPushButton(this);
    m_likeBtn->setFixedSize(32, 32);
    m_likeBtn->setCursor(Qt::PointingHandCursor);
    m_likeBtn->setFlat(true);
    m_likeBtn->setIcon(QIcon(":/resources/icon/heart.png"));
    m_likeBtn->setStyleSheet(QString(
            "QPushButton:pressed {"
            "  background: none;"
            "  border: none;"
            "}"));

    m_likeCount = new QLabel("666", this);

    m_commentBtn = new QPushButton(this);
    m_commentBtn->setFixedSize(32, 32);
    m_commentBtn->setCursor(Qt::PointingHandCursor);
    m_commentBtn->setIcon(QIcon(":/resources/icon/selected_message.png"));
    m_commentBtn->setFlat(true);
    m_commentBtn->setStyleSheet(QString(
            "QPushButton:pressed {"
            "  background: none;"
            "  border: none;"
            "}"));

    m_commentCount = new QLabel("0", this);

    connect(m_followBtn, &QPushButton::clicked, this, [this]() {
        m_isFollowed = !m_isFollowed;
        m_followBtn->setText(m_isFollowed ? "已关注" : "关注");
        emit followClicked(m_isFollowed);
    });

    connect(m_likeBtn, &QPushButton::clicked, this, [this]() {
        m_isLiked = !m_isLiked;
        m_likeBtn->setIcon(QIcon(m_isLiked ? ":/resources/icon/full_heart.png"
                                           : ":/resources/icon/heart.png"));
        m_likes += m_isLiked ? 1 : -1;
        m_likeCount->setText(QString::number(m_likes));
        emit likeClicked(m_isLiked);
    });

    connect(m_commentBtn, &QPushButton::clicked, this, &PostDetailView::commentClicked);
    connect(commentLineEdit->getLineEdit(), &QLineEdit::returnPressed, this, [this]() {
        const QString content = commentLineEdit->getLineEdit()->text().trimmed();
        if (!content.isEmpty()) {
            addComment(content);
            commentLineEdit->getLineEdit()->clear();
        }
    });
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

QRect PostDetailView::imageRect() const
{
    const int sideWidth = sidePanelWidthForTotalWidth(width());
    return QRect(0, 0, qMax(0, width() - sideWidth), height());
}

QRect PostDetailView::paintedImageRect() const
{
    return fittedImageRect(imageRect(),
                           m_fullImageSource.isEmpty() ? m_previewImageSize : m_fullImageSize);
}

void PostDetailView::setImageVisible(bool visible)
{
    if (m_imageVisible == visible) {
        return;
    }
    m_imageVisible = visible;
    update(imageRect());
}

QPixmap PostDetailView::transitionPixmap() const
{
    const QString source = m_fullImageSource.isEmpty() ? m_previewImageSource : m_fullImageSource;
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
    p.fillPath(outerPath, Qt::white);
    p.setClipPath(outerPath);

    const QRect detailImageRect = imageRect();
    p.fillRect(detailImageRect, kDetailImagePlaceholder);

    const qreal dpr = p.device()->devicePixelRatioF();
    if (m_imageVisible && !m_previewImageSource.isEmpty()) {
        const QRect previewRect = fittedImageRect(detailImageRect, m_previewImageSize);
        const QPixmap preview = ImageService::instance().scaled(m_previewImageSource,
                                                                previewRect.size(),
                                                                Qt::KeepAspectRatio,
                                                                dpr);
        if (!preview.isNull()) {
            p.drawPixmap(previewRect, preview);
        }
    }

    if (m_imageVisible && !m_fullImageSource.isEmpty()) {
        const QRect fullRect = fittedImageRect(detailImageRect, m_fullImageSize);
        const QPixmap fullImage = ImageService::instance().scaled(m_fullImageSource,
                                                                  fullRect.size(),
                                                                  Qt::KeepAspectRatio,
                                                                  dpr);
        if (!fullImage.isNull() && m_fullImageOpacity > 0.0) {
            p.save();
            p.setOpacity(m_fullImageOpacity);
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

    const int topH = 60;
    const int avatarX = rightX + 20;
    const int avatarY = 10;
    m_authorAvatar->setGeometry(avatarX, avatarY, 40, 40);
    m_authorName->setGeometry(avatarX + 50, avatarY, qMax(120, rightW - 170), 40);
    m_followBtn->setGeometry(rightX + rightW - 100, avatarY + 4, 80, 32);

    const int bottomH = 60;
    const int bottomY = h - bottomH;
    const int btnY = bottomY + (bottomH - 32) / 2;

    int x = rightX + 10;
    commentLineEdit->setGeometry(x, btnY, 160, 32);
    x += 160;
    m_likeBtn->setGeometry(x, btnY, 32, 32);
    x += 40;
    m_likeCount->setGeometry(x - 10, btnY, 50, 32);
    x += 10;
    m_commentBtn->setGeometry(x, btnY, 32, 32);
    x += 40;
    m_commentCount->setGeometry(x - 10, btnY, 50, 32);
    m_contentArea->setGeometry(rightX, topH, rightW, h - topH - bottomH);
}

void PostDetailView::setPreviewSummary(const PostSummary& summary)
{
    m_postId = summary.postId;
    m_authorId = summary.authorId;
    m_previewImageSource = summary.thumbnailImagePath;
    m_previewImageSize = summary.thumbnailImageSize;
    m_fullImageSource.clear();
    m_fullImageSize = {};
    m_fullImageOpacity = 0.0;

    m_authorName->setText(summary.authorName);
    m_authorAvatar->setPixmap(ImageService::instance().circularAvatar(summary.authorAvatarPath, 32));
    m_titleLabel->setText(summary.title);
    m_contentLabel->setText(QStringLiteral("加载中..."));

    m_isLiked = summary.isLiked;
    m_likes = summary.likeCount;
    m_comments = summary.commentCount;
    m_likeBtn->setIcon(QIcon(m_isLiked ? ":/resources/icon/full_heart.png"
                                       : ":/resources/icon/heart.png"));
    m_likeCount->setText(QString::number(m_likes));
    m_commentCount->setText(QString::number(m_comments));

    m_contentArea->relayout();
    updateLayout();
    update();
}

void PostDetailView::setPostData(const PostDetailData& data)
{
    m_postId = data.postId;
    m_authorId = data.authorId;

    if (!data.imagePaths.isEmpty()) {
        m_fullImageSource = data.imagePaths.first();
        m_fullImageSize = ImageService::instance().sourceSize(m_fullImageSource);
    }

    m_authorName->setText(data.authorName);
    m_authorAvatar->setPixmap(ImageService::instance().circularAvatar(data.authorAvatarPath, 32));
    m_titleLabel->setText(data.title);
    m_contentLabel->setText(data.content);

    m_isLiked = data.isLiked;
    m_likes = data.likeCount;
    m_comments = data.commentCount;
    m_likeBtn->setIcon(QIcon(m_isLiked ? ":/resources/icon/full_heart.png"
                                       : ":/resources/icon/heart.png"));
    m_likeCount->setText(QString::number(m_likes));
    m_commentCount->setText(QString::number(m_comments));

    auto* imageFade = new QVariantAnimation(this);
    imageFade->setDuration(180);
    imageFade->setStartValue(m_fullImageOpacity);
    imageFade->setEndValue(1.0);
    imageFade->setEasingCurve(QEasingCurve::OutCubic);
    connect(imageFade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_fullImageOpacity = value.toReal();
        update(imageRect());
    });
    imageFade->start(QAbstractAnimation::DeleteWhenStopped);

    m_contentArea->relayout();
    updateLayout();
    update();
}

QWidget* PostDetailView::createCommentWidget(const QString& userName, const QString& content)
{
    QWidget* commentWidget = new QWidget(m_contentArea->getContentWidget());
    commentWidget->setObjectName("CommentWidget");
    commentWidget->setFixedHeight(80);
    commentWidget->setStyleSheet("background-color: white;");

    QLabel* avatar = new QLabel(commentWidget);
    avatar->setFixedSize(32, 32);
    avatar->setPixmap(ImageService::instance().circularAvatar(
            UserRepository::instance().requestUserAvatarPath(CurrentUser::instance().getUserId()),
            32));

    QLabel* nameLabel = new QLabel(userName, commentWidget);
    QFont nameFont;
    nameFont.setPointSize(12);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);

    QLabel* contentLabel = new QLabel(content, commentWidget);
    contentLabel->setWordWrap(true);
    QFont contentFont;
    contentFont.setPointSize(12);
    contentLabel->setFont(contentFont);

    QLabel* timeLabel = new QLabel(QDateTime::currentDateTime().toString("hh:mm"), commentWidget);
    QFont timeFont;
    timeFont.setPointSize(10);
    timeLabel->setFont(timeFont);

    avatar->setGeometry(0, 0, 32, 32);
    nameLabel->setGeometry(40, 0, 200, 20);
    contentLabel->setGeometry(40, 20, commentWidget->width() - 50, 40);
    timeLabel->setGeometry(40, 60, 200, 20);

    avatar->show();
    nameLabel->show();
    contentLabel->show();
    timeLabel->show();
    commentWidget->show();

    return commentWidget;
}

void PostDetailView::addComment(const QString& content)
{
    const QString userName = CurrentUser::instance().getUserName();

    QWidget* commentWidget = createCommentWidget(userName, content);
    if (!commentWidget) {
        return;
    }

    m_contentArea->addCommentWidget(commentWidget);
    ++m_comments;
    m_commentCount->setText(QString::number(m_comments));
}
