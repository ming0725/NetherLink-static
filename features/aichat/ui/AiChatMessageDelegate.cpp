#include "AiChatMessageDelegate.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QtMath>

#include "features/aichat/model/AiChatMessageListModel.h"
#include "shared/theme/ThemeManager.h"

namespace {

QColor linkTextColor(bool dark, bool isFromUser)
{
    if (isFromUser) {
        return ThemeManager::instance().color(ThemeColor::AccentLinkTextOnAccent);
    }

    Q_UNUSED(dark);
    return ThemeManager::instance().color(ThemeColor::AccentLinkText);
}

QString documentTextForLayout(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

void allowWrappedPreformattedBlocks(QTextDocument& document)
{
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat blockFormat = block.blockFormat();
        if (!blockFormat.nonBreakableLines()) {
            continue;
        }

        blockFormat.setNonBreakableLines(false);
        QTextCursor cursor(block);
        cursor.setBlockFormat(blockFormat);
    }
}

QString textLayoutCacheKey(const QString& text, const QFont& font, int textWidth)
{
    QString key = font.toString();
    key.reserve(key.size() + text.size() + 24);
    key += QLatin1Char('\x1f');
    key += QString::number(qMax(1, textWidth));
    key += QLatin1Char('\x1f');
    key += text;
    return key;
}

QString textDocumentCacheKey(const QString& text,
                             const QFont& font,
                             int textWidth,
                             const QColor& textColor,
                             bool isFromUser,
                             bool dark)
{
    QString key = textLayoutCacheKey(text, font, textWidth);
    key += QLatin1Char('\x1f');
    key += QString::number(textColor.rgba());
    key += QLatin1Char('\x1f');
    key += isFromUser ? QLatin1Char('1') : QLatin1Char('0');
    key += QLatin1Char('\x1f');
    key += dark ? QLatin1Char('1') : QLatin1Char('0');
    return key;
}

int textCacheCost(const QString& text)
{
    return qBound(1, text.size() / 512 + 1, 16);
}

} // namespace

AiChatMessageDelegate::AiChatMessageDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    m_textDocumentCache.setMaxCost(96);
    m_textSizeCache.setMaxCost(256);
    m_urlRangesCache.setMaxCost(256);
}

void AiChatMessageDelegate::paint(QPainter* painter,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return;
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    if (text.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const bool isFromUser = index.data(AiChatMessageListModel::IsFromUserRole).toBool();
    const bool dark = ThemeManager::instance().isDark();
    const QColor bubbleColor = isFromUser
            ? (dark ? ThemeManager::instance().color(ThemeColor::AccentBubble) : ThemeManager::instance().color(ThemeColor::Accent))
            : ThemeManager::instance().color(ThemeColor::MessageBubblePeer);
    const bool bubbleSelected = m_bubbleSelectionIndex == index;
    const QColor effectiveBubbleColor = bubbleSelected
            ? (isFromUser ? bubbleColor.darker(118)
                          : ThemeManager::instance().color(ThemeColor::MessageBubblePeerSelected))
            : bubbleColor;
    const QColor textColor = isFromUser
            ? ThemeManager::instance().color(ThemeColor::TextOnAccent)
            : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor selectionColor = isFromUser
            ? ThemeManager::instance().color(ThemeColor::AccentTextSelectionOnAccent)
            : ThemeManager::instance().color(ThemeColor::AccentTextSelection);

    const LayoutMetrics metrics = layoutMetrics(option, index);
    QPainterPath bubblePath;
    bubblePath.addRoundedRect(metrics.bubbleRect, kBubbleRadius, kBubbleRadius);
    painter->fillPath(bubblePath, effectiveBubbleColor);

    const QTextDocument& textDocument = cachedTextDocument(text,
                                                           messageFont(),
                                                           metrics.textRect.width(),
                                                           textColor,
                                                           isFromUser,
                                                           dark);

    QAbstractTextDocumentLayout::PaintContext paintContext;
    if (m_selectionIndex == index && hasSelection()) {
        QTextCharFormat selectionFormat;
        selectionFormat.setBackground(selectionColor);
        selectionFormat.setForeground(textColor);

        QAbstractTextDocumentLayout::Selection selection;
        selection.cursor = QTextCursor(const_cast<QTextDocument*>(&textDocument));
        selection.cursor.setPosition(m_selection.start());
        selection.cursor.setPosition(m_selection.end(), QTextCursor::KeepAnchor);
        selection.format = selectionFormat;
        paintContext.selections.push_back(selection);
    }

    painter->save();
    painter->translate(metrics.textRect.topLeft());
    painter->setClipRect(QRect(QPoint(0, 0), metrics.textRect.size()));
    textDocument.documentLayout()->draw(painter, paintContext);
    painter->restore();

    painter->restore();
}

QSize AiChatMessageDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return QSize(option.rect.width(),
                     qMax(0, index.data(AiChatMessageListModel::BottomSpaceHeightRole).toInt()));
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    if (text.isEmpty()) {
        return QSize(option.rect.width(), 0);
    }

    const LayoutMetrics metrics = layoutMetrics(option, index);
    return QSize(option.rect.width(), metrics.bubbleRect.height() + kVerticalMargin * 2);
}

