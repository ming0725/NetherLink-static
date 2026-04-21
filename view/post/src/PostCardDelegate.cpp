#include "PostCardDelegate.h"

#include <QApplication>
#include <QPainter>

#include "ImageService.h"
#include "PostFeedModel.h"
#include "PostMasonryView.h"

namespace {

const QColor kImagePlaceholder(0xF2, 0xF2, 0xF2);

} // namespace

PostCardDelegate::PostCardDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void PostCardDelegate::paint(QPainter* painter,
                             const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const CardLayout layout = calculateLayout(index, option.rect);

    const QString coverPath = index.data(PostFeedModel::ThumbnailImageRole).toString();
    if (!coverPath.isEmpty()) {
        const QPixmap cover = ImageService::instance().previewCrop(coverPath,
                                                                   layout.imageRect.size(),
                                                                   12,
                                                                   painter->device()->devicePixelRatioF());
        painter->drawPixmap(layout.imageRect, cover);
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(kImagePlaceholder);
        painter->drawRoundedRect(layout.imageRect, 12, 12);
    }

    qreal hoverOpacity = 0.0;
    if (option.widget) {
        if (auto* masonryView = qobject_cast<PostMasonryView*>(option.widget->parentWidget())) {
            hoverOpacity = masonryView->hoverOverlayOpacityForIndex(index);
        }
    }

    if (hoverOpacity > 0.0) {
        QLinearGradient overlayGradient(layout.imageRect.topLeft(), layout.imageRect.bottomLeft());
        overlayGradient.setColorAt(0.0, QColor(48, 48, 48, qRound(hoverOpacity * 70)));
        overlayGradient.setColorAt(1.0, QColor(32, 32, 32, qRound(hoverOpacity * 132)));
        painter->setPen(Qt::NoPen);
        painter->setBrush(overlayGradient);
        painter->drawRoundedRect(layout.imageRect, 12, 12);
    }

    QFont titleFont = option.font;
    titleFont.setPointSize(kTitleFontSize);
    painter->setFont(titleFont);
    painter->setPen(Qt::black);
    painter->drawText(layout.titleRect,
                      Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                      index.data(PostFeedModel::TitleRole).toString());

    const QString avatarPath = index.data(PostFeedModel::AuthorAvatarRole).toString();
    const QPixmap avatar = ImageService::instance().circularAvatar(avatarPath,
                                                                   layout.avatarRect.width(),
                                                                   painter->device()->devicePixelRatioF());
    painter->drawPixmap(layout.avatarRect, avatar);

    QFont metaFont = option.font;
    metaFont.setPointSize(kMetaFontSize);
    painter->setFont(metaFont);
    painter->setPen(QColor(0x33, 0x33, 0x33));
    painter->drawText(layout.authorRect,
                      Qt::AlignVCenter | Qt::AlignLeft,
                      QFontMetrics(metaFont).elidedText(index.data(PostFeedModel::AuthorNameRole).toString(),
                                                        Qt::ElideRight,
                                                        layout.authorRect.width()));

    const bool liked = index.data(PostFeedModel::IsLikedRole).toBool();
    const QString likeIconPath = liked
            ? QStringLiteral(":/resources/icon/full_heart.png")
            : QStringLiteral(":/resources/icon/heart.png");
    const QPixmap likeIcon = ImageService::instance().scaled(likeIconPath,
                                                             layout.likeIconRect.size(),
                                                             Qt::KeepAspectRatio,
                                                             painter->device()->devicePixelRatioF());
    painter->drawPixmap(layout.likeIconRect, likeIcon);

    painter->setPen(QColor(0x55, 0x55, 0x55));
    painter->drawText(layout.likeCountRect,
                      Qt::AlignVCenter | Qt::AlignRight,
                      QString::number(index.data(PostFeedModel::LikeCountRole).toInt()));

    painter->restore();
}

QSize PostCardDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(kMinWidth, kMaxImageHeight + 96);
}

