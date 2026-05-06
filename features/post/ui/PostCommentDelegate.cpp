#include "PostCommentDelegate.h"

#include <algorithm>

#include <QApplication>
#include <QDate>
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>

#include "features/post/model/PostDetailListModel.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"

namespace {

constexpr int kOuterMargin = 16;
constexpr int kCommentTopMargin = 8;
constexpr int kCommentBottomMargin = 8;
constexpr int kCommentAvatarSize = 36;
constexpr int kReplyAvatarSize = 28;
constexpr int kCommentAvatarTextGap = 10;
constexpr int kReplyAvatarTextGap = 8;
constexpr int kNameContentGap = 3;
constexpr int kReplyGap = 14;
constexpr int kMetaActionGap = 8;
constexpr int kLoadMoreReplyTopGap = 6;
constexpr int kLoadMoreReplyHeightExtra = 0;
constexpr int kActionIconSize = 16;
constexpr int kActionGap = 4;
constexpr int kActionGroupGap = 10;
constexpr int kCommentMaxLines = 4;
constexpr int kReplyMaxLines = 4;

QFont fontWithPointSize(int pointSize, bool bold = false)
{
    QFont font = QApplication::font();
    font.setPointSize(pointSize);
    font.setBold(bold);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

const QFont& nameFont()
{
    static const QFont font = fontWithPointSize(13, true);
    return font;
}

const QFont& nameLayoutFont()
{
    static const QFont font = fontWithPointSize(12, true);
    return font;
}

const QFont& metaFont()
{
    static const QFont font = fontWithPointSize(11);
    return font;
}

const QFont& bodyFont()
{
    static const QFont font = fontWithPointSize(13);
    return font;
}

const QFont& actionFont()
{
    static const QFont font = fontWithPointSize(12);
    return font;
}

QString likeIconPath(bool liked)
{
    if (liked) {
        return QStringLiteral(":/resources/icon/full_heart.png");
    }
    return ThemeManager::instance().isDark()
            ? QStringLiteral(":/resources/icon/empty_heart_darkmode.png")
            : QStringLiteral(":/resources/icon/heart.png");
}

QString commentIconPath()
{
    return QStringLiteral(":/resources/icon/selected_message.png");
}

QString timeText(const QDateTime& time)
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

struct ReplyTextParts {
    QString text;
    int targetNameStart = -1;
    int targetNameLength = 0;
};

ReplyTextParts replyDisplayText(const PostCommentReply& reply)
{
    if (reply.targetReplyId.isEmpty()) {
        return {reply.content, -1, 0};
    }
    const QString prefixText = QStringLiteral("回复 ");
    ReplyTextParts parts;
    parts.text = QStringLiteral("%1%2：%3").arg(prefixText, reply.targetUserName, reply.content);
    parts.targetNameStart = prefixText.size();
    parts.targetNameLength = reply.targetUserName.size();
    return parts;
}

int countWidth(const QFontMetrics& metrics, int count)
{
    return qMax(18, metrics.horizontalAdvance(QString::number(count)) + 4);
}

} // namespace

PostCommentDelegate::PostCommentDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void PostCommentDelegate::clearCaches()
{
    m_layoutCache.clear();
    m_textMeasureCache.clear();
}

void PostCommentDelegate::paint(QPainter* painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    if (index.data(PostDetailListModel::ItemTypeRole).toInt() == PostDetailListModel::DetailItem) {
        return;
    }

    const auto* detailModel = qobject_cast<const PostDetailListModel*>(index.model());
    if (!detailModel) {
        return;
    }

    const PostComment* comment = detailModel->commentAtIndex(index);
    if (!comment) {
        return;
    }
    const Layout layout = calculateLayout(option, index);
    const qreal dpr = painter->device()->devicePixelRatioF();

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing
                            | QPainter::TextAntialiasing
                            | QPainter::SmoothPixmapTransform);
    painter->fillRect(option.rect, ThemeManager::instance().color(ThemeColor::PanelBackground));

    painter->drawPixmap(layout.avatarRect,
                        ImageService::instance().circularAvatar(comment->authorAvatarPath,
                                                                layout.avatarRect.width(),
                                                                dpr));