bool AiChatMessageDelegate::bubbleHitTest(const QStyleOptionViewItem& option,
                                          const QModelIndex& index,
                                          const QPoint& viewportPos) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return false;
    }

    const LayoutMetrics metrics = layoutMetrics(option, index);
    QPainterPath bubblePath;
    bubblePath.addRoundedRect(metrics.bubbleRect, kBubbleRadius, kBubbleRadius);
    return bubblePath.contains(viewportPos);
}

int AiChatMessageDelegate::characterIndexAt(const QStyleOptionViewItem& option,
                                            const QModelIndex& index,
                                            const QPoint& viewportPos,
                                            bool allowLineWhitespace) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return -1;
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    if (text.isEmpty()) {
        return -1;
    }

    const LayoutMetrics metrics = layoutMetrics(option, index);
    const QRect hitRect = allowLineWhitespace
            ? metrics.bubbleRect.adjusted(-2, -2, 2, 2)
            : metrics.textRect;
    if (!hitRect.contains(viewportPos)) {
        return -1;
    }

    const bool isFromUser = index.data(AiChatMessageListModel::IsFromUserRole).toBool();
    const bool dark = ThemeManager::instance().isDark();
    const QColor textColor = isFromUser
            ? ThemeManager::instance().color(ThemeColor::TextOnAccent)
            : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QTextDocument& textDocument = cachedTextDocument(text,
                                                           messageFont(),
                                                           metrics.textRect.width(),
                                                           textColor,
                                                           isFromUser,
                                                           dark);

    const QPointF local = viewportPos - metrics.textRect.topLeft();
    const qreal height = textDocument.size().height();

    if (local.y() < 0) {
        return allowLineWhitespace ? 0 : -1;
    }
    if (local.y() == 0) {
        return 0;
    }
    if (local.y() >= height) {
        return allowLineWhitespace ? text.size() : -1;
    }

    const int textLength = textDocument.toPlainText().size();
    const int lineCursor = SelectableText::lineCursorAt(textDocument,
                                                            local,
                                                            textLength,
                                                            allowLineWhitespace);
    if (lineCursor >= 0) {
        return lineCursor;
    }

    const Qt::HitTestAccuracy accuracy = allowLineWhitespace ? Qt::FuzzyHit : Qt::ExactHit;
    const int cursor = textDocument.documentLayout()->hitTest(local, accuracy);
    if (cursor < 0) {
        return SelectableText::lineCursorAt(textDocument,
                                                local,
                                                textLength,
                                                allowLineWhitespace);
    }

    return qBound(0, cursor, textLength);
}

