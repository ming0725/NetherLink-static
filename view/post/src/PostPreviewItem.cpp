// PostPreviewItem.cpp
#include "PostPreviewItem.h"
#include "ClickableLabel.h"
#include "ImageService.h"
#include <QResizeEvent>
#include <QFontMetrics>

PostPreviewItem::PostPreviewItem(const PostSummary& post,
                                 QWidget* parent)
        : QWidget(parent)
        , m_post(post)
{
    // 子控件
    m_imageLabel      = new ClickableLabel(this);
    m_titleLabel      = new ClickableLabel(this);
    m_avatarLabel     = new ClickableLabel(this);
    m_authorLabel     = new ClickableLabel(this);
    m_likeIconLabel   = new ClickableLabel(this);
    m_likeCountLabel  = new ClickableLabel(this);
    m_authorName = m_post.authorName;

    m_titleLabel->setWordWrap(true);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_authorLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_likeCountLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    QFont font = m_avatarLabel->font();
    font.setPointSize(10);
    m_titleLabel->setFont(font);
    font.setPointSize(9);
    m_likeCountLabel->setFont(font);
    m_authorLabel->setFont(font);
    m_likeIconLabel->setFixedSize(20,20);
    onClickLike();
    // 初始裁切
    setupUI(post.coverImagePath);
    // —— 其它信号槽连接 —— //
    connect(m_imageLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onViewPost);
    connect(m_imageLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onViewPost);
    connect(m_titleLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onViewPost);
    connect(m_authorLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onViewAuthor);
    connect(m_avatarLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onViewAuthor);
    connect(m_likeIconLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onClickLike);
    connect(m_likeCountLabel, &ClickableLabel::clicked, this, &PostPreviewItem::onClickLike);
}

void PostPreviewItem::setupUI(const QString& firstImagePath) {
    const QSize sourceSize = ImageService::instance().sourceSize(firstImagePath);
    if (sourceSize.isValid() && sourceSize.height() > 0) {
        imageBaseHeight = int(double(MinWidth) * double(sourceSize.height()) / double(sourceSize.width()));
    } else {
        imageBaseHeight = MinWidth;
    }
    imageBaseHeight = qMin(imageBaseHeight, MaxImgH);

    m_imageLabel->setRoundedPixmap(ImageService::instance().centerCrop(firstImagePath,
                                                                       QSize(MinWidth, imageBaseHeight),
                                                                       12),
                                   12);
    m_avatarLabel->setPixmap(ImageService::instance().circularAvatar(
            m_post.authorAvatarPath,
            AvatarR));
    m_avatarLabel->setFixedSize(AvatarR, AvatarR);
    // 文本
    m_authorLabel->setText(m_authorName);
    m_likeCountLabel->setText(QString::number(m_post.likeCount));
    m_titleLabel->setText(m_post.title);
}

void PostPreviewItem::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    if (m_post.coverImagePath.isEmpty() || imageBaseHeight <= 0) {
        return;
    }
    int W = width();
    int imgH = qMin(int(double(W) * double(imageBaseHeight) / double(imageBaseWidth)), MaxImgH);
    m_imageLabel->setGeometry(0, 0, W, imgH);
    m_imageLabel->setRoundedPixmap(ImageService::instance().centerCrop(m_post.coverImagePath,
                                                                       QSize(W, imgH),
                                                                       12),
                                   12);

    // 2) 计算标题占用高度 (1~2行)
    QFontMetrics fm(m_titleLabel->font());
    int lineH = fm.lineSpacing();
    int maxTitleH = lineH * 2;
    QRect rText = fm.boundingRect(0, 0, W - 2*Margin,
                                  maxTitleH,
                                  Qt::TextWordWrap,
                                  m_post.title);
    int titleH = qMin(rText.height(), maxTitleH);

    m_titleLabel->setGeometry(Margin,
                              imgH + Margin,
                              W - 2*Margin,
                              titleH);

    // 3) 底部行：头像 / 作者名 / 点赞图标 / 点赞数
    int y0 = imgH + Margin + titleH + Margin;
    // 头像
    m_avatarLabel->setGeometry(Margin, y0, AvatarR, AvatarR);

    // 点赞图标和数
    int iconW = m_likeIconLabel->width();
    int countW = fm.horizontalAdvance(m_likeCountLabel->text());
    int likeIconX = W - 1.2 * Margin - countW - iconW;
    m_likeIconLabel->setGeometry(likeIconX,
                                 y0 + (AvatarR-iconW)/2,
                                 iconW,
                                 iconW);
    m_likeCountLabel->setGeometry(W - Margin - countW,
                                  y0,
                                  countW,
                                  AvatarR);
    // 作者
    int authorX = AvatarR + Margin + Margin;
    m_authorLabel->setGeometry(authorX,
                               y0,
                               likeIconX - authorX - Margin, // 留点赞部分宽度
                               AvatarR);
    int totalH = y0 + AvatarR + Margin;
    setFixedHeight(totalH);
}

int PostPreviewItem::scaledHeightFor(double itemW) {
    QSize oldSize = size();
    resize(itemW, oldSize.height());
    int newH = height();
    resize(oldSize);
    return newH;
}


void PostPreviewItem::onViewPost() {
    QRect globalGeometry = QRect(mapToGlobal(QPoint(0, 0)), size());
    emit viewPostWithGeometry(m_post.postId, globalGeometry);
}

void PostPreviewItem::onViewAuthor() {
    emit viewAuthor();
}

void PostPreviewItem::onClickLike() {
    // 点赞图标
    const QPixmap fullHeart = ImageService::instance().scaled(":/resources/icon/full_heart.png",
                                                              QSize(20, 20));
    const QPixmap emptyHeart = ImageService::instance().scaled(":/resources/icon/heart.png",
                                                               QSize(20, 20));
    m_likeIconLabel->setRadius(-2);
    if (m_post.isLiked) {
        m_likeIconLabel->setPixmap(emptyHeart);
        m_post.likeCount--;
        m_likeCountLabel->setText(QString::number(m_post.likeCount));
        m_post.isLiked = false;
    }
    else {
        m_likeIconLabel->setPixmap(fullHeart);
        m_post.likeCount++;
        m_likeCountLabel->setText(QString::number(m_post.likeCount));
        m_post.isLiked = true;
    }
    resizeEvent(nullptr);
}