    painter->setFont(nameFont());
    painter->setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
    painter->drawText(layout.nameRect, Qt::AlignLeft | Qt::AlignVCenter, comment->authorName);

    drawMeasuredText(painter, comment->content, layout.contentRect, layout.contentMeasure);

    if (layout.commentCanExpand && !detailModel->isCommentExpanded(comment->commentId)) {
        painter->setFont(actionFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::Accent));
        painter->drawText(layout.expandRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("查看全文"));
    }

    painter->setFont(metaFont());
    painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
    painter->drawText(layout.timeRect, Qt::AlignLeft | Qt::AlignVCenter, timeText(comment->createdAt));

    painter->drawPixmap(layout.commentLikeRect,
                        ImageService::instance().scaled(likeIconPath(comment->isLiked),
                                                        layout.commentLikeRect.size(),
                                                        Qt::KeepAspectRatio,
                                                        dpr));
    painter->setFont(actionFont());
    painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
    painter->drawText(layout.commentLikeCountRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QString::number(comment->likeCount));
    painter->drawPixmap(layout.commentButtonRect,
                        ImageService::instance().scaled(commentIconPath(),
                                                        layout.commentButtonRect.size(),
                                                        Qt::KeepAspectRatio,
                                                        dpr));
    painter->drawText(layout.commentCountRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QString::number(comment->totalReplyCount));

    const int visibleReplies = qMin(detailModel->visibleReplyCount(comment->commentId), comment->replies.size());
    for (int i = 0; i < visibleReplies && i < layout.replyLayouts.size(); ++i) {
        const PostCommentReply& reply = comment->replies.at(i);
        const ReplyLayout& replyLayout = layout.replyLayouts.at(i);
        const ReplyTextParts text = replyDisplayText(reply);

        painter->drawPixmap(replyLayout.avatarRect,
                            ImageService::instance().circularAvatar(reply.authorAvatarPath,
                                                                    replyLayout.avatarRect.width(),
                                                                    dpr));
        painter->setFont(nameFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
        painter->drawText(replyLayout.nameRect, Qt::AlignLeft | Qt::AlignVCenter, reply.authorName);

        drawMeasuredText(painter, text.text, replyLayout.textRect, replyLayout.textMeasure);

        if (!replyLayout.expandRect.isNull() && !detailModel->isReplyExpanded(reply.replyId)) {
            painter->setFont(actionFont());
            painter->setPen(ThemeManager::instance().color(ThemeColor::Accent));
            painter->drawText(replyLayout.expandRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("查看全文"));
        }

        painter->setFont(metaFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
        painter->drawText(replyLayout.timeRect, Qt::AlignLeft | Qt::AlignVCenter, timeText(reply.createdAt));

        painter->drawPixmap(replyLayout.likeRect,
                            ImageService::instance().scaled(likeIconPath(reply.isLiked),
                                                            replyLayout.likeRect.size(),
                                                            Qt::KeepAspectRatio,
                                                            dpr));
        painter->setFont(actionFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(replyLayout.likeCountRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QString::number(reply.likeCount));
        painter->drawPixmap(replyLayout.replyButtonRect,
                            ImageService::instance().scaled(commentIconPath(),
                                                            replyLayout.replyButtonRect.size(),
                                                            Qt::KeepAspectRatio,
                                                            dpr));
        painter->drawText(replyLayout.replyTextRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("回复"));
    }

    if (!layout.loadMoreRepliesRect.isNull()) {
        painter->setFont(actionFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::Accent));
        painter->drawText(layout.loadMoreRepliesRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("加载更多回复"));
    }

    painter->restore();
}

QSize PostCommentDelegate::sizeHint(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    if (index.data(PostDetailListModel::ItemTypeRole).toInt() == PostDetailListModel::DetailItem) {
        return QSize(availableWidth(option), index.data(PostDetailListModel::DetailHeightRole).toInt());
    }

    const Layout layout = calculateLayout(option, index);
    return QSize(availableWidth(option), layout.totalHeight);
}

PostCommentDelegate::HitAction PostCommentDelegate::actionAt(const QStyleOptionViewItem& option,
                                                             const QModelIndex& index,
                                                             const QPoint& point) const
{
    HitAction result;
    if (index.data(PostDetailListModel::ItemTypeRole).toInt() == PostDetailListModel::DetailItem) {
        return result;
    }

    const auto* detailModel = qobject_cast<const PostDetailListModel*>(index.model());
    if (!detailModel) {
        return result;
    }

    const PostComment* comment = detailModel->commentAtIndex(index);
    if (!comment) {
        return result;
    }
    const Layout layout = calculateLayout(option, index);
    if (layout.commentLikeRect.adjusted(-6, -6, 6, 6).contains(point)
        || layout.commentLikeCountRect.contains(point)) {
        result.action = ToggleCommentLikeAction;
        result.commentId = comment->commentId;
        result.nextLiked = !comment->isLiked;
        return result;
    }
    if (layout.commentButtonRect.adjusted(-6, -6, 6, 6).contains(point)
        || layout.commentCountRect.contains(point)) {
        result.action = ReplyToCommentAction;
        result.commentId = comment->commentId;
        return result;
    }
    if (layout.expandRect.contains(point)) {
        result.action = ExpandCommentAction;
        result.commentId = comment->commentId;
        return result;
    }

    const int visibleReplies = qMin(detailModel->visibleReplyCount(comment->commentId), comment->replies.size());
    for (int i = 0; i < visibleReplies && i < layout.replyLayouts.size(); ++i) {
        const PostCommentReply& reply = comment->replies.at(i);
        const ReplyLayout& replyLayout = layout.replyLayouts.at(i);
        if (replyLayout.likeRect.adjusted(-6, -6, 6, 6).contains(point)
            || replyLayout.likeCountRect.contains(point)) {
            result.action = ToggleReplyLikeAction;
            result.commentId = comment->commentId;
            result.replyId = reply.replyId;
            result.nextLiked = !reply.isLiked;
            return result;
        }
        if (replyLayout.replyButtonRect.adjusted(-6, -6, 6, 6).contains(point)
            || replyLayout.replyTextRect.contains(point)) {
            result.action = ReplyToReplyAction;
            result.commentId = comment->commentId;
            result.replyId = reply.replyId;
            return result;
        }
        if (replyLayout.expandRect.contains(point)) {
            result.action = ExpandReplyAction;
            result.commentId = comment->commentId;
            result.replyId = reply.replyId;
            return result;
        }
    }

    if (layout.loadMoreRepliesRect.contains(point)) {
        result.action = LoadMoreRepliesAction;
        result.commentId = comment->commentId;
    }
    return result;
}

PostCommentDelegate::Layout PostCommentDelegate::calculateLayout(const QStyleOptionViewItem& option,
                                                                 const QModelIndex& index) const
{
    const QString commentId = index.data(PostDetailListModel::CommentIdRole).toString();
    const int width = availableWidth(option);
    const int revision = index.data(PostDetailListModel::CommentRevisionRole).toInt();
    const QString key = QStringLiteral("%1|%2|%3").arg(commentId,
                                                       QString::number(width),
                                                       QString::number(revision));

    Layout layout;
    if (const auto it = m_layoutCache.constFind(key); it != m_layoutCache.constEnd()) {
        layout = it.value();
    } else {
        QStyleOptionViewItem baseOption(option);
        baseOption.rect = QRect(0, 0, width, option.rect.height());
        layout = buildLayout(baseOption, index);
        m_layoutCache.insert(key, layout);
        trimCaches();
    }

    const QPoint offset = option.rect.topLeft();
    layout.avatarRect.translate(offset);
    layout.nameRect.translate(offset);
    layout.timeRect.translate(offset);
    layout.contentRect.translate(offset);
    layout.expandRect.translate(offset);
    layout.commentLikeRect.translate(offset);
    layout.commentLikeCountRect.translate(offset);
    layout.commentButtonRect.translate(offset);
    layout.commentCountRect.translate(offset);
    layout.loadMoreRepliesRect.translate(offset);
    for (ReplyLayout& replyLayout : layout.replyLayouts) {
        replyLayout.avatarRect.translate(offset);
        replyLayout.nameRect.translate(offset);
        replyLayout.timeRect.translate(offset);
        replyLayout.textRect.translate(offset);
        replyLayout.expandRect.translate(offset);
        replyLayout.likeRect.translate(offset);
        replyLayout.likeCountRect.translate(offset);
        replyLayout.replyButtonRect.translate(offset);
        replyLayout.replyTextRect.translate(offset);
    }
    return layout;
}

PostCommentDelegate::Layout PostCommentDelegate::buildLayout(const QStyleOptionViewItem& option,
                                                             const QModelIndex& index) const
{
    Layout layout;
    const auto* detailModel = qobject_cast<const PostDetailListModel*>(index.model());
    if (!detailModel) {
        return layout;
    }

    const PostComment* comment = detailModel->commentAtIndex(index);
    if (!comment) {
        return layout;
    }
    const int width = availableWidth(option);
    const int left = 0;
    const int top = 0;
    const int right = left + width - kOuterMargin;
    const int avatarX = left + kOuterMargin;
    const int contentX = avatarX + kCommentAvatarSize + kCommentAvatarTextGap;
    const int textWidth = qMax(80, right - contentX);
    const QFont body = bodyFont();
    const QFontMetrics nameMetrics(nameLayoutFont());
    const QFontMetrics metaMetrics(metaFont());
    const QFontMetrics actionMetrics(actionFont());

    int y = top + kCommentTopMargin;
    layout.avatarRect = QRect(avatarX, y, kCommentAvatarSize, kCommentAvatarSize);
    layout.nameRect = QRect(contentX, y - 1, textWidth, nameMetrics.lineSpacing());

    y += nameMetrics.lineSpacing() + kNameContentGap;
    const bool commentExpanded = detailModel->isCommentExpanded(comment->commentId);
    const TextMeasure commentMeasure = measureText(comment->content, body, textWidth, kCommentMaxLines);
    layout.contentMeasure = commentMeasure;
    layout.commentCanExpand = commentMeasure.canExpand;
    const int commentTextHeight = (commentExpanded || !layout.commentCanExpand)
            ? commentMeasure.fullHeight
            : commentMeasure.collapsedHeight;
    layout.contentRect = QRect(contentX, y, textWidth, commentTextHeight);
    y += commentTextHeight;

    if (layout.commentCanExpand && !commentExpanded) {
        y += 2;
        layout.expandRect = QRect(contentX, y, textWidth, actionMetrics.lineSpacing() + 4);
        y += layout.expandRect.height();
    }

    y += 4;
    layout.timeRect = QRect(contentX, y, textWidth, metaMetrics.lineSpacing());
    y += metaMetrics.lineSpacing() + kMetaActionGap;

    const int commentLikeCountWidth = countWidth(actionMetrics, comment->likeCount);
    const int commentCountWidth = countWidth(actionMetrics, comment->totalReplyCount);
    int actionX = contentX;
    layout.commentLikeRect = QRect(actionX, y + 1, kActionIconSize, kActionIconSize);
    actionX += kActionIconSize + kActionGap;
    layout.commentLikeCountRect = QRect(actionX, y, commentLikeCountWidth, actionMetrics.lineSpacing());
    actionX += commentLikeCountWidth + kActionGroupGap;
    layout.commentButtonRect = QRect(actionX, y + 1, kActionIconSize, kActionIconSize);
    actionX += kActionIconSize + kActionGap;
    layout.commentCountRect = QRect(actionX, y, commentCountWidth, actionMetrics.lineSpacing());
    y += qMax(kActionIconSize, actionMetrics.lineSpacing());

    const int visibleReplies = qMin(detailModel->visibleReplyCount(comment->commentId), comment->replies.size());
    if (visibleReplies > 0) {
        y += kReplyGap;
        const int replyAvatarX = contentX;
        const int replyContentX = replyAvatarX + kReplyAvatarSize + kReplyAvatarTextGap;
        const int replyTextWidth = qMax(70, right - replyContentX);

        for (int i = 0; i < visibleReplies; ++i) {
            const PostCommentReply& reply = comment->replies.at(i);
            const ReplyTextParts text = replyDisplayText(reply);
            const TextMeasure replyMeasure = measureText(text.text,
                                                         body,
                                                         replyTextWidth,
                                                         kReplyMaxLines,
                                                         text.targetNameStart,
                                                         text.targetNameLength);
            const bool replyCanExpand = replyMeasure.canExpand;
            const bool replyExpanded = detailModel->isReplyExpanded(reply.replyId);
            const int replyTextHeight = (replyExpanded || !replyCanExpand)
                    ? replyMeasure.fullHeight
                    : replyMeasure.collapsedHeight;

            ReplyLayout replyLayout;
            replyLayout.replyId = reply.replyId;
            replyLayout.avatarRect = QRect(replyAvatarX, y, kReplyAvatarSize, kReplyAvatarSize);
            replyLayout.nameRect = QRect(replyContentX, y - 1, replyTextWidth, nameMetrics.lineSpacing());
            y += nameMetrics.lineSpacing() + kNameContentGap;
            replyLayout.textRect = QRect(replyContentX, y, replyTextWidth, replyTextHeight);
            replyLayout.textMeasure = replyMeasure;
            y += replyTextHeight;
            if (replyCanExpand && !replyExpanded) {
                y += 2;
                replyLayout.expandRect = QRect(replyContentX, y, replyTextWidth, actionMetrics.lineSpacing() + 4);
                y += replyLayout.expandRect.height();
            }

            y += 4;
            replyLayout.timeRect = QRect(replyContentX, y, replyTextWidth, metaMetrics.lineSpacing());
            y += metaMetrics.lineSpacing() + kMetaActionGap;

            const int replyLikeCountWidth = countWidth(actionMetrics, reply.likeCount);
            int replyActionX = replyContentX;
            replyLayout.likeRect = QRect(replyActionX, y + 1, kActionIconSize, kActionIconSize);
            replyActionX += kActionIconSize + kActionGap;
            replyLayout.likeCountRect = QRect(replyActionX, y, replyLikeCountWidth, actionMetrics.lineSpacing());
            replyActionX += replyLikeCountWidth + kActionGroupGap;
            replyLayout.replyButtonRect = QRect(replyActionX, y + 1, kActionIconSize, kActionIconSize);
            replyActionX += kActionIconSize + kActionGap;
            replyLayout.replyTextRect = QRect(replyActionX, y, actionMetrics.horizontalAdvance(QStringLiteral("回复")) + 8, actionMetrics.lineSpacing());
            y += qMax(kActionIconSize, actionMetrics.lineSpacing());

            layout.replyLayouts.append(replyLayout);
            if (i != visibleReplies - 1) {
                y += kReplyGap;
            }
        }

        if (visibleReplies < comment->totalReplyCount) {
            y += kLoadMoreReplyTopGap;
            layout.loadMoreRepliesRect = QRect(replyContentX,
                                               y,
                                               replyTextWidth,
                                               actionMetrics.lineSpacing() + kLoadMoreReplyHeightExtra);
            y += layout.loadMoreRepliesRect.height();
        }
    }

    layout.totalHeight = y - top + kCommentBottomMargin;
    return layout;
}

PostCommentDelegate::TextMeasure PostCommentDelegate::measureText(const QString& text,
                                                                  const QFont& font,
                                                                  int width,
                                                                  int maxLines,
                                                                  int highlightedStart,
                                                                  int highlightedLength) const
{
    const QFontMetrics metrics(font);
    const int boundedWidth = qMax(1, width);
    const QString key = QStringLiteral("%1|%2|%3|%4|%5")
            .arg(font.toString(),
                 QString::number(boundedWidth),
                 QString::number(maxLines),
                 QString::number(highlightedStart),
                 QString::number(highlightedLength))
            + QStringLiteral("|")
            + text;
    if (const auto it = m_textMeasureCache.constFind(key); it != m_textMeasureCache.constEnd()) {
        return it.value();
    }

    TextMeasure measure;
    if (text.isEmpty()) {
        measure.fullHeight = metrics.lineSpacing();
        measure.collapsedHeight = metrics.lineSpacing();
        m_textMeasureCache.insert(key, measure);
        trimCaches();
        return measure;
    }

    QTextLayout textLayout(text, font);
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textLayout.setTextOption(textOption);
    textLayout.beginLayout();

    qreal y = 0.0;
    while (true) {
        QTextLine line = textLayout.createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLineWidth(boundedWidth);
        line.setPosition(QPointF(0.0, y));

        TextMeasure::Line measuredLine;
        measuredLine.y = qRound(line.y());
        measuredLine.baselineY = qRound(line.y() + line.ascent());
        measuredLine.height = qCeil(line.height());

        const int lineStart = line.textStart();
        const int lineEnd = lineStart + line.textLength();
        QVector<int> boundaries;
        boundaries.reserve(4);
        boundaries.append(lineStart);
        boundaries.append(lineEnd);

        const int highlightedEnd = highlightedStart + qMax(0, highlightedLength);
        if (highlightedStart >= 0 && highlightedEnd > highlightedStart) {
            const int overlapStart = qMax(lineStart, highlightedStart);
            const int overlapEnd = qMin(lineEnd, highlightedEnd);
            if (overlapStart < overlapEnd) {
                boundaries.append(overlapStart);
                boundaries.append(overlapEnd);
            }
        }

        std::sort(boundaries.begin(), boundaries.end());
        boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

        measuredLine.segments.reserve(qMax(1, boundaries.size() - 1));
        for (int i = 0; i + 1 < boundaries.size(); ++i) {
            const int segmentStart = boundaries.at(i);
            const int segmentEnd = boundaries.at(i + 1);
            if (segmentStart >= segmentEnd) {
                continue;
            }

            TextMeasure::Segment segment;
            segment.start = segmentStart;
            segment.length = segmentEnd - segmentStart;
            segment.x = metrics.horizontalAdvance(text.mid(lineStart, segmentStart - lineStart));
            segment.highlighted = highlightedStart >= 0
                    && segmentStart >= highlightedStart
                    && segmentEnd <= highlightedEnd;
            measuredLine.segments.append(segment);
        }

        measure.lines.append(measuredLine);
        y += line.height();
    }
    textLayout.endLayout();

    if (measure.lines.isEmpty()) {
        measure.fullHeight = metrics.lineSpacing();
        measure.collapsedHeight = metrics.lineSpacing();
    } else {
        const TextMeasure::Line& lastLine = measure.lines.constLast();
        measure.fullHeight = qMax(metrics.lineSpacing(), lastLine.y + lastLine.height);
        if (maxLines > 0 && measure.lines.size() > maxLines) {
            const TextMeasure::Line& collapsedLine = measure.lines.at(maxLines - 1);
            measure.collapsedHeight = qMax(metrics.lineSpacing(),
                                           collapsedLine.y + collapsedLine.height);
            measure.canExpand = true;
        } else {
            measure.collapsedHeight = measure.fullHeight;
        }
    }

    m_textMeasureCache.insert(key, measure);
    trimCaches();
    return measure;
}

void PostCommentDelegate::drawMeasuredText(QPainter* painter,
                                           const QString& text,
                                           const QRect& rect,
                                           const TextMeasure& measure) const
{
    if (text.isEmpty() || rect.isEmpty()) {
        return;
    }

    const QColor primary = ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor highlighted = ThemeManager::instance().color(ThemeColor::Accent);

    painter->save();
    painter->setFont(bodyFont());
    painter->setClipRect(rect);
    for (const TextMeasure::Line& line : measure.lines) {
        if (line.y >= rect.height()) {
            break;
        }
        for (const TextMeasure::Segment& segment : line.segments) {
            painter->setPen(segment.highlighted ? highlighted : primary);
            painter->drawText(QPoint(rect.left() + segment.x,
                                     rect.top() + line.baselineY),
                              text.mid(segment.start, segment.length));
        }
    }
    painter->restore();
}

void PostCommentDelegate::trimCaches() const
{
    if (m_layoutCache.size() > 512) {
        m_layoutCache.clear();
    }
    if (m_textMeasureCache.size() > 1024) {
        m_textMeasureCache.clear();
    }
}

int PostCommentDelegate::availableWidth(const QStyleOptionViewItem& option) const
{
    if (option.rect.width() > 0) {
        return option.rect.width();
    }
    if (option.widget) {
        return option.widget->width();
    }
    return 360;
}