QString AiChatMessageDelegate::urlAt(const QStyleOptionViewItem& option,
                                     const QModelIndex& index,
                                     const QPoint& viewportPos) const
{
    if (index.data(AiChatMessageListModel::IsBottomSpaceRole).toBool()) {
        return {};
    }

    const int cursor = characterIndexAt(option, index, viewportPos);
    if (cursor < 0) {
        return {};
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    const LayoutMetrics metrics = layoutMetrics(option, index);
    if (!metrics.textRect.contains(viewportPos)) {
        return {};
    }

    const bool isFromUser = index.data(AiChatMessageListModel::IsFromUserRole).toBool();
    const bool dark = ThemeManager::instance().isDark();
    const QColor textColor = isFromUser
            ? ThemeManager::instance().color(ThemeColor::TextOnAccent)
            : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QTextDocument& textDocument = cachedTextDocument(text,
                                                           messageFont(),
                                                           metrics.textRect.width(),
                                                           textColor,
                                                           isFromUser,
                                                           dark);
    const QString anchor = textDocument.documentLayout()->anchorAt(viewportPos - metrics.textRect.topLeft());
    if (!anchor.isEmpty()) {
        return anchor;
    }

    const QString rendered = renderedPlainText(text);
    if (rendered == text) {
        const QVector<TextRange> urls = cachedUrlRanges(text);
        for (const TextRange& url : urls) {
            const int end = url.start + url.length;
            if (cursor >= url.start && cursor <= end) {
                return url.text;
            }
        }
    }

    return {};
}

bool AiChatMessageDelegate::selectWordAt(const QStyleOptionViewItem& option,
                                         const QModelIndex& index,
                                         const QPoint& viewportPos)
{
    const int cursor = characterIndexAt(option, index, viewportPos);
    if (cursor < 0) {
        return false;
    }

    const QString text = renderedPlainText(index.data(AiChatMessageListModel::TextRole).toString());
    const SelectableText::Range range = SelectableText::wordRangeAt(text, cursor);
    if (!range.isValid()) {
        return false;
    }

    setSelection(index, range.start, range.end);
    return hasSelection();
}

void AiChatMessageDelegate::setSelection(const QModelIndex& index, int anchor, int cursor)
{
    m_selectionIndex = QPersistentModelIndex(index);
    m_bubbleSelectionIndex = QPersistentModelIndex();
    const int textLength = renderedPlainText(index.data(AiChatMessageListModel::TextRole).toString()).size();
    m_selection.set(anchor, cursor, textLength);
}

void AiChatMessageDelegate::clearSelection()
{
    m_selectionIndex = QPersistentModelIndex();
    m_selection.clear();
}

void AiChatMessageDelegate::setBubbleSelection(const QModelIndex& index)
{
    clearSelection();
    m_bubbleSelectionIndex = QPersistentModelIndex(index);
}

void AiChatMessageDelegate::clearBubbleSelection()
{
    m_bubbleSelectionIndex = QPersistentModelIndex();
}

bool AiChatMessageDelegate::hasSelection() const
{
    return m_selectionIndex.isValid() && m_selection.hasSelection();
}

bool AiChatMessageDelegate::hasBubbleSelection() const
{
    return m_bubbleSelectionIndex.isValid();
}

bool AiChatMessageDelegate::selectionContains(const QModelIndex& index, int cursor) const
{
    if (!hasSelection() || m_selectionIndex != index || cursor < 0) {
        return false;
    }

    return m_selection.contains(cursor);
}

QString AiChatMessageDelegate::selectedText() const
{
    if (!hasSelection()) {
        return {};
    }

    const QString text = renderedPlainText(m_selectionIndex.data(AiChatMessageListModel::TextRole).toString());
    return m_selection.selectedText(text);
}

QString AiChatMessageDelegate::renderedText(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return {};
    }

    return renderedPlainText(index.data(AiChatMessageListModel::TextRole).toString());
}

QPersistentModelIndex AiChatMessageDelegate::selectionIndex() const
{
    return m_selectionIndex;
}

