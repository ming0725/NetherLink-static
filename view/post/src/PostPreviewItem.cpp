// PostPreviewItem.cpp
#include "PostPreviewItem.h"
#include "ClickableLabel.h"
#include <QResizeEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include "PostRepository.h"
#include "UserRepository.h"

PostPreviewItem::PostPreviewItem(const Post& post,
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
    m_authorName = UserRepository::instance().getName(m_post.authorID);

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
    setupUI(post.picturesPath[0]);
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
    QPixmap origPostImage(firstImagePath);
    m_originalImage = origPostImage;
    int w0 = MinWidth;
    double ratio = double(origPostImage.width()) / double(origPostImage.height());
    int h0 = int(w0 / ratio);
    QPixmap tmp = origPostImage.scaled(w0,
                                       h0,
                                       Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
    if (h0 > MaxImgH) {
        // 中心裁掉上下
        int yoff = (h0 - MaxImgH) / 2;
        m_croppedPostImage = tmp.copy(0, yoff, w0, MaxImgH);
    } else {
        m_croppedPostImage = tmp;
    }

    m_imageLabel->setRoundedPixmap(m_croppedPostImage, 12);
    // 头像
    QPixmap pix = UserRepository::instance().getAvatar(m_post.authorID)
            .scaled(AvatarR,
                                     AvatarR,
                                     Qt::KeepAspectRatioByExpanding,
                                     Qt::SmoothTransformation);
    QPixmap circularPixmap(AvatarR, AvatarR);
    circularPixmap.fill(Qt::transparent);
    QPainter p(&circularPixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, AvatarR, AvatarR);
    p.setClipPath(path);
    p.drawPixmap(0, 0, pix);
    m_avatarLabel->setPixmap(circularPixmap);
    m_avatarLabel->setFixedSize(AvatarR, AvatarR);
    // 文本
    m_authorLabel->setText(m_authorName);
    m_likeCountLabel->setText(QString::number(m_post.likes));
    m_titleLabel->setText(m_post.title);
}

void PostPreviewItem::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    if (m_croppedPostImage.isNull()) {
        return;
    }
    int W = width();
    // 1) 图片等比例放大到全宽
    double r = double(m_croppedPostImage.width()) / double(m_croppedPostImage.height());
    int imgH = int(W / r);
    m_imageLabel->setGeometry(0, 0, W, imgH);
    m_imageLabel->setRoundedPixmap(m_croppedPostImage.scaled(W, imgH,
                                                      Qt::KeepAspectRatio,
                                                      Qt::SmoothTransformation), 12);

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
    emit viewPostWithGeometry(m_post, globalGeometry, m_originalImage);
}

void PostPreviewItem::onViewAuthor() {
    emit viewAuthor();
}

void PostPreviewItem::onClickLike() {
    // 点赞图标
    QPixmap fullHeart, emptyHeart;
    if (!QPixmapCache::find("full_heart", &fullHeart)) {
        fullHeart = QPixmap(":/resources/icon/full_heart.png")
                .scaled(20,
                        20,
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
        QPixmapCache::insert("full_heart", fullHeart);
    }
    if (!QPixmapCache::find("empty_heart", &emptyHeart)) {
        emptyHeart = QPixmap(":/resources/icon/heart.png")
                .scaled(20,
                        20,
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
        QPixmapCache::insert("empty_heart", emptyHeart);
    }
    m_likeIconLabel->setRadius(-2);
    if (m_post.isLiked) {
        m_likeIconLabel->setPixmap(emptyHeart);
        m_post.likes--;
        m_likeCountLabel->setText(QString::number(m_post.likes));
        m_post.isLiked = false;
    }
    else {
        m_likeIconLabel->setPixmap(fullHeart);
        m_post.likes++;
        m_likeCountLabel->setText(QString::number(m_post.likes));
        m_post.isLiked = true;
    }
    resizeEvent(nullptr);
}
