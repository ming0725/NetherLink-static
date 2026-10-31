#include "PostCardDelegate.h"

#include <QApplication>
#include <QDateTime>
#include <QLinearGradient>
#include <QPainter>

#include "shared/services/ImageService.h"
#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"
#include "features/post/model/PostFeedModel.h"
#include "PostMasonryView.h"
#include "PostTypography.h"

namespace {

QFont titleFont()
{
    return AppFonts::applicationPixelSizedFont(PostTypography::kCardTitleFontPx);
}

QFontMetrics titleMetrics()
{
    return AppFonts::applicationPixelSizedMetrics(PostTypography::kCardTitleFontPx);
}

QFont metaFont()
{
    return AppFonts::applicationPixelSizedFont(PostTypography::kCardMetaFontPx);
}

QFontMetrics metaMetrics()
{
    return AppFonts::applicationPixelSizedMetrics(PostTypography::kCardMetaFontPx);
}

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
    painter->setRenderHints(QPainter::Antialiasing
                            | QPainter::TextAntialiasing
                            | QPainter::SmoothPixmapTransform);
    AppFonts::configurePainterForText(*painter);

    const CardLayout layout = calculateLayout(index, option.rect);
    if (index.data(PostFeedModel::IsLoadingPlaceholderRole).toBool()) {
        drawLoadingPlaceholder(painter,
                               layout,
                               index.data(PostFeedModel::LoadingStartedAtRole).toLongLong());
        painter->restore();
        return;
    }

    const qreal devicePixelRatio = painter->device()->devicePixelRatioF();

    const QString coverPath = index.data(PostFeedModel::ThumbnailImageRole).toString();
    if (!coverPath.isEmpty()) {
        const QPixmap cover = ImageService::instance().previewCrop(coverPath,
                                                                   layout.imageRect.size(),
                                                                   12,
                                                                   devicePixelRatio);
        if (cover.isNull()) {
            MediaPlaceholderRenderer::drawImage(painter, layout.imageRect, 12);
        } else {
            painter->drawPixmap(layout.imageRect, cover);
        }
    } else {
        MediaPlaceholderRenderer::drawImage(painter, layout.imageRect, 12);
    }

    qreal hoverOpacity = 0.0;
    if (option.widget) {
        if (auto* masonryView = qobject_cast<PostMasonryView*>(option.widget->parentWidget())) {
            hoverOpacity = masonryView->hoverOverlayOpacityForIndex(index);
        }
    }

    if (hoverOpacity > 0.0) {
        QLinearGradient overlayGradient(layout.imageRect.topLeft(), layout.imageRect.bottomLeft());
        QColor overlayStart = ThemeManager::instance().color(ThemeColor::MediaOverlayStart);
        overlayStart.setAlpha(qRound(overlayStart.alpha() * hoverOpacity));
        QColor overlayEnd = ThemeManager::instance().color(ThemeColor::MediaOverlayEnd);
        overlayEnd.setAlpha(qRound(overlayEnd.alpha() * hoverOpacity));
        overlayGradient.setColorAt(0.0, overlayStart);
        overlayGradient.setColorAt(1.0, overlayEnd);
        painter->setPen(Qt::NoPen);
        painter->setBrush(overlayGradient);
        painter->drawRoundedRect(layout.imageRect, 12, 12);
    }

    painter->setFont(titleFont());
    painter->setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
    painter->drawText(layout.titleRect,
                      Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                      index.data(PostFeedModel::TitleRole).toString());

    const QString avatarPath = index.data(PostFeedModel::AuthorAvatarRole).toString();
    const QPixmap avatar = ImageService::instance().circularAvatar(avatarPath,
                                                                   layout.avatarRect.width(),
                                                                   devicePixelRatio);
    if (avatar.isNull()) {
        MediaPlaceholderRenderer::drawAvatar(painter, layout.avatarRect);
    } else {
        painter->drawPixmap(layout.avatarRect, avatar);
    }

    painter->setFont(metaFont());
    painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
    painter->drawText(layout.authorRect,
                      Qt::AlignVCenter | Qt::AlignLeft,
                      metaMetrics().elidedText(index.data(PostFeedModel::AuthorNameRole).toString(),
                                               Qt::ElideRight,
                                               layout.authorRect.width()));