AiChatMessageDelegate::LayoutMetrics AiChatMessageDelegate::layoutMetrics(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const
{
    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    const int bubbleMaxWidth = maxBubbleWidth(option.rect.width());
    const int maxTextWidth = qMax(1, bubbleMaxWidth - kBubblePadding * 2);
    const QSize textSize = textDocumentSize(text, messageFont(), maxTextWidth);
    const int bubbleWidth = qMin(textSize.width() + kBubblePadding * 2,
                                 bubbleMaxWidth);
    const int bubbleHeight = textSize.height() + kBubblePadding * 2;
    const bool isFromUser = index.data(AiChatMessageListModel::IsFromUserRole).toBool();
    const int x = isFromUser
            ? option.rect.right() - kHorizontalMargin - bubbleWidth + 1
            : option.rect.left() + kHorizontalMargin;
    const QRect bubbleRect(x, option.rect.top() + kVerticalMargin, bubbleWidth, bubbleHeight);
    const QRect textRect = bubbleRect.adjusted(kBubblePadding,
                                               kBubblePadding,
                                               -kBubblePadding,
                                               -kBubblePadding);

    return {bubbleRect, textRect, textSize};
}

void AiChatMessageDelegate::configureTextDocument(QTextDocument& document,
                                                  const QString& text,
                                                  const QFont& font,
                                                  int textWidth,
                                                  const QColor& textColor,
                                                  bool isFromUser,
                                                  bool dark) const
{
    document.setUndoRedoEnabled(false);
    document.setDocumentMargin(0);
    document.setDefaultFont(font);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document.setDefaultTextOption(textOption);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    document.setMarkdown(documentTextForLayout(text));
    allowWrappedPreformattedBlocks(document);
#else
    document.setPlainText(documentTextForLayout(text));
#endif
    document.setDefaultTextOption(textOption);
    document.setTextWidth(qMax(1, textWidth));

    QTextCursor cursor(&document);
    cursor.select(QTextCursor::Document);
    QTextCharFormat textFormat;
    textFormat.setForeground(textColor);
    cursor.mergeCharFormat(textFormat);

    const QColor urlColor = linkTextColor(dark, isFromUser);
    QTextCharFormat linkFormat;
    linkFormat.setForeground(urlColor);
    linkFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);

    const int characterCount = qMax(0, document.characterCount() - 1);
    for (int position = 0; position < characterCount; ++position) {
        QTextCursor charCursor(&document);
        charCursor.setPosition(position);
        charCursor.setPosition(position + 1, QTextCursor::KeepAnchor);
        if (charCursor.charFormat().isAnchor()) {
            charCursor.mergeCharFormat(linkFormat);
        }
    }

    if (document.toPlainText() == text) {
        const QVector<TextRange> urls = cachedUrlRanges(text);
        for (const TextRange& url : urls) {
            cursor.clearSelection();
            cursor.setPosition(url.start);
            cursor.setPosition(url.start + url.length, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(linkFormat);
        }
    }
}

QSize AiChatMessageDelegate::textDocumentSize(const QString& text,
                                              const QFont& font,
                                              int maxTextWidth) const
{
    const QString key = textLayoutCacheKey(text, font, maxTextWidth);
    if (const TextSizeCacheEntry* entry = m_textSizeCache.object(key)) {
        return entry->size;
    }

    QTextDocument textDocument;
    configureTextDocument(textDocument,
                          text,
                          font,
                          maxTextWidth,
                          ThemeManager::instance().color(ThemeColor::PrimaryText),
                          false,
                          false);

    const int width = qMin(qCeil(textDocument.idealWidth()), maxTextWidth);
    const int height = qCeil(textDocument.size().height());
    const QSize size(qCeil(width), qCeil(height));

    auto* entry = new TextSizeCacheEntry;
    entry->size = size;
    m_textSizeCache.insert(key, entry, textCacheCost(text));
    return size;
}

const QTextDocument& AiChatMessageDelegate::cachedTextDocument(const QString& text,
                                                               const QFont& font,
                                                               int textWidth,
                                                               const QColor& textColor,
                                                               bool isFromUser,
                                                               bool dark) const
{
    const QString key = textDocumentCacheKey(text, font, textWidth, textColor, isFromUser, dark);
    if (const TextDocumentCacheEntry* entry = m_textDocumentCache.object(key)) {
        return entry->document;
    }

    auto* entry = new TextDocumentCacheEntry;
    configureTextDocument(entry->document,
                          text,
                          font,
                          textWidth,
                          textColor,
                          isFromUser,
                          dark);
    m_textDocumentCache.insert(key, entry, textCacheCost(text));
    return m_textDocumentCache.object(key)->document;
}

QVector<AiChatMessageDelegate::TextRange> AiChatMessageDelegate::cachedUrlRanges(
        const QString& text) const
{
    if (const UrlRangesCacheEntry* entry = m_urlRangesCache.object(text)) {
        return entry->ranges;
    }

    auto* entry = new UrlRangesCacheEntry;
    entry->ranges = urlRanges(text);
    const QVector<TextRange> ranges = entry->ranges;
    m_urlRangesCache.insert(text, entry, textCacheCost(text));
    return ranges;
}

QVector<AiChatMessageDelegate::TextRange> AiChatMessageDelegate::urlRanges(const QString& text) const
{
    static const QRegularExpression urlRegex(
            QStringLiteral(R"(\b((?:https?://|www\.)[A-Za-z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+))"),
            QRegularExpression::CaseInsensitiveOption);

    QVector<TextRange> ranges;
    QRegularExpressionMatchIterator it = urlRegex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString rawUrl = match.captured(1);
        const QString url = SelectableText::trimmedUrlText(rawUrl);
        if (url.isEmpty()) {
            continue;
        }

        TextRange range;
        range.start = match.capturedStart(1);
        range.length = url.size();
        range.text = url;
        ranges.push_back(range);
    }
    return ranges;
}

QString AiChatMessageDelegate::renderedPlainText(const QString& text) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QTextDocument document;
    document.setMarkdown(documentTextForLayout(text));
    return document.toPlainText();
#else
    return documentTextForLayout(text);
#endif
}

QFont AiChatMessageDelegate::messageFont() const
{
    QFont font = QApplication::font();
    font.setPixelSize(14);
    return font;
}

int AiChatMessageDelegate::maxBubbleWidth(int itemWidth) const
{
    const int availableWidth = qMax(0, itemWidth - kHorizontalMargin * 2);
    return qMin(static_cast<int>(itemWidth * 0.72), availableWidth);
}
