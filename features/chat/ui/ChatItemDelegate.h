#ifndef CHATITEMDELEGATE_H
#define CHATITEMDELEGATE_H

#include <QCache>
#include <QPersistentModelIndex>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QVector>

#include "shared/types/ChatMessage.h"
#include "shared/ui/SelectableText.h"
#include "shared/ui/StyledActionMenu.h"

class QColor;

class ChatItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ChatItemDelegate(QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                    const QStyleOptionViewItem& option,
                    const QModelIndex& index) override;
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
    bool hasSelection() const;
    bool selectionContains(const QModelIndex& index, int cursor) const;
    QString selectedText() const;
    QPersistentModelIndex selectionIndex() const;

signals:
    void deleteRequested(int row);

private:
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

    static constexpr int AVATAR_SIZE = 40;
    static constexpr int BUBBLE_MARGIN = 10;
    static constexpr int BUBBLE_PADDING = 12;
    static constexpr int BUBBLE_RADIUS = 10;
    static constexpr int IMAGE_RADIUS = 12;
    static constexpr int IMAGE_MAX_HEIGHT = 200;
    static constexpr int NAME_HEIGHT = 20;  // 群聊成员名字的高度
    static constexpr int GROUP_INFO_GAP = 5;  // 群成员名字与消息内容间距
    static constexpr int NAME_MAX_WIDTH = 140;  // 群聊成员名字的最大宽度
    static constexpr int ROLE_PADDING = 5;  // 身份标签的内边距
    static constexpr int ROLE_HEIGHT = 18;  // 身份标签的高度
    static constexpr int ROLE_RADIUS = 4;   // 身份标签的圆角半径
    static constexpr int HORIZONTAL_EDGE_MARGIN = 20;   // 聊天项左右安全边距
    
    // 时间标识相关常量
    static constexpr int TIME_HEADER_HEIGHT = 22;  // 时间标识高度
    static constexpr int TIME_HEADER_PADDING = 8;  // 时间标识内边距
    static constexpr int TIME_HEADER_RADIUS = 11;  // 时间标识圆角半径
    static constexpr int TIME_HEADER_MIN_WIDTH = 60;  // 时间标识最小宽度
    static constexpr int TIME_HEADER_FONT_SIZE = 11;  // 时间标识字体大小
    
    void drawBubble(QPainter* painter, const QRect& rect,
                    bool isFromMe, const ChatMessage* message, bool isSelected,
                    const QModelIndex& index) const;
    void drawTextMessage(QPainter* painter, const QRect& rect,
                         const QString& text, bool isFromMe,
                         const QColor& textColor,
                         const QModelIndex& index) const;
    void drawImageMessage(QPainter* painter, const QRect& rect,
                         const QString& imageSource, bool isSelected) const;
    void drawAvatar(QPainter* painter, const QRect& rect,
                    const QString& userId) const;
    void drawGroupInfo(QPainter* painter, const QRect& rect,
                      const ChatMessage* message) const;
    void drawGroupInfoForMe(QPainter* painter, const QRect& rect,
                          const ChatMessage* message) const;
    void drawTimeHeader(QPainter* painter, const QRect& rect,
                       const QString& text) const;

    QRect calculateBubbleRect(const QRect& contentRect,
                             const ChatMessage* message,
                             int maxWidth, bool isFromMe) const;
    QRect calculateAvatarRect(const QRect& contentRect,
                             bool isFromMe) const;
    int calculateMaxBubbleWidth(const QRect& contentRect) const;
    QRect calculateTimeHeaderRect(const QRect& contentRect,
                                const QString& text) const;
    QRect calculateTextRect(const QRect& bubbleRect) const;
    QFont messageFont() const;
    QSize textDocumentSize(const QString& text, const QFont& font, int maxTextWidth) const;
    const QTextDocument& cachedTextDocument(const QString& text,
                                            const QFont& font,
                                            int textWidth,
                                            const QColor& textColor,
                                            bool isFromMe,
                                            bool dark) const;
    QVector<TextRange> cachedUrlRanges(const QString& text) const;
    void configureTextDocument(QTextDocument& document,
                               const QString& text,
                               const QFont& font,
                               int textWidth,
                               const QColor& textColor,
                               bool isFromMe,
                               bool dark) const;
    QVector<TextRange> urlRanges(const QString& text) const;

    void showContextMenu(const QPoint& pos, const QModelIndex& index,
                        const ChatMessage* message) const;

    QPersistentModelIndex m_selectionIndex;
    SelectableText::Selection m_selection;
    mutable QCache<QString, TextDocumentCacheEntry> m_textDocumentCache;
    mutable QCache<QString, TextSizeCacheEntry> m_textSizeCache;
    mutable QCache<QString, UrlRangesCacheEntry> m_urlRangesCache;
};
#endif // CHATITEMDELEGATE_H 