    const bool liked = index.data(PostFeedModel::IsLikedRole).toBool();
    const QString likeIconPath = liked
            ? QStringLiteral(":/resources/icon/full_heart.png")
            : (ThemeManager::instance().isDark()
               ? QStringLiteral(":/resources/icon/empty_heart_darkmode.png")
               : QStringLiteral(":/resources/icon/heart.png"));
    const QPixmap likeIcon = ImageService::instance().scaled(likeIconPath,
                                                             layout.likeIconRect.size(),
                                                             Qt::KeepAspectRatio,
                                                             devicePixelRatio);
    painter->drawPixmap(layout.likeIconRect, likeIcon);

    painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
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
    if (index.data(PostFeedModel::IsLoadingPlaceholderRole).toBool()) {
        return NoAction;
    }

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

PostCardDelegate::CardLayout PostCardDelegate::cachedBaseLayout(const QModelIndex& index, int width) const
{
    const QString key = layoutCacheKey(index, width);
    if (const auto it = m_layoutCache.constFind(key); it != m_layoutCache.constEnd()) {
        return it.value();
    }

    CardLayout layout;
    const int imageHeight = imageHeightForWidth(index, width);
    layout.imageRect = QRect(0, 0, width, imageHeight);

    const int titleMaxHeight = titleMetrics().lineSpacing() * kTitleMaxLines;
    int titleHeight = titleMaxHeight;
    if (!index.data(PostFeedModel::IsLoadingPlaceholderRole).toBool()) {
        const QRect titleBounds = titleMetrics().boundingRect(0,
                                                              0,
                                                              width - 2 * kMargin,
                                                              titleMaxHeight,
                                                              Qt::TextWordWrap,
                                                              index.data(PostFeedModel::TitleRole).toString());
        titleHeight = qMin(titleBounds.height(), titleMaxHeight);
    }
    const int titleY = imageHeight + kMargin;
    layout.titleRect = QRect(kMargin,
                             titleY,
                             width - 2 * kMargin,
                             titleHeight);

    const int rowY = titleY + titleHeight + kMargin;
    layout.avatarRect = QRect(kMargin, rowY, kAvatarSize, kAvatarSize);

    const QString likeText = QString::number(index.data(PostFeedModel::LikeCountRole).toInt());
    const int likeCountWidth = metaMetrics().horizontalAdvance(likeText);
    const int likeIconX = width - static_cast<int>(1.2 * kMargin) - likeCountWidth - kLikeIconSize;
    layout.likeIconRect = QRect(likeIconX,
                                rowY + (kAvatarSize - kLikeIconSize) / 2,
                                kLikeIconSize,
                                kLikeIconSize);
    layout.likeCountRect = QRect(width - kMargin - likeCountWidth,
                                 rowY,
                                 likeCountWidth,
                                 kAvatarSize);

    const int authorX = kMargin * 2 + kAvatarSize;
    layout.authorRect = QRect(authorX,
                              rowY,
                              qMax(0, likeIconX - authorX - kMargin),
                              kAvatarSize);
    layout.totalHeight = rowY + kAvatarSize + kMargin;

    m_layoutCache.insert(key, layout);
    return layout;
}

PostCardDelegate::CardLayout PostCardDelegate::calculateLayout(const QModelIndex& index, const QRect& rect) const
{
    CardLayout layout = cachedBaseLayout(index, rect.width());
    const QPoint offset = rect.topLeft();
    layout.imageRect.translate(offset);
    layout.titleRect.translate(offset);
    layout.avatarRect.translate(offset);
    layout.authorRect.translate(offset);
    layout.likeIconRect.translate(offset);
    layout.likeCountRect.translate(offset);
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

QString PostCardDelegate::layoutCacheKey(const QModelIndex& index, int width) const
{
    return QStringLiteral("%1|%2|%3|%4x%5|%6")
            .arg(index.data(PostFeedModel::PostIdRole).toString(),
                 QString::number(width),
                 QString::number(index.data(PostFeedModel::LikeCountRole).toInt()),
                 QString::number(index.data(PostFeedModel::ThumbnailSizeRole).toSize().width()),
                 QString::number(index.data(PostFeedModel::ThumbnailSizeRole).toSize().height()),
                 index.data(PostFeedModel::TitleRole).toString());
}

void PostCardDelegate::drawLoadingPlaceholder(QPainter* painter,
                                              const CardLayout& layout,
                                              qint64 loadingStartedAtMs) const
{
    const bool dark = ThemeManager::instance().isDark();
    QColor base = ThemeManager::instance().color(ThemeColor::LoadingPlaceholderBase);
    QColor highlight = ThemeManager::instance().color(ThemeColor::LoadingPlaceholderHighlight);
    base.setAlpha(235);
    highlight.setAlpha(255);

    constexpr qint64 kShimmerPeriodMs = 1200;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsed = loadingStartedAtMs > 0
            ? qMax<qint64>(0, now - loadingStartedAtMs)
            : now;
    const qreal progress = static_cast<qreal>(elapsed % kShimmerPeriodMs) / kShimmerPeriodMs;
    const qreal center = -0.35 + progress * 1.7;
    auto shimmerBrush = [&](const QRectF& targetRect) {
        QLinearGradient gradient(targetRect.topLeft(), targetRect.topRight());
        gradient.setColorAt(0.0, base);
        gradient.setColorAt(qBound(0.0, center - 0.16, 1.0), base);
        gradient.setColorAt(qBound(0.0, center, 1.0), highlight);
        gradient.setColorAt(qBound(0.0, center + 0.16, 1.0), base);
        gradient.setColorAt(1.0, base);
        return QBrush(gradient);
    };

    QColor lineBase = ThemeManager::instance().color(ThemeColor::LoadingPlaceholderLineBase);
    lineBase.setAlpha(dark ? 210 : 220);
    QColor lineHighlight = ThemeManager::instance().color(ThemeColor::LoadingPlaceholderLineHighlight);
    lineHighlight.setAlpha(245);
    auto lineBrush = [&](const QRectF& targetRect) {
        QLinearGradient gradient(targetRect.topLeft(), targetRect.topRight());
        gradient.setColorAt(0.0, lineBase);
        gradient.setColorAt(qBound(0.0, center - 0.14, 1.0), lineBase);
        gradient.setColorAt(qBound(0.0, center, 1.0), lineHighlight);
        gradient.setColorAt(qBound(0.0, center + 0.14, 1.0), lineBase);
        gradient.setColorAt(1.0, lineBase);
        return QBrush(gradient);
    };

    painter->save();
    painter->setPen(Qt::NoPen);

    painter->setBrush(shimmerBrush(layout.imageRect));
    painter->drawRoundedRect(layout.imageRect, 12, 12);

    const int titleLineHeight = 10;
    const QRect titleLineOne(layout.titleRect.left(),
                             layout.titleRect.top() + 2,
                             qMax(64, layout.titleRect.width() * 4 / 5),
                             titleLineHeight);
    const QRect titleLineTwo(titleLineOne.left(),
                             titleLineOne.bottom() + 7,
                             qMax(48, layout.titleRect.width() * 3 / 5),
                             titleLineHeight);
    painter->setBrush(lineBrush(titleLineOne));
    painter->drawRoundedRect(titleLineOne, 5, 5);
    painter->setBrush(lineBrush(titleLineTwo));
    painter->drawRoundedRect(titleLineTwo, 5, 5);

    painter->setBrush(shimmerBrush(layout.avatarRect));
    painter->drawEllipse(layout.avatarRect);

    const QRect authorLine(layout.authorRect.left(),
                           layout.authorRect.top() + (layout.authorRect.height() - 9) / 2,
                           qMax(46, layout.authorRect.width() * 2 / 5),
                           9);
    painter->setBrush(lineBrush(authorLine));
    painter->drawRoundedRect(authorLine, 4, 4);

    const QRect likeLine(layout.likeIconRect.left(),
                         layout.likeIconRect.top() + (layout.likeIconRect.height() - 9) / 2,
                         qMax(24, layout.likeIconRect.width() + layout.likeCountRect.width()),
                         9);
    painter->setBrush(lineBrush(likeLine));
    painter->drawRoundedRect(likeLine, 4, 4);

    painter->restore();
}
