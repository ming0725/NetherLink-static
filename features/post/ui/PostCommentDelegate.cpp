#include "PostCommentDelegate.h"

#include <algorithm>

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QDate>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
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
constexpr int kPostBodyHorizontalMargin = 20;
constexpr int kPostBodyTopMargin = 20;
constexpr int kPostTitleBodyGap = 12;
constexpr int kPostBodyDateTopGap = 16;
constexpr int kPostBodyDividerTopGap = 16;
constexpr int kPostBodyBottomMargin = 16;

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

const QFont& postTitleFont()
{
    static const QFont font = fontWithPointSize(16, true);
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

QString documentTextForLayout(QString text)
{
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);
    return text;
}

void configurePlainTextDocument(QTextDocument& document,
                                const QString& text,
                                const QFont& font,
                                int textWidth,
                                const QColor& textColor,
                                const QColor& highlightedColor,
                                int highlightedStart,
                                int highlightedLength)
{
    document.setUndoRedoEnabled(false);
    document.setDocumentMargin(0);
    document.setDefaultFont(font);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document.setDefaultTextOption(textOption);
    document.setPlainText(documentTextForLayout(text));
    document.setTextWidth(qMax(1, textWidth));

    QTextCursor cursor(&document);
    cursor.select(QTextCursor::Document);
    QTextCharFormat textFormat;
    textFormat.setForeground(textColor);
    cursor.mergeCharFormat(textFormat);

    const int highlightedEnd = highlightedStart + qMax(0, highlightedLength);
    if (highlightedStart >= 0 && highlightedEnd > highlightedStart) {
        cursor.clearSelection();
        cursor.setPosition(qBound(0, highlightedStart, text.size()));
        cursor.setPosition(qBound(0, highlightedEnd, text.size()), QTextCursor::KeepAnchor);
        QTextCharFormat highlightedFormat;
        highlightedFormat.setForeground(highlightedColor);
        cursor.mergeCharFormat(highlightedFormat);
    }
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
    const int itemType = index.data(PostDetailListModel::ItemTypeRole).toInt();
    if (itemType == PostDetailListModel::PostBodyItem) {
        const QString title = index.data(PostDetailListModel::PostTitleTextRole).toString();
        const QString text = index.data(PostDetailListModel::PostBodyTextRole).toString();
        const QString dateText = index.data(PostDetailListModel::PostBodyDateTextRole).toString();
        const int textWidth = qMax(80, availableWidth(option) - kPostBodyHorizontalMargin * 2);
        const TextMeasure titleMeasure = measureText(title, postTitleFont(), textWidth, 0);
        const TextMeasure textMeasure = measureText(text, bodyFont(), textWidth, 0);
        const QRect titleRect(option.rect.left() + kPostBodyHorizontalMargin,
                              option.rect.top() + kPostBodyTopMargin,
                              textWidth,
                              titleMeasure.fullHeight);
        const QRect textRect(titleRect.left(),
                             titleRect.bottom() + 1 + kPostTitleBodyGap,
                             textWidth,
                             textMeasure.fullHeight);

        painter->save();
        painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        painter->fillRect(option.rect, ThemeManager::instance().color(ThemeColor::PanelBackground));
        drawMeasuredText(painter, title, titleRect, titleMeasure, PostTitleText, {}, {}, postTitleFont());
        drawMeasuredText(painter, text, textRect, textMeasure, PostContentText, {}, {}, bodyFont());

        int y = textRect.bottom() + 1 + kPostBodyDateTopGap;
        if (!dateText.isEmpty()) {
            painter->setFont(metaFont());
            painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
            const QFontMetrics metaMetrics(metaFont());
            painter->drawText(QRect(textRect.left(), y, textRect.width(), metaMetrics.lineSpacing()),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              dateText);
            y += metaMetrics.lineSpacing();
        }

        y += kPostBodyDividerTopGap;
        painter->setPen(QPen(ThemeManager::instance().color(ThemeColor::Divider), 1));
        painter->drawLine(textRect.left(), y, textRect.right(), y);
        painter->restore();
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

    drawSingleLineText(painter,
                       comment->authorName,
                       layout.nameRect,
                       CommentAuthorText,
                       comment->commentId,
                       {},
                       nameFont());

    drawMeasuredText(painter,
                     comment->content,
                     layout.contentRect,
                     layout.contentMeasure,
                     CommentContentText,
                     comment->commentId,
                     {},
                     bodyFont());

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
        drawSingleLineText(painter,
                           reply.authorName,
                           replyLayout.nameRect,
                           ReplyAuthorText,
                           comment->commentId,
                           reply.replyId,
                           nameFont());

        drawMeasuredText(painter,
                         text.text,
                         replyLayout.textRect,
                         replyLayout.textMeasure,
                         ReplyContentText,
                         comment->commentId,
                         reply.replyId,
                         bodyFont());

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
    const int itemType = index.data(PostDetailListModel::ItemTypeRole).toInt();
    if (itemType == PostDetailListModel::PostBodyItem) {
        const QString title = index.data(PostDetailListModel::PostTitleTextRole).toString();
        const QString text = index.data(PostDetailListModel::PostBodyTextRole).toString();
        const QString dateText = index.data(PostDetailListModel::PostBodyDateTextRole).toString();
        const int textWidth = qMax(80, availableWidth(option) - kPostBodyHorizontalMargin * 2);
        const TextMeasure titleMeasure = measureText(title, postTitleFont(), textWidth, 0);
        const TextMeasure textMeasure = measureText(text, bodyFont(), textWidth, 0);
        const QFontMetrics metaMetrics(metaFont());
        const int dateHeight = dateText.isEmpty() ? 0 : metaMetrics.lineSpacing();
        const int height = kPostBodyTopMargin
                + titleMeasure.fullHeight
                + kPostTitleBodyGap
                + textMeasure.fullHeight
                + kPostBodyDateTopGap
                + dateHeight
                + kPostBodyDividerTopGap
                + 1
                + kPostBodyBottomMargin;
        return QSize(availableWidth(option), qMax(1, height));
    }

    const Layout layout = calculateLayout(option, index);
    return QSize(availableWidth(option), layout.totalHeight);
}

PostCommentDelegate::HitAction PostCommentDelegate::actionAt(const QStyleOptionViewItem& option,
                                                             const QModelIndex& index,
                                                             const QPoint& point) const
{
    HitAction result;
    if (index.data(PostDetailListModel::ItemTypeRole).toInt() == PostDetailListModel::PostBodyItem) {
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

PostCommentDelegate::TextHit PostCommentDelegate::textHitAt(const QStyleOptionViewItem& option,
                                                            const QModelIndex& index,
                                                            const QPoint& point,
                                                            bool allowLineWhitespace) const
{
    TextHit hit;
    const int itemType = index.data(PostDetailListModel::ItemTypeRole).toInt();

    if (itemType == PostDetailListModel::PostBodyItem) {
        const QString title = index.data(PostDetailListModel::PostTitleTextRole).toString();
        const QString text = index.data(PostDetailListModel::PostBodyTextRole).toString();
        const int textWidth = qMax(80, availableWidth(option) - kPostBodyHorizontalMargin * 2);
        const TextMeasure titleMeasure = measureText(title, postTitleFont(), textWidth, 0);
        const TextMeasure textMeasure = measureText(text, bodyFont(), textWidth, 0);
        const QRect titleRect(option.rect.left() + kPostBodyHorizontalMargin,
                              option.rect.top() + kPostBodyTopMargin,
                              textWidth,
                              titleMeasure.fullHeight);
        const int titleCursor = measuredCharacterIndexAt(title,
                                                         postTitleFont(),
                                                         titleRect,
                                                         point,
                                                         allowLineWhitespace);
        if (titleCursor >= 0) {
            hit.target = PostTitleText;
            hit.text = title;
            hit.cursor = titleCursor;
            return hit;
        }

        const QRect textRect(titleRect.left(),
                             titleRect.bottom() + 1 + kPostTitleBodyGap,
                             textWidth,
                             textMeasure.fullHeight);
        const int cursor = measuredCharacterIndexAt(text,
                                                    bodyFont(),
                                                    textRect,
                                                    point,
                                                    allowLineWhitespace);
        if (cursor >= 0) {
            hit.target = PostContentText;
            hit.text = text;
            hit.cursor = cursor;
        }
        return hit;
    }

    const auto* detailModel = qobject_cast<const PostDetailListModel*>(index.model());
    if (!detailModel) {
        return hit;
    }

    const PostComment* comment = detailModel->commentAtIndex(index);
    if (!comment) {
        return hit;
    }

    const Layout layout = calculateLayout(option, index);
    const int commentContentCursor = measuredCharacterIndexAt(comment->content,
                                                             bodyFont(),
                                                             layout.contentRect,
                                                             point,
                                                             allowLineWhitespace);
    if (commentContentCursor >= 0) {
        hit.target = CommentContentText;
        hit.commentId = comment->commentId;
        hit.text = comment->content;
        hit.cursor = commentContentCursor;
        return hit;
    }

    const int visibleReplies = qMin(detailModel->visibleReplyCount(comment->commentId), comment->replies.size());
    for (int i = 0; i < visibleReplies && i < layout.replyLayouts.size(); ++i) {
        const PostCommentReply& reply = comment->replies.at(i);
        const ReplyLayout& replyLayout = layout.replyLayouts.at(i);

        const ReplyTextParts text = replyDisplayText(reply);
        const int replyTextCursor = measuredCharacterIndexAt(text.text,
                                                            bodyFont(),
                                                            replyLayout.textRect,
                                                            point,
                                                            allowLineWhitespace);
        if (replyTextCursor >= 0) {
            hit.target = ReplyContentText;
            hit.commentId = comment->commentId;
            hit.replyId = reply.replyId;
            hit.text = text.text;
            hit.cursor = replyTextCursor;
            return hit;
        }
    }

    return hit;
}

bool PostCommentDelegate::authorHitAt(const QStyleOptionViewItem& option,
                                      const QModelIndex& index,
                                      const QPoint& point) const
{
    if (index.data(PostDetailListModel::ItemTypeRole).toInt() == PostDetailListModel::PostBodyItem) {
        return false;
    }

    const auto* detailModel = qobject_cast<const PostDetailListModel*>(index.model());
    if (!detailModel) {
        return false;
    }

    const PostComment* comment = detailModel->commentAtIndex(index);
    if (!comment) {
        return false;
    }

    const Layout layout = calculateLayout(option, index);
    if (layout.avatarRect.contains(point) ||
            singleLineCharacterIndexAt(comment->authorName, nameFont(), layout.nameRect, point, false) >= 0) {
        return true;
    }

    const int visibleReplies = qMin(detailModel->visibleReplyCount(comment->commentId), comment->replies.size());
    for (int i = 0; i < visibleReplies && i < layout.replyLayouts.size(); ++i) {
        const PostCommentReply& reply = comment->replies.at(i);
        const ReplyLayout& replyLayout = layout.replyLayouts.at(i);
        if (replyLayout.avatarRect.contains(point) ||
                singleLineCharacterIndexAt(reply.authorName, nameFont(), replyLayout.nameRect, point, false) >= 0) {
            return true;
        }
    }

    return false;
}

PostCommentDelegate::InteractionTarget PostCommentDelegate::interactionTargetAt(const QStyleOptionViewItem& option,
                                                                                const QModelIndex& index,
                                                                                const QPoint& point) const
{
    InteractionTarget target;
    if (index.data(PostDetailListModel::ItemTypeRole).toInt() == PostDetailListModel::PostBodyItem) {
        return target;
    }

    const auto* detailModel = qobject_cast<const PostDetailListModel*>(index.model());
    if (!detailModel) {
        return target;
    }

    const PostComment* comment = detailModel->commentAtIndex(index);
    if (!comment) {
        return target;
    }

    const Layout layout = calculateLayout(option, index);
    const int visibleReplies = qMin(detailModel->visibleReplyCount(comment->commentId), comment->replies.size());
    for (int i = 0; i < visibleReplies && i < layout.replyLayouts.size(); ++i) {
        const PostCommentReply& reply = comment->replies.at(i);
        const ReplyLayout& replyLayout = layout.replyLayouts.at(i);
        const QRect replyBounds = replyLayout.avatarRect
                .united(replyLayout.nameRect)
                .united(replyLayout.textRect)
                .united(replyLayout.timeRect)
                .united(replyLayout.likeRect)
                .united(replyLayout.likeCountRect)
                .united(replyLayout.replyButtonRect)
                .united(replyLayout.replyTextRect)
                .united(replyLayout.expandRect);
        if (replyBounds.contains(point)) {
            target.commentId = comment->commentId;
            target.replyId = reply.replyId;
            target.text = replyDisplayText(reply).text;
            target.isReply = true;
            return target;
        }
    }

    const QRect commentBounds = layout.avatarRect
            .united(layout.nameRect)
            .united(layout.contentRect)
            .united(layout.timeRect)
            .united(layout.commentLikeRect)
            .united(layout.commentLikeCountRect)
            .united(layout.commentButtonRect)
            .united(layout.commentCountRect)
            .united(layout.expandRect);
    if (commentBounds.contains(point) || option.rect.contains(point)) {
        target.commentId = comment->commentId;
        target.text = comment->content;
    }
    return target;
}

bool PostCommentDelegate::selectWordAt(const QStyleOptionViewItem& option,
                                       const QModelIndex& index,
                                       const QPoint& point)
{
    const TextHit hit = textHitAt(option, index, point);
    if (!hit.isValid()) {
        return false;
    }

    const SelectableText::Range range = SelectableText::wordRangeAt(hit.text, hit.cursor);
    if (!range.isValid()) {
        return false;
    }

    setSelection(index, hit.target, hit.commentId, hit.replyId, range.start, range.end);
    return hasSelection();
}

void PostCommentDelegate::setSelection(const QModelIndex& index,
                                       TextTargetType target,
                                       const QString& commentId,
                                       const QString& replyId,
                                       int anchor,
                                       int cursor)
{
    const QString text = textForTarget(index, target, commentId, replyId);
    if (text.isEmpty()) {
        clearSelection();
        return;
    }

    m_selectionIndex = QPersistentModelIndex(index);
    m_selectionTarget = target;
    m_selectionCommentId = commentId;
    m_selectionReplyId = replyId;
    m_selection.set(anchor, cursor, text.size());
}

void PostCommentDelegate::clearSelection()
{
    m_selectionIndex = QPersistentModelIndex();
    m_selectionTarget = NoTextTarget;
    m_selectionCommentId.clear();
    m_selectionReplyId.clear();
    m_selection.clear();
}

bool PostCommentDelegate::hasSelection() const
{
    return m_selectionIndex.isValid() &&
            m_selectionTarget != NoTextTarget &&
            m_selection.hasSelection();
}

bool PostCommentDelegate::selectionContains(const QModelIndex& index,
                                            TextTargetType target,
                                            const QString& commentId,
                                            const QString& replyId,
                                            int cursor) const
{
    if (!hasSelection() || !selectionMatches(index, target, commentId, replyId) || cursor < 0) {
        return false;
    }
    return m_selection.contains(cursor);
}

QString PostCommentDelegate::selectedText() const
{
    if (!hasSelection()) {
        return {};
    }

    const QString text = textForTarget(m_selectionIndex,
                                       m_selectionTarget,
                                       m_selectionCommentId,
                                       m_selectionReplyId);
    return m_selection.selectedText(text);
}

QPersistentModelIndex PostCommentDelegate::selectionIndex() const
{
    return m_selectionIndex;
}

PostCommentDelegate::TextTargetType PostCommentDelegate::selectionTarget() const
{
    return m_selectionTarget;
}

QString PostCommentDelegate::selectionCommentId() const
{
    return m_selectionCommentId;
}

QString PostCommentDelegate::selectionReplyId() const
{
    return m_selectionReplyId;
}

void PostCommentDelegate::selectAll(const QModelIndex& index,
                                    TextTargetType target,
                                    const QString& commentId,
                                    const QString& replyId)
{
    const QString text = textForTarget(index, target, commentId, replyId);
    if (text.isEmpty()) {
        return;
    }
    setSelection(index, target, commentId, replyId, 0, text.size());
}

int PostCommentDelegate::textLengthForTarget(const QModelIndex& index,
                                             TextTargetType target,
                                             const QString& commentId,
                                             const QString& replyId) const
{
    return textForTarget(index, target, commentId, replyId).size();
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
    measure.highlightedStart = highlightedStart;
    measure.highlightedLength = highlightedLength;
    if (text.isEmpty()) {
        measure.fullHeight = metrics.lineSpacing();
        measure.collapsedHeight = metrics.lineSpacing();
        m_textMeasureCache.insert(key, measure);
        trimCaches();
        return measure;
    }

    QTextDocument textDocument;
    configurePlainTextDocument(textDocument,
                               text,
                               font,
                               boundedWidth,
                               ThemeManager::instance().color(ThemeColor::PrimaryText),
                               ThemeManager::instance().color(ThemeColor::Accent),
                               highlightedStart,
                               highlightedLength);

    int lineCount = 0;
    int collapsedHeight = metrics.lineSpacing();
    for (QTextBlock block = textDocument.begin(); block.isValid(); block = block.next()) {
        const QTextLayout* layout = block.layout();
        if (!layout) {
            continue;
        }
        const QPointF blockPosition = textDocument.documentLayout()->blockBoundingRect(block).topLeft();
        for (int i = 0; i < layout->lineCount(); ++i) {
            const QTextLine line = layout->lineAt(i);
            ++lineCount;
            if (maxLines > 0 && lineCount == maxLines) {
                collapsedHeight = qMax(metrics.lineSpacing(),
                                       qCeil(blockPosition.y() + line.y() + line.height()));
            }
        }
    }

    if (lineCount == 0) {
        measure.fullHeight = metrics.lineSpacing();
        measure.collapsedHeight = metrics.lineSpacing();
    } else {
        measure.fullHeight = qMax(metrics.lineSpacing(), qCeil(textDocument.size().height()));
        if (maxLines > 0 && lineCount > maxLines) {
            measure.collapsedHeight = collapsedHeight;
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
                                           const TextMeasure& measure,
                                           TextTargetType target,
                                           const QString& commentId,
                                           const QString& replyId,
                                           const QFont& font) const
{
    if (text.isEmpty() || rect.isEmpty()) {
        return;
    }

    const QColor primary = ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor highlighted = ThemeManager::instance().color(ThemeColor::Accent);
    const QColor selectionColor = ThemeManager::instance().isDark()
            ? QColor(0x4c, 0x82, 0xc5, 150)
            : QColor(0x8b, 0xb7, 0xff, 130);
    const bool selected = selectionMatches(QModelIndex(m_selectionIndex), target, commentId, replyId) && hasSelection();

    QTextDocument textDocument;
    configurePlainTextDocument(textDocument,
                               text,
                               font,
                               rect.width(),
                               primary,
                               highlighted,
                               measure.highlightedStart,
                               measure.highlightedLength);

    QAbstractTextDocumentLayout::PaintContext paintContext;
    if (selected) {
        QTextCharFormat selectionFormat;
        selectionFormat.setBackground(selectionColor);
        selectionFormat.setForeground(primary);

        QAbstractTextDocumentLayout::Selection selection;
        selection.cursor = QTextCursor(&textDocument);
        selection.cursor.setPosition(m_selection.start());
        selection.cursor.setPosition(m_selection.end(), QTextCursor::KeepAnchor);
        selection.format = selectionFormat;
        paintContext.selections.push_back(selection);
    }

    painter->save();
    painter->setClipRect(rect);
    painter->translate(rect.left(), rect.top());
    textDocument.documentLayout()->draw(painter, paintContext);
    painter->restore();
}

void PostCommentDelegate::drawSingleLineText(QPainter* painter,
                                             const QString& text,
                                             const QRect& rect,
                                             TextTargetType target,
                                             const QString& commentId,
                                             const QString& replyId,
                                             const QFont& font) const
{
    if (text.isEmpty() || rect.isEmpty()) {
        return;
    }

    const QFontMetrics metrics(font);
    const QColor primary = ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor selectionColor = ThemeManager::instance().isDark()
            ? QColor(0x4c, 0x82, 0xc5, 150)
            : QColor(0x8b, 0xb7, 0xff, 130);
    const int lineHeight = metrics.lineSpacing();
    const int textY = rect.top() + (rect.height() - lineHeight) / 2;

    painter->save();
    painter->setFont(font);
    painter->setClipRect(rect);
    if (selectionMatches(QModelIndex(m_selectionIndex), target, commentId, replyId) && hasSelection()) {
        const int start = m_selection.start();
        const int end = m_selection.end();
        const int x1 = metrics.horizontalAdvance(text.left(start));
        const int x2 = metrics.horizontalAdvance(text.left(end));
        if (x2 > x1) {
            painter->fillRect(QRect(rect.left() + x1,
                                    textY,
                                    qMin(x2, rect.width()) - qMin(x1, rect.width()),
                                    lineHeight),
                              selectionColor);
        }
    }
    painter->setPen(primary);
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
    painter->restore();
}

int PostCommentDelegate::singleLineCharacterIndexAt(const QString& text,
                                                    const QFont& font,
                                                    const QRect& rect,
                                                    const QPoint& point,
                                                    bool allowLineWhitespace) const
{
    if (text.isEmpty() || !rect.adjusted(0, -2, 0, 2).contains(point)) {
        return -1;
    }

    QTextLayout layout(text, font);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (!line.isValid()) {
        layout.endLayout();
        return -1;
    }
    line.setLineWidth(qMax(1, rect.width()));
    layout.endLayout();

    const qreal x = point.x() - rect.left();
    if (!allowLineWhitespace && (x < 0.0 || x > rect.width())) {
        return -1;
    }
    if (x >= line.naturalTextWidth()) {
        return text.size();
    }
    return qBound(0, line.xToCursor(qBound(0.0, x, static_cast<qreal>(rect.width()))), text.size());
}

int PostCommentDelegate::measuredCharacterIndexAt(const QString& text,
                                                  const QFont& font,
                                                  const QRect& rect,
                                                  const QPoint& point,
                                                  bool allowLineWhitespace) const
{
    if (text.isEmpty() || rect.isEmpty()) {
        return -1;
    }

    if (point.y() < rect.top() || point.y() > rect.bottom()) {
        return -1;
    }
    if (!allowLineWhitespace && !rect.contains(point)) {
        return -1;
    }

    QTextDocument textDocument;
    configurePlainTextDocument(textDocument,
                               text,
                               font,
                               rect.width(),
                               ThemeManager::instance().color(ThemeColor::PrimaryText),
                               ThemeManager::instance().color(ThemeColor::Accent),
                               -1,
                               0);

    const QPointF local = point - rect.topLeft();
    if (local.y() < 0) {
        return allowLineWhitespace ? 0 : -1;
    }
    if (local.y() >= qMin<qreal>(textDocument.size().height(), rect.height())) {
        return allowLineWhitespace ? text.size() : -1;
    }

    const int lineCursor = SelectableText::lineCursorAt(textDocument,
                                                            local,
                                                            text.size(),
                                                            allowLineWhitespace);
    if (lineCursor >= 0) {
        return lineCursor;
    }

    const Qt::HitTestAccuracy accuracy = allowLineWhitespace ? Qt::FuzzyHit : Qt::ExactHit;
    const int cursor = textDocument.documentLayout()->hitTest(local, accuracy);
    if (cursor < 0) {
        return SelectableText::lineCursorAt(textDocument,
                                                local,
                                                text.size(),
                                                allowLineWhitespace);
    }
    return qBound(0, cursor, text.size());
}

QString PostCommentDelegate::textForTarget(const QModelIndex& index,
                                           TextTargetType target,
                                           const QString& commentId,
                                           const QString& replyId) const
{
    if (target == PostTitleText &&
            index.data(PostDetailListModel::ItemTypeRole).toInt() == PostDetailListModel::PostBodyItem) {
        return index.data(PostDetailListModel::PostTitleTextRole).toString();
    }

    if (target == PostContentText &&
            index.data(PostDetailListModel::ItemTypeRole).toInt() == PostDetailListModel::PostBodyItem) {
        return index.data(PostDetailListModel::PostBodyTextRole).toString();
    }

    const auto* detailModel = qobject_cast<const PostDetailListModel*>(index.model());
    if (!detailModel || commentId.isEmpty()) {
        return {};
    }

    const PostComment* comment = detailModel->commentById(commentId);
    if (!comment) {
        return {};
    }

    switch (target) {
    case CommentAuthorText:
        return comment->authorName;
    case CommentContentText:
        return comment->content;
    case ReplyAuthorText:
    case ReplyContentText:
        for (const PostCommentReply& reply : comment->replies) {
            if (reply.replyId == replyId) {
                return target == ReplyAuthorText ? reply.authorName : replyDisplayText(reply).text;
            }
        }
        return {};
    default:
        return {};
    }
}

bool PostCommentDelegate::selectionMatches(const QModelIndex& index,
                                           TextTargetType target,
                                           const QString& commentId,
                                           const QString& replyId) const
{
    return m_selectionIndex.isValid() &&
            m_selectionIndex == index &&
            m_selectionTarget == target &&
            m_selectionCommentId == commentId &&
            m_selectionReplyId == replyId;
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
