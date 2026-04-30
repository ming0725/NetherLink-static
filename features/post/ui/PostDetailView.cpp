// PostDetailView.cpp
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "shared/services/ImageService.h"
#include "PostDetailView.h"

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
            painter.setBrush(m_pressed ? QColor(0, 0, 0, 28) : QColor(0, 0, 0, 14));
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

} // namespace

PostDetailScrollArea::PostDetailScrollArea(QWidget* parent)
    : OverlayScrollArea(parent)
{
    getContentWidget()->setObjectName("PostDetailContentWidget");
    useVerticalContentLayout(QMargins(20, 20, 20, 20), 20);
    setViewportBackgroundColor(Qt::white);
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);
}

void PostDetailScrollArea::setLabels(QLabel* titleLabel, QLabel* contentLabel)
{
    m_titleLabel = titleLabel;
    m_contentLabel = contentLabel;
    if (auto* layout = qobject_cast<QVBoxLayout*>(contentLayout())) {
        if (m_titleLabel && layout->indexOf(m_titleLabel) < 0) {
            layout->addWidget(m_titleLabel);
        }
        if (m_contentLabel && layout->indexOf(m_contentLabel) < 0) {
            layout->addWidget(m_contentLabel);
        }
    }
    relayoutContent();
}

void PostDetailScrollArea::relayout()
{
    relayoutContent();
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
    m_panelContainer = new QWidget(this);
    m_panelContainer->setAttribute(Qt::WA_TranslucentBackground);

    m_authorAvatar = new QLabel(m_panelContainer);
    m_authorAvatar->setFixedSize({40, 40});

    m_commentLineEdit = new IconLineEdit(m_panelContainer);
    m_commentLineEdit->setIcon(QStringLiteral(":/resources/icon/selected_message.png"));
    m_commentLineEdit->getLineEdit()->setPlaceholderText("说点什么吧...");

    m_authorName = new QLabel(m_panelContainer);
    QFont nameFont;
    nameFont.setStyleStrategy(QFont::PreferAntialias);
    nameFont.setPointSize(13);
    nameFont.setBold(true);
    m_authorName->setFont(nameFont);

    m_followBtn = new QPushButton("关注", m_panelContainer);
    m_followBtn->setFixedSize(80, 32);
    m_followBtn->setCursor(Qt::PointingHandCursor);

    m_contentArea = new PostDetailScrollArea(m_panelContainer);

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

    m_likeBtn = new IconActionButton(m_panelContainer);
    m_likeBtn->setIcon(QIcon(":/resources/icon/heart.png"));
    m_likeBtn->setIconSize(QSize(18, 18));

    m_likeCount = new QLabel("666", m_panelContainer);

    m_commentBtn = new IconActionButton(m_panelContainer);
    m_commentBtn->setIcon(QIcon(":/resources/icon/selected_message.png"));
    m_commentBtn->setIconSize(QSize(18, 18));

    m_commentCount = new QLabel("0", m_panelContainer);

    connect(m_followBtn, &QPushButton::clicked, this, [this]() {
        m_state.isFollowed = !m_state.isFollowed;
        m_followBtn->setText(m_state.isFollowed ? "已关注" : "关注");
        emit followClicked(m_state.isFollowed);
    });

    connect(m_likeBtn, &QPushButton::clicked, this, [this]() {
        emit likeClicked(!m_state.isLiked);
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
    p.fillPath(outerPath, Qt::white);
    p.setClipPath(outerPath);

    const QRect detailImageRect = imageRect();
    p.fillRect(detailImageRect, kDetailImagePlaceholder);
    const int topH = 60;
    const int bottomH = 60;
    p.setPen(QColor(0xE9, 0xE9, 0xE9));
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
    m_contentArea->setGeometry(0, topH, rightW, h - topH - bottomH);
}

void PostDetailView::setPreviewSummary(const PostSummary& summary)
{
    applySummaryState(summary, true);
}

void PostDetailView::setPostData(const PostDetailData& data)
{
    m_state.postId = data.postId;
    m_state.authorId = data.authorId;
    m_state.authorName = data.authorName;
    m_state.authorAvatarPath = data.authorAvatarPath;
    m_state.title = data.title;
    m_state.content = data.content;
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

    syncUiFromState();

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
        m_state.fullImageSource.clear();
        m_state.fullImageSize = {};
        m_state.fullImageOpacity = 0.0;
    }

    syncUiFromState();
}

void PostDetailView::syncUiFromState()
{
    m_authorName->setText(m_state.authorName);
    m_authorAvatar->setPixmap(ImageService::instance().circularAvatar(m_state.authorAvatarPath, 32));
    m_followBtn->setText(m_state.isFollowed ? "已关注" : "关注");
    m_titleLabel->setText(m_state.title);
    m_contentLabel->setText(m_state.content);
    syncEngagementUi();
    m_contentArea->relayout();
    updateLayout();
    update();
}

void PostDetailView::syncEngagementUi()
{
    m_likeBtn->setIcon(QIcon(m_state.isLiked ? ":/resources/icon/full_heart.png"
                                             : ":/resources/icon/heart.png"));
    m_likeCount->setText(QString::number(m_state.likeCount));
    m_commentCount->setText(QString::number(m_state.commentCount));
}
