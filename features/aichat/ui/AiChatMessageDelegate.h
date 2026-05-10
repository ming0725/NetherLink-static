#pragma once

#include <QPersistentModelIndex>
#include <QString>
#include <QStyledItemDelegate>
#include <QVector>

class QColor;
class QTextDocument;

class AiChatMessageDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit AiChatMessageDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    bool bubbleHitTest(const QStyleOptionViewItem& option,
                       const QModelIndex& index,
                       const QPoint& viewportPos) const;
    int characterIndexAt(const QStyleOptionViewItem& option,
                         const QModelIndex& index,
                         const QPoint& viewportPos,
                         bool allowLineWhitespace = false) const;
    QString urlAt(const QStyleOptionViewItem& option,
                  const QModelIndex& index,
                  const QPoint& viewportPos) const;

    void setSelection(const QModelIndex& index, int anchor, int cursor);
    void clearSelection();
    bool hasSelection() const;
    bool selectionContains(const QModelIndex& index, int cursor) const;
    QString selectedText() const;
    QPersistentModelIndex selectionIndex() const;

private:
    struct LayoutMetrics {
        QRect bubbleRect;
        QRect textRect;
        QSize textSize;
    };

    struct TextRange {
        int start = -1;
        int length = 0;
        QString text;
    };

    LayoutMetrics layoutMetrics(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const;
    void configureTextDocument(QTextDocument& document,
                               const QString& text,
                               const QFont& font,
                               int textWidth,
                               const QColor& textColor,
                               bool isFromUser,
                               bool dark) const;
    QSize textDocumentSize(const QString& text, const QFont& font, int maxTextWidth) const;
    QVector<TextRange> urlRanges(const QString& text) const;
    QFont messageFont() const;
    int maxBubbleWidth(int itemWidth) const;
    int selectionStart() const;
    int selectionEnd() const;

    QPersistentModelIndex m_selectionIndex;
    int m_selectionAnchor = -1;
    int m_selectionCursor = -1;

    static constexpr int kVerticalMargin = 8;
    static constexpr int kHorizontalMargin = 24;
    static constexpr int kBubblePadding = 12;
    static constexpr int kBubbleRadius = 12;
};
