#pragma once

#include <QStyledItemDelegate>
#include <QHash>
#include <QString>
#include <QVector>

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

    struct HitAction {
        Action action = NoAction;
        QString commentId;
        QString replyId;
        bool nextLiked = false;
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
    void clearCaches();

private:
    struct TextMeasure {
        struct Segment {
            int start = 0;
            int length = 0;
            int x = 0;
            bool highlighted = false;
        };

        struct Line {
            int y = 0;
            int baselineY = 0;
            int height = 0;
            QVector<Segment> segments;
        };

        int fullHeight = 1;
        int collapsedHeight = 1;
        bool canExpand = false;
        QVector<Line> lines;
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
                          const TextMeasure& measure) const;
    void trimCaches() const;
    int availableWidth(const QStyleOptionViewItem& option) const;

    mutable QHash<QString, Layout> m_layoutCache;
    mutable QHash<QString, TextMeasure> m_textMeasureCache;
};
