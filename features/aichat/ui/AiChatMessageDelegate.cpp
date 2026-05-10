#include "AiChatMessageDelegate.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
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
        return QColor(0xff, 0xe2, 0x72);
    }

    return dark ? QColor(0x6a, 0xb7, 0xff) : QColor(0x1b, 0x6e, 0xd6);
}

QString trimmedUrlText(const QString& text)
{
    QString url = text;
    while (!url.isEmpty()) {
        const QChar last = url.back();
        if (last == QLatin1Char('.') || last == QLatin1Char(',') ||
                last == QLatin1Char(';') || last == QLatin1Char(':') ||
                last == QLatin1Char('!') || last == QLatin1Char('?') ||
                last == QLatin1Char(')') || last == QLatin1Char(']') ||
                last == QLatin1Char('}') || last == QLatin1Char('"') ||
                last == QLatin1Char('\'') || last == QChar(0x3002) ||
                last == QChar(0xff0c) || last == QChar(0xff1b) ||
                last == QChar(0xff1a) || last == QChar(0xff01) ||
                last == QChar(0xff1f) || last == QChar(0xff09) ||
                last == QChar(0x3011) || last == QChar(0x300b)) {
            url.chop(1);
            continue;
        }
        break;
    }
    return url;
}

QString documentTextForLayout(QString text)
{
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);
    return text;
}

} // namespace

AiChatMessageDelegate::AiChatMessageDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void AiChatMessageDelegate::paint(QPainter* painter,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
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
            ? (dark ? QColor(0x1e, 0x86, 0xdf) : ThemeManager::instance().color(ThemeColor::Accent))
            : (dark ? QColor(0x2d, 0x31, 0x38) : ThemeManager::instance().color(ThemeColor::PanelBackground));
    const QColor textColor = isFromUser
            ? Qt::white
            : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor selectionColor = isFromUser
            ? QColor(255, 255, 255, 88)
            : (dark ? QColor(0x4c, 0x82, 0xc5, 150) : QColor(0x8b, 0xb7, 0xff, 130));

    const LayoutMetrics metrics = layoutMetrics(option, index);
    QPainterPath bubblePath;
    bubblePath.addRoundedRect(metrics.bubbleRect, kBubbleRadius, kBubbleRadius);
    painter->fillPath(bubblePath, bubbleColor);

    QTextDocument textDocument;
    configureTextDocument(textDocument,
                          text,
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
        selection.cursor = QTextCursor(&textDocument);
        selection.cursor.setPosition(selectionStart());
        selection.cursor.setPosition(selectionEnd(), QTextCursor::KeepAnchor);
        selection.format = selectionFormat;
        paintContext.selections.push_back(selection);
    }

    painter->save();
    painter->translate(metrics.textRect.topLeft());
    textDocument.documentLayout()->draw(painter, paintContext);
    painter->restore();

    painter->restore();
}

QSize AiChatMessageDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
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

    QTextDocument textDocument;
    configureTextDocument(textDocument,
                          text,
                          messageFont(),
                          metrics.textRect.width(),
                          Qt::black,
                          index.data(AiChatMessageListModel::IsFromUserRole).toBool(),
                          ThemeManager::instance().isDark());

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

    const Qt::HitTestAccuracy accuracy = allowLineWhitespace ? Qt::FuzzyHit : Qt::ExactHit;
    const int cursor = textDocument.documentLayout()->hitTest(local, accuracy);
    if (cursor < 0) {
        return -1;
    }

    return qBound(0, cursor, text.size());
}

QString AiChatMessageDelegate::urlAt(const QStyleOptionViewItem& option,
                                     const QModelIndex& index,
                                     const QPoint& viewportPos) const
{
    const int cursor = characterIndexAt(option, index, viewportPos);
    if (cursor < 0) {
        return {};
    }

    const QString text = index.data(AiChatMessageListModel::TextRole).toString();
    const QVector<TextRange> urls = urlRanges(text);
    for (const TextRange& url : urls) {
        const int end = url.start + url.length;
        if (cursor >= url.start && cursor <= end) {
            return url.text;
        }
    }

    return {};
}

void AiChatMessageDelegate::setSelection(const QModelIndex& index, int anchor, int cursor)
{
    m_selectionIndex = QPersistentModelIndex(index);
    const int textLength = index.data(AiChatMessageListModel::TextRole).toString().size();
    m_selectionAnchor = qBound(0, anchor, textLength);
    m_selectionCursor = qBound(0, cursor, textLength);
}

void AiChatMessageDelegate::clearSelection()
{
    m_selectionIndex = QPersistentModelIndex();
    m_selectionAnchor = -1;
    m_selectionCursor = -1;
}

bool AiChatMessageDelegate::hasSelection() const
{
    return m_selectionIndex.isValid() && m_selectionAnchor >= 0 &&
            m_selectionCursor >= 0 && m_selectionAnchor != m_selectionCursor;
}

bool AiChatMessageDelegate::selectionContains(const QModelIndex& index, int cursor) const
{
    if (!hasSelection() || m_selectionIndex != index || cursor < 0) {
        return false;
    }

    return cursor >= selectionStart() && cursor <= selectionEnd();
}

QString AiChatMessageDelegate::selectedText() const
{
    if (!hasSelection()) {
        return {};
    }

    const QString text = m_selectionIndex.data(AiChatMessageListModel::TextRole).toString();
    return text.mid(selectionStart(), selectionEnd() - selectionStart());
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
    document.setPlainText(documentTextForLayout(text));
    document.setTextWidth(qMax(1, textWidth));

    QTextCursor cursor(&document);
    cursor.select(QTextCursor::Document);
    QTextCharFormat textFormat;
    textFormat.setForeground(textColor);
    cursor.mergeCharFormat(textFormat);

    const QColor urlColor = linkTextColor(dark, isFromUser);
    const QVector<TextRange> urls = urlRanges(text);
    for (const TextRange& url : urls) {
        QTextCharFormat linkFormat;
        linkFormat.setForeground(urlColor);
        linkFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);

        cursor.clearSelection();
        cursor.setPosition(url.start);
        cursor.setPosition(url.start + url.length, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(linkFormat);
    }
}

QSize AiChatMessageDelegate::textDocumentSize(const QString& text,
                                              const QFont& font,
                                              int maxTextWidth) const
{
    QTextDocument textDocument;
    textDocument.setUndoRedoEnabled(false);
    textDocument.setDocumentMargin(0);
    textDocument.setDefaultFont(font);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textDocument.setDefaultTextOption(textOption);
    textDocument.setPlainText(documentTextForLayout(text));
    textDocument.setTextWidth(qMax(1, maxTextWidth));

    const int width = qMin(qCeil(textDocument.idealWidth()), maxTextWidth);
    const int height = qCeil(textDocument.size().height());

    return QSize(qCeil(width), qCeil(height));
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
        const QString url = trimmedUrlText(rawUrl);
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

int AiChatMessageDelegate::selectionStart() const
{
    return qMin(m_selectionAnchor, m_selectionCursor);
}

int AiChatMessageDelegate::selectionEnd() const
{
    return qMax(m_selectionAnchor, m_selectionCursor);
}