int PostCardDelegate::heightForWidth(const QModelIndex& index, int width, qreal) const
{
    return calculateLayout(index, QRect(0, 0, width, 0)).totalHeight;
}

QRect PostCardDelegate::imageRect(const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    return calculateLayout(index, option.rect).imageRect;
}

PostCardDelegate::Action PostCardDelegate::actionAt(const QStyleOptionViewItem& option,
                                                    const QModelIndex& index,
                                                    const QPoint& point) const
{
    const CardLayout layout = calculateLayout(index, option.rect);
    if (layout.imageRect.contains(point)) {
        return OpenPostAction;
    }
    if (layout.likeIconRect.contains(point) || layout.likeCountRect.contains(point)) {
        return ToggleLikeAction;
    }
    if (layout.avatarRect.contains(point) || layout.authorRect.contains(point)) {
        return OpenAuthorAction;
    }
    return NoAction;
}

PostCardDelegate::CardLayout PostCardDelegate::calculateLayout(const QModelIndex& index, const QRect& rect) const
{
    CardLayout layout;
    const int imageHeight = imageHeightForWidth(index, rect.width());
    layout.imageRect = QRect(rect.left(), rect.top(), rect.width(), imageHeight);

    QFont titleFont = QApplication::font();
    titleFont.setPointSize(kTitleFontSize);
    const QFontMetrics titleMetrics(titleFont);
    const int titleMaxHeight = titleMetrics.lineSpacing() * kTitleMaxLines;
    const QRect titleBounds = titleMetrics.boundingRect(0,
                                                        0,
                                                        rect.width() - 2 * kMargin,
                                                        titleMaxHeight,
                                                        Qt::TextWordWrap,
                                                        index.data(PostFeedModel::TitleRole).toString());
    const int titleHeight = qMin(titleBounds.height(), titleMaxHeight);
    const int titleY = imageHeight + kMargin;
    layout.titleRect = QRect(rect.left() + kMargin,
                             rect.top() + titleY,
                             rect.width() - 2 * kMargin,
                             titleHeight);

    const int rowY = rect.top() + titleY + titleHeight + kMargin;
    layout.avatarRect = QRect(rect.left() + kMargin, rowY, kAvatarSize, kAvatarSize);

    QFont metaFont = QApplication::font();
    metaFont.setPointSize(kMetaFontSize);
    const QFontMetrics metaMetrics(metaFont);
    const QString likeText = QString::number(index.data(PostFeedModel::LikeCountRole).toInt());
    const int likeCountWidth = metaMetrics.horizontalAdvance(likeText);
    const int likeIconX = rect.right() - static_cast<int>(1.2 * kMargin) - likeCountWidth - kLikeIconSize;
    layout.likeIconRect = QRect(likeIconX,
                                rowY + (kAvatarSize - kLikeIconSize) / 2,
                                kLikeIconSize,
                                kLikeIconSize);
    layout.likeCountRect = QRect(rect.right() - kMargin - likeCountWidth,
                                 rowY,
                                 likeCountWidth,
                                 kAvatarSize);

    const int authorX = rect.left() + kMargin * 2 + kAvatarSize;
    layout.authorRect = QRect(authorX,
                              rowY,
                              qMax(0, likeIconX - authorX - kMargin),
                              kAvatarSize);
    layout.totalHeight = rowY + kAvatarSize + kMargin - rect.top();
    return layout;
}

int PostCardDelegate::imageHeightForWidth(const QModelIndex& index, int width) const
{
    const QSize sourceSize = index.data(PostFeedModel::ThumbnailSizeRole).toSize();
    if (!sourceSize.isValid() || sourceSize.width() <= 0 || sourceSize.height() <= 0) {
        return kMinWidth;
    }

    const int scaled = qRound(qreal(width) * qreal(sourceSize.height()) / qreal(sourceSize.width()));
    return qMin(scaled, kMaxImageHeight);
}
