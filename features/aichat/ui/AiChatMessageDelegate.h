#pragma once

#include <QCache>
#include <QPersistentModelIndex>
#include <QString>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QVector>

#include "shared/ui/SelectableText.h"

class QColor;

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

    bool selectWordAt(const QStyleOptionViewItem& option,
                      const QModelIndex& index,
                      const QPoint& viewportPos);
    void setSelection(const QModelIndex& index, int anchor, int cursor);
    void clearSelection();
    void setBubbleSelection(const QModelIndex& index);
    void clearBubbleSelection();
    bool hasSelection() const;
    bool hasBubbleSelection() const;
    bool selectionContains(const QModelIndex& index, int cursor) const;
    QString selectedText() const;
    QString renderedText(const QModelIndex& index) const;
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

    struct TextDocumentCacheEntry {
        QTextDocument document;
    };

    struct TextSizeCacheEntry {
        QSize size;
    };

    struct UrlRangesCacheEntry {
        QVector<TextRange> ranges;
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
    QSize textDocumentSize(const QString& text,
                           const QFont& font,
                           int maxTextWidth,
                           bool isFromUser) const;
    const QTextDocument& cachedTextDocument(const QString& text,
                                            const QFont& font,
                                            int textWidth,
                                            const QColor& textColor,
                                            bool isFromUser,
                                            bool dark) const;
    QVector<TextRange> cachedUrlRanges(const QString& text) const;
    QVector<TextRange> urlRanges(const QString& text) const;
    QString renderedPlainText(const QString& text, bool isFromUser) const;
    QFont messageFont() const;
    int maxBubbleWidth(int itemWidth) const;

    QPersistentModelIndex m_selectionIndex;
    QPersistentModelIndex m_bubbleSelectionIndex;
    SelectableText::Selection m_selection;
    mutable QCache<QString, TextDocumentCacheEntry> m_textDocumentCache;
    mutable QCache<QString, TextSizeCacheEntry> m_textSizeCache;
    mutable QCache<QString, UrlRangesCacheEntry> m_urlRangesCache;

    static constexpr int kVerticalMargin = 8;
    static constexpr int kHorizontalMargin = 24;
    static constexpr int kBubblePadding = 12;
    static constexpr int kBubbleRadius = 12;
};
