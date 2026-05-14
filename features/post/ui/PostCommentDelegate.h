#pragma once

#include <QStyledItemDelegate>
#include <QHash>
#include <QPersistentModelIndex>
#include <QString>
#include <QVector>

#include "shared/ui/SelectableText.h"

class PostCommentDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    enum Action {
        NoAction,
        ToggleCommentLikeAction,
        ExpandCommentAction,
        ReplyToCommentAction,
        ToggleReplyLikeAction,
        ExpandReplyAction,
        ReplyToReplyAction,
        LoadMoreRepliesAction
    };

    enum TextTargetType {
        NoTextTarget,
        PostTitleText,
        PostContentText,
        CommentAuthorText,
        CommentContentText,
        ReplyAuthorText,
        ReplyContentText
    };

    struct HitAction {
        Action action = NoAction;
        QString commentId;
        QString replyId;
        bool nextLiked = false;
    };

    struct TextHit {
        TextTargetType target = NoTextTarget;
        QString commentId;
        QString replyId;
        QString text;
        int cursor = -1;

        bool isValid() const { return target != NoTextTarget && cursor >= 0; }
    };

    struct InteractionTarget {
        QString commentId;
        QString replyId;
        QString text;
        bool isReply = false;

        bool isValid() const { return !commentId.isEmpty(); }
    };

    explicit PostCommentDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    HitAction actionAt(const QStyleOptionViewItem& option,
                       const QModelIndex& index,
                       const QPoint& point) const;
    TextHit textHitAt(const QStyleOptionViewItem& option,
                      const QModelIndex& index,
                      const QPoint& point,
                      bool allowLineWhitespace = false) const;
    bool authorHitAt(const QStyleOptionViewItem& option,
                     const QModelIndex& index,
                     const QPoint& point) const;
    InteractionTarget interactionTargetAt(const QStyleOptionViewItem& option,
                                          const QModelIndex& index,
                                          const QPoint& point) const;
    bool selectWordAt(const QStyleOptionViewItem& option,
                      const QModelIndex& index,
                      const QPoint& point);
    void setSelection(const QModelIndex& index,
                      TextTargetType target,
                      const QString& commentId,
                      const QString& replyId,
                      int anchor,
                      int cursor);
    void clearSelection();
    bool hasSelection() const;
    bool selectionContains(const QModelIndex& index,
                           TextTargetType target,
                           const QString& commentId,
                           const QString& replyId,
                           int cursor) const;
    QString selectedText() const;
    QPersistentModelIndex selectionIndex() const;
    TextTargetType selectionTarget() const;
    QString selectionCommentId() const;
    QString selectionReplyId() const;
    void selectAll(const QModelIndex& index,
                   TextTargetType target,
                   const QString& commentId,
                   const QString& replyId);
    int textLengthForTarget(const QModelIndex& index,
                            TextTargetType target,
                            const QString& commentId,
                            const QString& replyId) const;
    void clearCaches();

private:
    struct TextMeasure {
        int fullHeight = 1;
        int collapsedHeight = 1;
        bool canExpand = false;
        int highlightedStart = -1;
        int highlightedLength = 0;
    };

    struct ReplyLayout {
        QString replyId;
        QRect avatarRect;
        QRect nameRect;
        QRect timeRect;
        QRect textRect;
        QRect expandRect;
        QRect likeRect;
        QRect likeCountRect;
        QRect replyButtonRect;
        QRect replyTextRect;
        TextMeasure textMeasure;
        QRect revealClipRect;
        qreal revealProgress = 1.0;
    };

    struct Layout {
        QRect avatarRect;
        QRect nameRect;
        QRect timeRect;
        QRect contentRect;
        QRect expandRect;
        QRect commentLikeRect;
        QRect commentLikeCountRect;
        QRect commentButtonRect;
        QRect commentCountRect;
        TextMeasure contentMeasure;
        QVector<ReplyLayout> replyLayouts;
        QRect loadMoreRepliesRect;
        int totalHeight = 1;
        bool commentCanExpand = false;
    };

    Layout calculateLayout(const QStyleOptionViewItem& option,
                           const QModelIndex& index) const;
    Layout buildLayout(const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;
    TextMeasure measureText(const QString& text,
                            const QFont& font,
                            int width,
                            int maxLines,
                            int highlightedStart = -1,
                            int highlightedLength = 0) const;
    void drawMeasuredText(QPainter* painter,
                          const QString& text,
                          const QRect& rect,
                          const TextMeasure& measure,
                          TextTargetType target,
                          const QString& commentId,
                          const QString& replyId,
                          const QFont& font) const;
    void drawSingleLineText(QPainter* painter,
                            const QString& text,
                            const QRect& rect,
                            TextTargetType target,
                            const QString& commentId,
                            const QString& replyId,
                            const QFont& font) const;
    int singleLineCharacterIndexAt(const QString& text,
                                   const QFont& font,
                                   const QRect& rect,
                                   const QPoint& point,
                                   bool allowLineWhitespace) const;
    int measuredCharacterIndexAt(const QString& text,
                                 const QFont& font,
                                 const QRect& rect,
                                 const QPoint& point,
                                 bool allowLineWhitespace) const;
    QString textForTarget(const QModelIndex& index,
                          TextTargetType target,
                          const QString& commentId,
                          const QString& replyId) const;
    bool selectionMatches(const QModelIndex& index,
                          TextTargetType target,
                          const QString& commentId,
                          const QString& replyId) const;
    void trimCaches() const;
    int availableWidth(const QStyleOptionViewItem& option) const;

    QPersistentModelIndex m_selectionIndex;
    TextTargetType m_selectionTarget = NoTextTarget;
    QString m_selectionCommentId;
    QString m_selectionReplyId;
    SelectableText::Selection m_selection;
    mutable QHash<QString, Layout> m_layoutCache;
    mutable QHash<QString, TextMeasure> m_textMeasureCache;
};
