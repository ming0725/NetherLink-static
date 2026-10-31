#include "markdowndelegate.h"

#include "codesyntaxhighlighter.h"
#include "markdowndocumentmodel.h"

#include "jkqtmathtext/jkqtmathtext.h"
#include "shared/services/AppFonts.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/renderers/LoadingSpinnerRenderer.h"

#include <QApplication>
#include <QCache>
#include <QDateTime>
#include <QFontDatabase>
#include <QFontInfo>
#include <QHash>
#include <QImage>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QScreen>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace {

constexpr int kHorizontalPadding = 18;
constexpr int kVerticalPadding = 6;
constexpr int kListIndent = 18;
constexpr int kNestedListIndent = 22;
constexpr int kListMarkerGap = 6;
constexpr int kTaskCheckboxTextGap = 4;
constexpr int kQuoteIndent = 16;
constexpr int kCodePadding = 10;
constexpr int kCodeBlockExtra = 4;
constexpr int kCodeHeaderHeight = 30;
constexpr int kCodeHeaderHorizontalPadding = 10;
constexpr int kCodeHeaderIconWidth = 24;
constexpr int kCodeHeaderIconSize = 14;
constexpr int kCodeHeaderSpinnerSize = 13;
constexpr int kCodeHeaderGap = 7;
constexpr int kCodeCopyButtonSize = 22;
constexpr int kCodeBlockRadius = 6;
constexpr int kSettingActionButtonSize = 24;
constexpr int kSettingBodyPadding = 14;
constexpr int kSettingControlHeight = 40;
constexpr int kSettingControlMaxWidth = 340;
constexpr int kInlineCodePaddingX = 3;
constexpr int kInlineCodePaddingY = 1;
constexpr int kInlineCodeRadius = 6;
constexpr int kTableCellPaddingX = 10;
constexpr int kTableCellPaddingY = 7;
constexpr int kTableMinRowHeight = 34;
constexpr qreal kTableMathScale = 1.24;
constexpr int kMathHorizontalPadding = 18;
constexpr int kMathVerticalPadding = 12;
constexpr qreal kMathAtomGap = 2.0;
constexpr qreal kMathDisplayScale = 1.30;
constexpr qreal kMathInlineScale = 1.16;
constexpr qreal kMathScriptScale = 0.74;
constexpr qreal kMathFractionScale = 0.95;
constexpr qreal kMathFractionGap = 5.0;
constexpr qreal kMathRuleWidth = 1.2;
constexpr qreal kMathRasterPaddingX = 4.0;
constexpr qreal kMathRasterPaddingY = 3.0;
constexpr qreal kInlineMathRasterPaddingX = 0.0;
constexpr qreal kInlineMathRasterPaddingY = 1.0;
constexpr qreal kInlineMathHorizontalPadding = 0.0;
constexpr qreal kInlineMathBaselineOffsetY = 0;
constexpr int kInlineMathListMarkerOffsetY = 2;
constexpr int kMathBlockTopPadding = 0;
constexpr int kMathBlockBottomPadding = 6;
constexpr int kDefaultFontPixelSize = 18;
constexpr int kMinFontPixelSize = 10;

const QString kSettingsBackgroundSource(QStringLiteral(":/resources/icon/options_background.png"));
const QString kMinecraftButtonSource(QStringLiteral(":/resources/icon/mc_button.png"));
const QString kMinecraftSliderSource(QStringLiteral(":/resources/icon/mc_slider.png"));
const QString kMinecraftSliderHandleSource(QStringLiteral(":/resources/icon/mc_slider_handle.png"));
const QString kSettingUndoSource(QStringLiteral(":/resources/icon/undo.svg"));
const QString kSettingReapplySource(QStringLiteral(":/resources/icon/refresh.svg"));

struct CodeBlockPalette {
    QColor background;
    QColor border;
    QColor headerBackground;
    QColor headerStickyBackground;
    QColor headerBorder;
    QColor headerText;
    QColor icon;
    QColor copyIcon;
    QColor copyIconCopied;
    QColor inlineCodeBackground;
    QColor inlineCodeText;
    QColor codeText;
    QColor selectionBackground;
    QColor tokenKeyword;
    QColor tokenTypeKeyword;
    QColor tokenFunction;
    QColor tokenComment;
    QColor tokenString;
    QColor tokenPreprocessor;
    QColor tokenNumber;
    QColor tokenOperator;
};

CodeBlockPalette codeBlockPalette()
{
    if (ThemeManager::instance().isDark()) {
        return {
            QColor(QStringLiteral("#15181d")),
            QColor(QStringLiteral("#303640")),
            QColor(QStringLiteral("#20242b")),
            QColor(QStringLiteral("#262b33")),
            QColor(QStringLiteral("#333a45")),
            QColor(QStringLiteral("#c9d1d9")),
            QColor(QStringLiteral("#d0d7de")),
            QColor(QStringLiteral("#d0d7de")),
            QColor(QStringLiteral("#63d279")),
            QColor(QStringLiteral("#303640")),
            QColor(QStringLiteral("#f0f3f6")),
            QColor(QStringLiteral("#e6edf3")),
            QColor(0x2f, 0x6f, 0xb6, 170),
            QColor(QStringLiteral("#ff7b72")),
            QColor(QStringLiteral("#d2a8ff")),
            QColor(QStringLiteral("#d2a8ff")),
            QColor(QStringLiteral("#8b949e")),
            QColor(QStringLiteral("#a5d6ff")),
            QColor(QStringLiteral("#79c0ff")),
            QColor(QStringLiteral("#ffa657")),
            QColor(QStringLiteral("#ff7b72"))
        };
    }

    return {
        QColor(QStringLiteral("#fafafa")),
        QColor(QStringLiteral("#e5e5e5")),
        QColor(QStringLiteral("#f3f3f3")),
        QColor(QStringLiteral("#f3f3f3")),
        QColor(QStringLiteral("#e5e5e5")),
        QColor(QStringLiteral("#57606a")),
        QColor(QStringLiteral("#6e7781")),
        QColor(QStringLiteral("#57606a")),
        QColor(QStringLiteral("#2da44e")),
        QColor(QStringLiteral("#ececec")),
        QColor(QStringLiteral("#000000")),
        QColor(QStringLiteral("#111111")),
        QColor(QStringLiteral("#cfe8ff")),
        QColor(QStringLiteral("#cc3f7a")),
        QColor(QStringLiteral("#6f42c1")),
        QColor(QStringLiteral("#6f42c1")),
        QColor(QStringLiteral("#808080")),
        QColor(QStringLiteral("#008a2e")),
        QColor(QStringLiteral("#0057b8")),
        QColor(QStringLiteral("#d35400")),
        QColor(QStringLiteral("#cc3f7a"))
    };
}

QColor selectionHighlightColor()
{
    return codeBlockPalette().selectionBackground;
}

QString spansSignature(const QVector<MarkdownRenderer::InlineSpan> &spans)
{
    QString signature;
    signature.reserve(spans.size() * 16);
    for (const MarkdownRenderer::InlineSpan &span : spans) {
        signature += QString::number(span.start);
        signature += QLatin1Char(':');
        signature += QString::number(span.length);
        signature += QLatin1Char(':');
        signature += QString::number(span.style);
        signature += QLatin1Char(':');
        signature += span.href;
        signature += QLatin1Char('|');
    }
    return signature;
}

QString blockSignature(const MarkdownRenderer::Block &block)
{
    QString signature;
    signature += QString::number(static_cast<int>(block.type));
    signature += QLatin1Char('|');
    signature += QString::number(block.level);
    signature += QLatin1Char('|');
    signature += QString::number(block.number);
    signature += QLatin1Char('|');
    signature += block.quote ? QLatin1Char('1') : QLatin1Char('0');
    signature += QLatin1Char('|');
    signature += block.taskListItem ? QLatin1Char('1') : QLatin1Char('0');
    signature += QLatin1Char('|');
    signature += block.taskChecked ? QLatin1Char('1') : QLatin1Char('0');
    signature += QLatin1Char('|');
    signature += block.open ? QLatin1Char('1') : QLatin1Char('0');
    signature += QLatin1Char('|');
    signature += block.language;
    signature += QLatin1Char('|');
    signature += block.text;
    signature += QLatin1Char('|');
    signature += spansSignature(block.spans);

    if (block.type == MarkdownRenderer::BlockType::Table) {
        for (MarkdownRenderer::TableAlignment alignment : block.tableAlignments) {
            signature += QString::number(static_cast<int>(alignment));
            signature += QLatin1Char(',');
        }
        signature += QLatin1Char('|');
        for (const MarkdownRenderer::TableRow &row : block.tableRows) {
            signature += row.header ? QLatin1Char('H') : QLatin1Char('B');
            for (const MarkdownRenderer::TableCell &cell : row.cells) {
                signature += cell.text;
                signature += QLatin1Char('\x1f');
                signature += cell.math ? QLatin1Char('M') : QLatin1Char('T');
                signature += cell.mathSource;
                signature += QLatin1Char('\x1f');
                signature += spansSignature(cell.spans);
                signature += QLatin1Char('\x1e');
            }
        }
    }

    if (block.type == MarkdownRenderer::BlockType::SettingBlock) {
        signature += QLatin1Char('|');
        signature += QString::number(static_cast<int>(block.settingControl));
        signature += QLatin1Char('|');
        signature += block.settingLabel;
        signature += QLatin1Char('|');
        signature += block.settingAction;
        signature += QLatin1Char('|');
        signature += block.settingValueText;
        signature += QLatin1Char('|');
        signature += block.settingPreviousValueText;
        signature += QLatin1Char('|');
        signature += block.settingPreviousLabel;
        signature += QLatin1Char('|');
        signature += QString::number(block.settingMinimum);
        signature += QLatin1Char(':');
        signature += QString::number(block.settingMaximum);
        signature += QLatin1Char(':');
        signature += QString::number(block.settingValue);
    }

    return QString::number(qHash(signature)) + QLatin1Char(':') + QString::number(signature.size());
}

QString fontSignature(const QFont &font)
{
    return font.toString();
}

int fontPixelSize(const QFont &font)
{
    if (font.pixelSize() > 0) {
        return font.pixelSize();
    }

    const int resolvedPixelSize = QFontInfo(font).pixelSize();
    return resolvedPixelSize > 0 ? resolvedPixelSize : kDefaultFontPixelSize;
}

QString sizeHintCacheKey(const QStyleOptionViewItem &option, const MarkdownRenderer::Block &block)
{
    return QString::number(qMax(80, option.rect.width())) +
           QLatin1Char('|') +
           fontSignature(option.font) +
           QLatin1Char('|') +
           blockSignature(block);
}

int deviceDpiY(const QStyleOptionViewItem &option)
{
    if (option.widget) {
        return qMax(1, option.widget->logicalDpiY());
    }
    if (QScreen *screen = QApplication::primaryScreen()) {
        return qMax(1, qRound(screen->logicalDotsPerInchY()));
    }
    return 96;
}

void setImageDpi(QImage &image, int dpiY)
{
    const int dotsPerMeter = qRound(dpiY / 0.0254);
    image.setDotsPerMeterX(dotsPerMeter);
    image.setDotsPerMeterY(dotsPerMeter);
}

qreal pointSizeForPixelSize(qreal pixelSize, int dpiY)
{
    return qMax<qreal>(1.0, pixelSize * 72.0 / qMax(1, dpiY));
}

QString mathSizeCacheKey(const QStyleOptionViewItem &option, const QString &source, qreal scale)
{
    return fontSignature(option.font) +
           QLatin1Char('|') +
           QString::number(deviceDpiY(option)) +
           QLatin1Char('|') +
           QString::number(scale, 'f', 3) +
           QLatin1Char('|') +
           source;
}

QString mathSizeCacheKey(const QFont &font, int dpiY, const QString &source, qreal scale)
{
    return fontSignature(font) +
           QLatin1Char('|') +
           QString::number(dpiY) +
           QLatin1Char('|') +
           QString::number(scale, 'f', 3) +
           QLatin1Char('|') +
           source;
}

QString mathPixmapCacheKey(const QFont &font,
                           int dpiY,
                           qreal devicePixelRatio,
                           const QColor &color,
                           const QString &source,
                           qreal scale,
                           bool displayMode)
{
    return mathSizeCacheKey(font, dpiY, source, scale) +
           QLatin1Char('|') +
           QString::number(devicePixelRatio, 'f', 3) +
           QLatin1Char('|') +
           QString::number(color.rgba(), 16) +
           QLatin1Char('|') +
           (displayMode ? QLatin1Char('d') : QLatin1Char('i'));
}

struct BlockLayout {
    std::unique_ptr<QTextLayout> textLayout;
    QRect textRect;
    qreal lineHeight = 0;
    int height = 0;
};

struct TableCellLayout {
    std::unique_ptr<QTextLayout> textLayout;
    QRect cellRect;
    QRect innerRect;
    QRect textRect;
    QRect mathRect;
    QSizeF mathSize;
    QVector<MarkdownRenderer::InlineSpan> spans;
    QString mathSource;
    QFont font;
    MarkdownRenderer::TableAlignment alignment = MarkdownRenderer::TableAlignment::None;
    int textStart = 0;
    int textLength = 0;
    int textHeight = 0;
    bool math = false;
};

struct TableRowLayout {
    std::vector<TableCellLayout> cells;
    int height = 0;
};

struct TableLayout {
    std::vector<TableRowLayout> rows;
    QRect tableRect;
    int height = 0;
};

enum class MathNodeKind {
    Row,
    Symbol,
    SupSub,
    Fraction,
    Sqrt
};

struct MathNode {
    MathNodeKind kind = MathNodeKind::Row;
    QString text;
    QVector<MathNode> children;
    int sourceStart = 0;
    int sourceEnd = 0;
};

enum class MathBoxKind {
    Container,
    Symbol,
    Fraction,
    Sqrt
};

struct MathBox {
    MathBoxKind kind = MathBoxKind::Container;
    QRectF rect;
    qreal baseline = 0.0;
    int sourceStart = 0;
    int sourceEnd = 0;
    QString text;
    QFont font;
    QVector<MathBox> children;
    QRectF ruleRect;
    QPainterPath path;
};

struct MathLayout {
    MathBox root;
    QRect textRect;
    int height = 0;
};

struct JkMathLayout {
    QString source;
    QRect textRect;
    int height = 0;
    bool parsed = false;
};

struct MathSizeCacheEntry {
    QSizeF size;
    qreal baseline = 0.0;
    bool parsed = false;
};

struct MathPixmapCacheEntry {
    QPixmap pixmap;
    QSizeF logicalSize;
    bool parsed = false;
};

struct MathLeaf {
    QRectF rect;
    int sourceStart = 0;
    int sourceEnd = 0;
};

QRect contentRect(const QStyleOptionViewItem &option);
QSizeF inlineMathRenderedSize(const QFont &font,
                              int dpiY,
                              const QString &source,
                              const QColor &color);
QSizeF inlineMathReservedSize(const QFont &font,
                              int dpiY,
                              const QString &source,
                              const QColor &color);
QString inlineMathPlaceholderText(int length);
QString textWithInlineMathPlaceholders(QString text, const QVector<MarkdownRenderer::InlineSpan> &spans);
QVector<QTextLayout::FormatRange> inlineFormatsForSpans(const QVector<MarkdownRenderer::InlineSpan> &spans,
                                                        const QFont &font,
                                                        const QColor &color,
                                                        const QStyleOptionViewItem &option);

QString layoutTextForBlock(const MarkdownRenderer::Block &block)
{
    QString text = block.text;
    if (block.type == MarkdownRenderer::BlockType::CodeBlock) {
        text.replace(QLatin1Char('\n'), QChar::LineSeparator);
    } else {
        text = textWithInlineMathPlaceholders(std::move(text), block.spans);
    }
    return text;
}

MarkdownRenderer::Block modelBlock(const QModelIndex &index)
{
    const QVariant value = index.data(MarkdownDocumentModel::BlockRole);
    if (value.canConvert<MarkdownRenderer::Block>()) {
        return value.value<MarkdownRenderer::Block>();
    }

    MarkdownRenderer::Block block;
    block.text = index.data(Qt::DisplayRole).toString();
    return block;
}

bool isQuoteDecoratedBlock(const MarkdownRenderer::Block &block)
{
    return block.type == MarkdownRenderer::BlockType::BlockQuote ||
           (block.type == MarkdownRenderer::BlockType::MathBlock && block.quote);
}

bool isQuoteDecoratedIndex(const QModelIndex &index)
{
    return index.isValid() && isQuoteDecoratedBlock(modelBlock(index));
}

bool shouldConnectQuoteBars(const MarkdownRenderer::Block &current, const MarkdownRenderer::Block &adjacent)
{
    if (!isQuoteDecoratedBlock(current) || !isQuoteDecoratedBlock(adjacent)) {
        return false;
    }
    return (current.type == MarkdownRenderer::BlockType::MathBlock && current.quote) ||
           (adjacent.type == MarkdownRenderer::BlockType::MathBlock && adjacent.quote);
}

QFont blockFont(const QFont &baseFont, const MarkdownRenderer::Block &block)
{
    QFont font = baseFont;
    const int baseSize = qMax(kMinFontPixelSize, fontPixelSize(font));

    if (block.type == MarkdownRenderer::BlockType::Heading) {
        static const double scales[] = {0.0, 2.05, 1.75, 1.48, 1.28, 1.14, 1.0};
        const int level = qBound(1, block.level, 6);
        font.setPixelSize(qRound(baseSize * scales[level]));
        font.setWeight(QFont::Bold);
    } else if (block.type == MarkdownRenderer::BlockType::CodeBlock) {
        font.setFamilies({QStringLiteral("Menlo"), QStringLiteral("Consolas"), QStringLiteral("monospace")});
        font.setPixelSize(qMax(kMinFontPixelSize, baseSize - 1));
    }

    return font;
}

QColor textColor(const QPalette &palette, const MarkdownRenderer::Block &block)
{
    if (isQuoteDecoratedBlock(block)) {
        return ThemeManager::instance().isDark()
                ? QColor(QStringLiteral("#9aa4b2"))
                : QColor(QStringLiteral("#57606a"));
    }
    if (block.type == MarkdownRenderer::BlockType::CodeBlock) {
        return codeBlockPalette().codeText;
    }
    return palette.color(QPalette::Text);
}

QTextCharFormat baseFormat(const QFont &font, const QColor &color)
{
    QTextCharFormat format;
    format.setFont(font);
    format.setForeground(color);
    return format;
}

QSizeF paddedMathSize(const QSizeF &size, qreal paddingX, qreal paddingY)
{
    if (size.isEmpty()) {
        return {};
    }
    return QSizeF(size.width() + paddingX * 2.0,
                  size.height() + paddingY * 2.0);
}

QSizeF paddedDisplayMathSize(const QSizeF &size)
{
    return paddedMathSize(size, kMathRasterPaddingX, kMathRasterPaddingY);
}

QSizeF paddedInlineMathSize(const QSizeF &size)
{
    return paddedMathSize(size, kInlineMathRasterPaddingX, kInlineMathRasterPaddingY);
}

QString inlineMathPlaceholderText(int length)
{
    return QString(qMax(0, length), QChar(0x00a0));
}

QString textWithInlineMathPlaceholders(QString text, const QVector<MarkdownRenderer::InlineSpan> &spans)
{
    for (const MarkdownRenderer::InlineSpan &span : spans) {
        if (!(span.style & MarkdownRenderer::Math) || span.length <= 0 ||
            span.start < 0 || span.start + span.length > text.size()) {
            continue;
        }
        text.replace(span.start, span.length, inlineMathPlaceholderText(span.length));
    }
    return text;
}

QFont inlineCodeFont(const QFont &font)
{
    QFont codeFont = font;
    codeFont.setFamilies({QStringLiteral("Menlo"), QStringLiteral("Consolas"), QStringLiteral("monospace")});
    codeFont.setPixelSize(qMax(kMinFontPixelSize, fontPixelSize(font) - 2));
    return codeFont;
}

QVector<QTextLayout::FormatRange> inlineFormatsForSpans(const QVector<MarkdownRenderer::InlineSpan> &spans,
                                                        const QFont &font,
                                                        const QColor &color,
                                                        const QStyleOptionViewItem &option)
{
    QVector<QTextLayout::FormatRange> ranges;

    for (const MarkdownRenderer::InlineSpan &span : spans) {
        QTextCharFormat format = baseFormat(font, color);
        if (span.style & MarkdownRenderer::Bold) {
            QFont boldFont = font;
            boldFont.setWeight(QFont::Bold);
            format.setFont(boldFont);
            format.setFontWeight(QFont::Bold);
        }
        if (span.style & MarkdownRenderer::Italic) {
            format.setFontItalic(true);
        }
        if (span.style & MarkdownRenderer::Strike) {
            format.setFontStrikeOut(true);
        }
        if (span.style & MarkdownRenderer::Code) {
            format.setFont(inlineCodeFont(font));
            format.setForeground(codeBlockPalette().inlineCodeText);
        }
        if (span.style & MarkdownRenderer::Link) {
            format.setForeground(ThemeManager::instance().color(ThemeColor::AccentLinkText));
            format.setFontUnderline(true);
        }
        if (span.style & MarkdownRenderer::Math) {
            const QSizeF renderedSize = inlineMathReservedSize(font, deviceDpiY(option), span.href, color);
            if (!renderedSize.isEmpty()) {
                QFont placeholderFont = font;
                placeholderFont.setPixelSize(span.length > 1
                                             ? 1
                                             : qMax(kMinFontPixelSize, fontPixelSize(font)));

                const qreal naturalWidth =
                        QFontMetricsF(placeholderFont).horizontalAdvance(inlineMathPlaceholderText(span.length));
                if (naturalWidth > 0.0 && span.length > 1) {
                    const qreal spacing = (renderedSize.width() - naturalWidth) / (span.length - 1);
                    placeholderFont.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
                }
                format.setFont(placeholderFont);
            }
            format.setForeground(Qt::transparent);
        }

        QTextLayout::FormatRange range;
        range.start = span.start;
        range.length = span.length;
        range.format = format;
        ranges.append(range);
    }

    return ranges;
}

void appendMathTransparencyFormats(QVector<QTextLayout::FormatRange> &ranges,
                                   const QVector<MarkdownRenderer::InlineSpan> &spans,
                                   const QFont &font,
                                   const QColor &color,
                                   const QStyleOptionViewItem &option)
{
    const QVector<QTextLayout::FormatRange> formattedRanges = inlineFormatsForSpans(spans, font, color, option);
    for (int i = 0; i < spans.size() && i < formattedRanges.size(); ++i) {
        if (spans.at(i).style & MarkdownRenderer::Math) {
            ranges.append(formattedRanges.at(i));
        }
    }
}

void drawInlineCodeBackgrounds(QPainter *painter,
                               const QTextLayout *layout,
                               const QPoint &origin,
                               const QVector<MarkdownRenderer::InlineSpan> &spans)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(codeBlockPalette().inlineCodeBackground);

    for (const MarkdownRenderer::InlineSpan &span : spans) {
        if (!(span.style & MarkdownRenderer::Code) || span.length <= 0) {
            continue;
        }

        const int spanStart = span.start;
        const int spanEnd = span.start + span.length;
        for (int lineIndex = 0; lineIndex < layout->lineCount(); ++lineIndex) {
            const QTextLine line = layout->lineAt(lineIndex);
            const int lineStart = line.textStart();
            const int lineEnd = lineStart + line.textLength();
            const int start = qMax(spanStart, lineStart);
            const int end = qMin(spanEnd, lineEnd);
            if (end <= start) {
                continue;
            }

            const qreal left = line.cursorToX(start);
            const qreal right = line.cursorToX(end);
            QRectF rect(origin.x() + line.position().x() + left - kInlineCodePaddingX,
                        origin.y() + line.position().y() + kInlineCodePaddingY,
                        right - left + kInlineCodePaddingX * 2,
                        line.height() - kInlineCodePaddingY * 2);
            painter->drawRoundedRect(rect, kInlineCodeRadius, kInlineCodeRadius);
        }
    }

    painter->restore();
}

QColor codeTokenColor(CodeSyntaxHighlighter::TokenType type)
{
    const CodeBlockPalette palette = codeBlockPalette();
    switch (type) {
    case CodeSyntaxHighlighter::TokenType::Keyword:
        return palette.tokenKeyword;
    case CodeSyntaxHighlighter::TokenType::TypeKeyword:
        return palette.tokenTypeKeyword;
    case CodeSyntaxHighlighter::TokenType::Function:
        return palette.tokenFunction;
    case CodeSyntaxHighlighter::TokenType::Comment:
        return palette.tokenComment;
    case CodeSyntaxHighlighter::TokenType::String:
        return palette.tokenString;
    case CodeSyntaxHighlighter::TokenType::Preprocessor:
        return palette.tokenPreprocessor;
    case CodeSyntaxHighlighter::TokenType::Number:
        return palette.tokenNumber;
    case CodeSyntaxHighlighter::TokenType::Operator:
        return palette.tokenOperator;
    }
    return palette.codeText;
}

QVector<QTextLayout::FormatRange> codeFormatsForBlock(const MarkdownRenderer::Block &block,
                                                      const QFont &font,
                                                      const QColor &color)
{
    QVector<QTextLayout::FormatRange> ranges;
    static QCache<QString, QVector<CodeSyntaxHighlighter::TokenSpan>> syntaxCache(4096);
    const QString cacheKey = block.language +
                             QLatin1Char('|') +
                             QString::number(qHash(block.text)) +
                             QLatin1Char(':') +
                             QString::number(block.text.size());
    QVector<CodeSyntaxHighlighter::TokenSpan> spans;
    if (const auto *cached = syntaxCache.object(cacheKey)) {
        spans = *cached;
    } else {
        spans = CodeSyntaxHighlighter::highlight(block.text, block.language);
        syntaxCache.insert(cacheKey,
                           new QVector<CodeSyntaxHighlighter::TokenSpan>(spans),
                           qMax(1, block.text.size() / 1024));
    }
    ranges.reserve(spans.size());

    for (const CodeSyntaxHighlighter::TokenSpan &span : spans) {
        QTextCharFormat format = baseFormat(font, color);
        format.setForeground(codeTokenColor(span.type));
        if (span.type == CodeSyntaxHighlighter::TokenType::Comment) {
            format.setFontItalic(true);
        }

        QTextLayout::FormatRange range;
        range.start = span.start;
        range.length = span.length;
        range.format = format;
        ranges.append(range);
    }

    return ranges;
}

void appendSelectionFormat(QVector<QTextLayout::FormatRange> &ranges,
                           int selectionStart,
                           int selectionEnd,
                           int textStart,
                           int textLength)
{
    const int localStart = qMax(selectionStart, textStart) - textStart;
    const int localEnd = qMin(selectionEnd, textStart + textLength) - textStart;
    if (selectionStart < 0 || localEnd <= localStart) {
        return;
    }

    QTextCharFormat selectionFormat;
    const QColor selectionBackground = selectionHighlightColor();
    selectionFormat.setBackground(selectionBackground);
    selectionFormat.setForeground(ThemeManager::textColorOn(selectionBackground));

    QTextLayout::FormatRange selectionRange;
    selectionRange.start = localStart;
    selectionRange.length = localEnd - localStart;
    selectionRange.format = selectionFormat;
    ranges.append(selectionRange);
}

QVector<QTextLayout::FormatRange> inlineFormats(const MarkdownRenderer::Block &block,
                                                const QFont &font,
                                                const QColor &color,
                                                const QStyleOptionViewItem &option)
{
    QVector<QTextLayout::FormatRange> ranges =
        block.type == MarkdownRenderer::BlockType::CodeBlock
            ? codeFormatsForBlock(block, font, color)
            : inlineFormatsForSpans(block.spans, font, color, option);

    const int selectionStart = option.index.data(MarkdownDocumentModel::SelectionStartRole).toInt();
    const int selectionEnd = option.index.data(MarkdownDocumentModel::SelectionEndRole).toInt();
    appendSelectionFormat(ranges, selectionStart, selectionEnd, 0, block.text.size());
    if (block.type != MarkdownRenderer::BlockType::CodeBlock) {
        appendMathTransparencyFormats(ranges, block.spans, font, color, option);
    }

    return ranges;
}

QVector<QTextLayout::FormatRange> tableCellFormats(const MarkdownRenderer::TableCell &cell,
                                                   const QFont &font,
                                                   const QColor &color,
                                                   const QStyleOptionViewItem &option,
                                                   int textStart)
{
    QVector<QTextLayout::FormatRange> ranges = inlineFormatsForSpans(cell.spans, font, color, option);
    const int selectionStart = option.index.data(MarkdownDocumentModel::SelectionStartRole).toInt();
    const int selectionEnd = option.index.data(MarkdownDocumentModel::SelectionEndRole).toInt();
    appendSelectionFormat(ranges, selectionStart, selectionEnd, textStart, cell.text.size());
    appendMathTransparencyFormats(ranges, cell.spans, font, color, option);
    return ranges;
}

QFont tableCellFont(const QFont &baseFont, bool header)
{
    QFont font = baseFont;
    const int baseSize = qMax(kMinFontPixelSize, fontPixelSize(font));
    font.setPixelSize(baseSize);
    if (header) {
        font.setWeight(QFont::DemiBold);
    }
    return font;
}

Qt::Alignment tableTextAlignment(MarkdownRenderer::TableAlignment alignment)
{
    switch (alignment) {
    case MarkdownRenderer::TableAlignment::Center:
        return Qt::AlignHCenter;
    case MarkdownRenderer::TableAlignment::Right:
        return Qt::AlignRight;
    case MarkdownRenderer::TableAlignment::Left:
    case MarkdownRenderer::TableAlignment::None:
        return Qt::AlignLeft;
    }
    return Qt::AlignLeft;
}

int alignedContentLeft(const QRect &bounds, int contentWidth, MarkdownRenderer::TableAlignment alignment)
{
    const int width = qMin(qMax(1, contentWidth), qMax(1, bounds.width()));
    switch (alignment) {
    case MarkdownRenderer::TableAlignment::Center:
        return bounds.left() + qMax(0, (bounds.width() - width) / 2);
    case MarkdownRenderer::TableAlignment::Right:
        return bounds.right() - width + 1;
    case MarkdownRenderer::TableAlignment::Left:
    case MarkdownRenderer::TableAlignment::None:
        return bounds.left();
    }
    return bounds.left();
}

int tableColumnCount(const MarkdownRenderer::Block &block)
{
    int count = block.tableAlignments.size();
    for (const MarkdownRenderer::TableRow &row : block.tableRows) {
        count = qMax(count, row.cells.size());
    }
    return count;
}

QVector<int> tableColumnWidths(int availableWidth, int columnCount)
{
    QVector<int> widths;
    if (columnCount <= 0) {
        return widths;
    }

    const int baseWidth = qMax(1, availableWidth / columnCount);
    int remainder = qMax(0, availableWidth - baseWidth * columnCount);
    widths.reserve(columnCount);
    for (int column = 0; column < columnCount; ++column) {
        int width = baseWidth;
        if (remainder > 0) {
            ++width;
            --remainder;
        }
        widths.append(width);
    }
    return widths;
}

int layoutText(QTextLayout *layout, int width)
{
    layout->beginLayout();

    qreal y = 0;
    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(qMax(20, width));
        line.setPosition(QPointF(0, y));
        y += line.height();
    }

    layout->endLayout();
    return qCeil(y);
}

QString mathCommandText(const QString &command)
{
    static const QHash<QString, QString> symbols = {
        {QStringLiteral("alpha"), QStringLiteral("α")},
        {QStringLiteral("beta"), QStringLiteral("β")},
        {QStringLiteral("gamma"), QStringLiteral("γ")},
        {QStringLiteral("delta"), QStringLiteral("δ")},
        {QStringLiteral("epsilon"), QStringLiteral("ϵ")},
        {QStringLiteral("theta"), QStringLiteral("θ")},
        {QStringLiteral("lambda"), QStringLiteral("λ")},
        {QStringLiteral("mu"), QStringLiteral("μ")},
        {QStringLiteral("pi"), QStringLiteral("π")},
        {QStringLiteral("rho"), QStringLiteral("ρ")},
        {QStringLiteral("sigma"), QStringLiteral("σ")},
        {QStringLiteral("phi"), QStringLiteral("ϕ")},
        {QStringLiteral("omega"), QStringLiteral("ω")},
        {QStringLiteral("Gamma"), QStringLiteral("Γ")},
        {QStringLiteral("Delta"), QStringLiteral("Δ")},
        {QStringLiteral("Theta"), QStringLiteral("Θ")},
        {QStringLiteral("Lambda"), QStringLiteral("Λ")},
        {QStringLiteral("Pi"), QStringLiteral("Π")},
        {QStringLiteral("Sigma"), QStringLiteral("Σ")},
        {QStringLiteral("Phi"), QStringLiteral("Φ")},
        {QStringLiteral("Omega"), QStringLiteral("Ω")},
        {QStringLiteral("int"), QStringLiteral("∫")},
        {QStringLiteral("sum"), QStringLiteral("∑")},
        {QStringLiteral("prod"), QStringLiteral("∏")},
        {QStringLiteral("infty"), QStringLiteral("∞")},
        {QStringLiteral("partial"), QStringLiteral("∂")},
        {QStringLiteral("nabla"), QStringLiteral("∇")},
        {QStringLiteral("cdot"), QStringLiteral("⋅")},
        {QStringLiteral("times"), QStringLiteral("×")},
        {QStringLiteral("pm"), QStringLiteral("±")},
        {QStringLiteral("mp"), QStringLiteral("∓")},
        {QStringLiteral("le"), QStringLiteral("≤")},
        {QStringLiteral("leq"), QStringLiteral("≤")},
        {QStringLiteral("ge"), QStringLiteral("≥")},
        {QStringLiteral("geq"), QStringLiteral("≥")},
        {QStringLiteral("neq"), QStringLiteral("≠")},
        {QStringLiteral("approx"), QStringLiteral("≈")},
        {QStringLiteral("to"), QStringLiteral("→")},
        {QStringLiteral("rightarrow"), QStringLiteral("→")},
        {QStringLiteral("leftarrow"), QStringLiteral("←")},
        {QStringLiteral("Rightarrow"), QStringLiteral("⇒")},
        {QStringLiteral("Leftarrow"), QStringLiteral("⇐")},
        {QStringLiteral("sqrt"), QStringLiteral("√")},
        {QStringLiteral("sin"), QStringLiteral("sin")},
        {QStringLiteral("cos"), QStringLiteral("cos")},
        {QStringLiteral("tan"), QStringLiteral("tan")},
        {QStringLiteral("log"), QStringLiteral("log")},
        {QStringLiteral("ln"), QStringLiteral("ln")},
        {QStringLiteral("exp"), QStringLiteral("exp")}
    };
    return symbols.value(command, command);
}

class MathParser
{
public:
    explicit MathParser(QString source)
        : m_source(std::move(source))
    {
    }

    MathNode parse()
    {
        m_position = 0;
        MathNode node = parseRow(QChar());
        node.sourceStart = 0;
        node.sourceEnd = m_source.size();
        return node;
    }

private:
    void skipSpaces()
    {
        while (m_position < m_source.size() && m_source.at(m_position).isSpace()) {
            ++m_position;
        }
    }

    MathNode parseRow(QChar terminator)
    {
        MathNode row;
        row.kind = MathNodeKind::Row;
        row.sourceStart = m_position;

        while (m_position < m_source.size()) {
            if (!terminator.isNull() && m_source.at(m_position) == terminator) {
                break;
            }

            if (m_source.at(m_position).isSpace()) {
                ++m_position;
                continue;
            }

            row.children.append(parseAtom());
        }

        row.sourceEnd = m_position;
        return row;
    }

    MathNode parseGroup()
    {
        const int start = m_position;
        ++m_position;
        MathNode group = parseRow(QLatin1Char('}'));
        if (m_position < m_source.size() && m_source.at(m_position) == QLatin1Char('}')) {
            ++m_position;
        }
        group.sourceStart = start;
        group.sourceEnd = m_position;
        return group;
    }

    MathNode parseScriptArgument()
    {
        skipSpaces();
        if (m_position < m_source.size() && m_source.at(m_position) == QLatin1Char('{')) {
            return parseGroup();
        }
        return parseBaseAtom();
    }

    MathNode parseCommand(int start)
    {
        ++m_position;
        const int commandStart = m_position;
        while (m_position < m_source.size() && m_source.at(m_position).isLetter()) {
            ++m_position;
        }
        if (commandStart == m_position && m_position < m_source.size()) {
            ++m_position;
        }

        const QString command = m_source.mid(commandStart, m_position - commandStart);
        if (command == QStringLiteral("frac")) {
            MathNode fraction;
            fraction.kind = MathNodeKind::Fraction;
            fraction.sourceStart = start;
            fraction.children.append(parseScriptArgument());
            fraction.children.append(parseScriptArgument());
            fraction.sourceEnd = m_position;
            return fraction;
        }

        if (command == QStringLiteral("sqrt")) {
            MathNode sqrt;
            sqrt.kind = MathNodeKind::Sqrt;
            sqrt.sourceStart = start;
            sqrt.children.append(parseScriptArgument());
            sqrt.sourceEnd = m_position;
            return sqrt;
        }

        if (command == QStringLiteral("left") || command == QStringLiteral("right")) {
            skipSpaces();
            if (m_position < m_source.size()) {
                MathNode delimiter;
                delimiter.kind = MathNodeKind::Symbol;
                delimiter.sourceStart = start;
                delimiter.text = m_source.mid(m_position, 1);
                ++m_position;
                delimiter.sourceEnd = m_position;
                return delimiter;
            }
        }

        MathNode symbol;
        symbol.kind = MathNodeKind::Symbol;
        symbol.text = mathCommandText(command);
        symbol.sourceStart = start;
        symbol.sourceEnd = m_position;
        return symbol;
    }

    MathNode parseBaseAtom()
    {
        if (m_position >= m_source.size()) {
            MathNode empty;
            empty.sourceStart = empty.sourceEnd = m_position;
            return empty;
        }

        if (m_source.at(m_position) == QLatin1Char('{')) {
            return parseGroup();
        }

        const int start = m_position;
        if (m_source.at(m_position) == QLatin1Char('\\')) {
            return parseCommand(start);
        }

        MathNode symbol;
        symbol.kind = MathNodeKind::Symbol;
        symbol.text = m_source.mid(m_position, 1);
        symbol.sourceStart = m_position;
        ++m_position;
        symbol.sourceEnd = m_position;
        return symbol;
    }

    MathNode parseAtom()
    {
        MathNode base = parseBaseAtom();
        MathNode sup;
        MathNode sub;
        bool hasSup = false;
        bool hasSub = false;

        while (m_position < m_source.size()) {
            const QChar marker = m_source.at(m_position);
            if (marker != QLatin1Char('^') && marker != QLatin1Char('_')) {
                break;
            }

            ++m_position;
            if (marker == QLatin1Char('^')) {
                sup = parseScriptArgument();
                hasSup = true;
            } else {
                sub = parseScriptArgument();
                hasSub = true;
            }
        }

        if (!hasSup && !hasSub) {
            return base;
        }

        MathNode node;
        node.kind = MathNodeKind::SupSub;
        node.sourceStart = base.sourceStart;
        node.sourceEnd = qMax(hasSup ? sup.sourceEnd : base.sourceEnd,
                              hasSub ? sub.sourceEnd : base.sourceEnd);
        node.children.append(base);
        node.children.append(hasSup ? sup : MathNode());
        node.children.append(hasSub ? sub : MathNode());
        return node;
    }

    QString m_source;
    int m_position = 0;
};

QString mathFontFamily()
{
    static const QString family = [] {
        const QStringList preferred = {
            QStringLiteral("KaTeX_Math"),
            QStringLiteral("KaTeX Math"),
            QStringLiteral("STIX Math"),
            QStringLiteral("STIX Two Math"),
            QStringLiteral("Latin Modern Math")
        };
        const QStringList families = QFontDatabase::families();
        for (const QString &candidate : preferred) {
            if (families.contains(candidate)) {
                return candidate;
            }
        }
        return QString();
    }();
    return family;
}

QString mathUprightFontFamily()
{
    static const QString family = [] {
        const QStringList preferred = {
            QStringLiteral("KaTeX_Main"),
            QStringLiteral("KaTeX Main"),
            QStringLiteral("KaTeX_Size1"),
            QStringLiteral("KaTeX Size1"),
            QStringLiteral("STIX Two Math"),
            QStringLiteral("STIX Math"),
            QStringLiteral("STIXGeneral"),
            QStringLiteral("Latin Modern Math"),
            QStringLiteral("Cambria Math"),
            QStringLiteral("Apple Symbols"),
            QStringLiteral("Times New Roman")
        };
        const QStringList families = QFontDatabase::families();
        for (const QString &candidate : preferred) {
            if (families.contains(candidate)) {
                return candidate;
            }
        }
        return QString();
    }();
    return family;
}

QString mathIntegralFontFamily()
{
    static const QString family = [] {
        const QStringList preferred = {
            QStringLiteral("STIXIntegralsUp"),
            QStringLiteral("STIXIntegralsUpD"),
            QStringLiteral("STIX Two Math"),
            QStringLiteral("Apple Symbols")
        };
        const QStringList families = QFontDatabase::families();
        for (const QString &candidate : preferred) {
            if (families.contains(candidate)) {
                return candidate;
            }
        }
        return QString();
    }();
    return family;
}

QFont mathFont(const QFont &baseFont, qreal scale = 1.0)
{
    QFont font = baseFont;
    const QString family = mathFontFamily();
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    font.setStyle(QFont::StyleNormal);
    font.setItalic(false);
    const qreal baseSize = qMax<qreal>(kMinFontPixelSize, fontPixelSize(baseFont));
    font.setPixelSize(qRound(qMax<qreal>(8.0, baseSize * scale)));
    return font;
}

QFont mathUprightFont(const QFont &baseFont)
{
    QFont font = QApplication::font();
    const QString family = mathUprightFontFamily();
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    font.setStyle(QFont::StyleNormal);
    font.setItalic(false);
    font.setWeight(QFont::Normal);
    font.setPixelSize(qMax(kMinFontPixelSize, fontPixelSize(baseFont)));
    return font;
}

bool isMathItalicSymbol(const QString &text)
{
    if (text.size() != 1) {
        return false;
    }

    const uint code = text.at(0).unicode();
    return (code >= 'A' && code <= 'Z') ||
           (code >= 'a' && code <= 'z') ||
           (code >= 0x0391 && code <= 0x03A9) ||
           (code >= 0x03B1 && code <= 0x03C9) ||
           code == 0x03D1 ||
           code == 0x03D5 ||
           code == 0x03D6 ||
           code == 0x03F1 ||
           code == 0x03F5;
}

bool isStretchySymbol(const QString &text)
{
    return text == QStringLiteral("(") || text == QStringLiteral(")") ||
           text == QStringLiteral("[") || text == QStringLiteral("]") ||
           text == QStringLiteral("{") || text == QStringLiteral("}") ||
           text == QStringLiteral("|");
}

bool isLargeOperatorNode(const MathNode &node)
{
    return node.kind == MathNodeKind::Symbol &&
           (node.text == QStringLiteral("∫") ||
            node.text == QStringLiteral("∑") ||
            node.text == QStringLiteral("∏"));
}

QRectF textBounds(const QString &text, const QFont &font)
{
    const QFontMetricsF metrics(font);
    return QRectF(0,
                  0,
                  qMax<qreal>(1.0, metrics.horizontalAdvance(text)),
                  metrics.ascent() + metrics.descent());
}

void moveMathBox(MathBox &box, const QPointF &topLeft)
{
    box.rect.moveTopLeft(topLeft);
}

MathBox layoutMathNode(const MathNode &node, const QFont &font);

MathBox layoutMathRow(const MathNode &node, const QFont &font)
{
    QVector<MathBox> children;
    children.reserve(node.children.size());
    qreal baseline = 0.0;
    qreal descent = 0.0;
    qreal width = 0.0;

    for (const MathNode &childNode : node.children) {
        MathBox child = layoutMathNode(childNode, font);
        if (child.rect.width() <= 0.0 && child.rect.height() <= 0.0) {
            continue;
        }
        baseline = qMax(baseline, child.baseline);
        descent = qMax(descent, child.rect.height() - child.baseline);
        width += child.rect.width() + kMathAtomGap;
        children.append(std::move(child));
    }

    if (!children.isEmpty()) {
        width -= kMathAtomGap;
    }

    MathBox box;
    box.kind = MathBoxKind::Container;
    box.sourceStart = node.sourceStart;
    box.sourceEnd = node.sourceEnd;
    box.baseline = baseline;
    box.rect = QRectF(0, 0, qMax<qreal>(1.0, width), qMax<qreal>(1.0, baseline + descent));

    qreal x = 0.0;
    for (MathBox &child : children) {
        moveMathBox(child, QPointF(x, baseline - child.baseline));
        x += child.rect.width() + kMathAtomGap;
        box.children.append(std::move(child));
    }
    return box;
}

MathBox layoutMathSymbol(const MathNode &node, const QFont &font)
{
    QFont symbolFont = isMathItalicSymbol(node.text) ? font : mathUprightFont(font);
    if (node.text == QStringLiteral("∫")) {
        const QString integralFamily = mathIntegralFontFamily();
        if (!integralFamily.isEmpty()) {
            symbolFont.setFamily(integralFamily);
        }
    }
    if (isStretchySymbol(node.text)) {
        symbolFont.setPixelSize(qRound(fontPixelSize(font) * 1.08));
    }

    const QFontMetricsF metrics(symbolFont);
    MathBox box;
    box.kind = MathBoxKind::Symbol;
    box.text = node.text;
    box.font = symbolFont;
    box.sourceStart = node.sourceStart;
    box.sourceEnd = node.sourceEnd;
    box.baseline = metrics.ascent();
    box.rect = textBounds(node.text, symbolFont);
    return box;
}

MathBox layoutMathSupSub(const MathNode &node, const QFont &font)
{
    MathBox base = layoutMathNode(node.children.value(0), font);
    MathBox sup = layoutMathNode(node.children.value(1), mathFont(font, kMathScriptScale));
    MathBox sub = layoutMathNode(node.children.value(2), mathFont(font, kMathScriptScale));
    const bool hasSup = node.children.size() > 1 && node.children.at(1).sourceEnd > node.children.at(1).sourceStart;
    const bool hasSub = node.children.size() > 2 && node.children.at(2).sourceEnd > node.children.at(2).sourceStart;

    const qreal scriptWidth = qMax(hasSup ? sup.rect.width() : 0.0,
                                   hasSub ? sub.rect.width() : 0.0);
    const bool largeOperator = isLargeOperatorNode(node.children.value(0));
    const qreal baseY = largeOperator && hasSup ? qMax<qreal>(0.0, sup.rect.height() * 0.58) : 0.0;
    const qreal supY = largeOperator
                           ? 0.0
                           : qMax<qreal>(0.0, base.baseline - sup.baseline - base.rect.height() * 0.30);
    const qreal subY = baseY + base.baseline + qMax<qreal>(2.0, sub.baseline * 0.20);
    const qreal height = qMax(qMax(baseY + base.rect.height(),
                                   hasSup ? supY + sup.rect.height() : 0.0),
                              hasSub ? subY + sub.rect.height() : 0.0);

    MathBox box;
    box.kind = MathBoxKind::Container;
    box.sourceStart = node.sourceStart;
    box.sourceEnd = node.sourceEnd;
    box.baseline = baseY + base.baseline;
    box.rect = QRectF(0, 0, base.rect.width() + scriptWidth + 1.0, height);

    moveMathBox(base, QPointF(0, baseY));
    box.children.append(std::move(base));
    if (hasSup) {
        moveMathBox(sup, QPointF(box.children.first().rect.width() + 1.0, supY));
        box.children.append(std::move(sup));
    }
    if (hasSub) {
        moveMathBox(sub, QPointF(box.children.first().rect.width() + 1.0, subY));
        box.children.append(std::move(sub));
    }
    return box;
}

MathBox layoutMathFraction(const MathNode &node, const QFont &font)
{
    MathBox numerator = layoutMathNode(node.children.value(0), mathFont(font, kMathFractionScale));
    MathBox denominator = layoutMathNode(node.children.value(1), mathFont(font, kMathFractionScale));
    const qreal width = qMax(numerator.rect.width(), denominator.rect.width()) + 12.0;
    const qreal ruleY = numerator.rect.height() + kMathFractionGap;
    const qreal denominatorY = ruleY + kMathRuleWidth + kMathFractionGap;

    MathBox box;
    box.kind = MathBoxKind::Fraction;
    box.sourceStart = node.sourceStart;
    box.sourceEnd = node.sourceEnd;
    box.baseline = denominatorY + denominator.baseline;
    box.rect = QRectF(0, 0, width, denominatorY + denominator.rect.height());
    box.ruleRect = QRectF(0, ruleY, width, kMathRuleWidth);

    moveMathBox(numerator, QPointF((width - numerator.rect.width()) / 2.0, 0));
    moveMathBox(denominator, QPointF((width - denominator.rect.width()) / 2.0, denominatorY));
    box.children.append(std::move(numerator));
    box.children.append(std::move(denominator));
    return box;
}

MathBox layoutMathSqrt(const MathNode &node, const QFont &font)
{
    MathBox radicand = layoutMathNode(node.children.value(0), font);
    const qreal leftWidth = 12.0;
    const qreal topPadding = 3.0;
    const qreal width = leftWidth + radicand.rect.width() + 4.0;
    const qreal height = radicand.rect.height() + topPadding + 2.0;

    MathBox box;
    box.kind = MathBoxKind::Sqrt;
    box.sourceStart = node.sourceStart;
    box.sourceEnd = node.sourceEnd;
    box.baseline = topPadding + radicand.baseline;
    box.rect = QRectF(0, 0, width, height);

    QPainterPath path;
    path.moveTo(1.0, height * 0.58);
    path.lineTo(leftWidth * 0.34, height * 0.58);
    path.lineTo(leftWidth * 0.55, height - 2.0);
    path.lineTo(leftWidth, 1.0);
    path.lineTo(width - 1.0, 1.0);
    box.path = path;

    moveMathBox(radicand, QPointF(leftWidth, topPadding));
    box.children.append(std::move(radicand));
    return box;
}

MathBox layoutMathNode(const MathNode &node, const QFont &font)
{
    switch (node.kind) {
    case MathNodeKind::Symbol:
        return layoutMathSymbol(node, font);
    case MathNodeKind::SupSub:
        return layoutMathSupSub(node, font);
    case MathNodeKind::Fraction:
        return layoutMathFraction(node, font);
    case MathNodeKind::Sqrt:
        return layoutMathSqrt(node, font);
    case MathNodeKind::Row:
        return layoutMathRow(node, font);
    }
    return {};
}

void drawMathSelection(QPainter *painter,
                       const MathBox &box,
                       const QPointF &origin,
                       int selectionStart,
                       int selectionEnd)
{
    if (selectionStart < 0 || selectionEnd <= selectionStart ||
        box.sourceEnd <= selectionStart || box.sourceStart >= selectionEnd) {
        return;
    }

    const QPointF boxOrigin = origin + box.rect.topLeft();
    if (box.kind == MathBoxKind::Symbol && box.sourceEnd > box.sourceStart) {
        painter->fillRect(QRectF(boxOrigin, box.rect.size()),
                          selectionHighlightColor());
        return;
    }

    for (const MathBox &child : box.children) {
        drawMathSelection(painter, child, boxOrigin, selectionStart, selectionEnd);
    }
}

void drawMathBox(QPainter *painter, const MathBox &box, const QPointF &origin, const QColor &color)
{
    const QPointF boxOrigin = origin + box.rect.topLeft();
    painter->save();
    painter->setPen(color);

    if (box.kind == MathBoxKind::Symbol) {
        painter->setFont(box.font);
        painter->drawText(boxOrigin + QPointF(0, box.baseline), box.text);
    } else if (box.kind == MathBoxKind::Fraction) {
        painter->fillRect(QRectF(boxOrigin + box.ruleRect.topLeft(), box.ruleRect.size()), color);
    } else if (box.kind == MathBoxKind::Sqrt) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(color, kMathRuleWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->drawPath(box.path.translated(boxOrigin));
    }

    for (const MathBox &child : box.children) {
        drawMathBox(painter, child, boxOrigin, color);
    }
    painter->restore();
}

void collectMathLeaves(const MathBox &box, const QPointF &origin, QVector<MathLeaf> *leaves)
{
    const QPointF boxOrigin = origin + box.rect.topLeft();
    if (box.kind == MathBoxKind::Symbol && box.sourceEnd > box.sourceStart) {
        MathLeaf leaf;
        leaf.rect = QRectF(boxOrigin, box.rect.size());
        leaf.sourceStart = box.sourceStart;
        leaf.sourceEnd = box.sourceEnd;
        leaves->append(leaf);
        return;
    }

    for (const MathBox &child : box.children) {
        collectMathLeaves(child, boxOrigin, leaves);
    }
}

int mathCursorForPosition(const MathLayout &layout, const QPoint &position)
{
    QVector<MathLeaf> leaves;
    collectMathLeaves(layout.root, layout.textRect.topLeft(), &leaves);
    if (leaves.isEmpty()) {
        return 0;
    }

    qreal bestDistance = std::numeric_limits<qreal>::max();
    int bestCursor = 0;
    for (const MathLeaf &leaf : leaves) {
        if (leaf.rect.contains(position)) {
            return position.x() < leaf.rect.center().x() ? leaf.sourceStart : leaf.sourceEnd;
        }

        const QPointF center = leaf.rect.center();
        const qreal dx = center.x() - position.x();
        const qreal dy = center.y() - position.y();
        const qreal distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestCursor = position.x() < center.x() ? leaf.sourceStart : leaf.sourceEnd;
        }
    }
    return bestCursor;
}

bool mathHasTextAtPosition(const MathLayout &layout, const QPoint &position)
{
    return layout.textRect.adjusted(-3, -3, 3, 3).contains(position);
}

MathLayout buildMathLayout(const QStyleOptionViewItem &option, const MarkdownRenderer::Block &block)
{
    const QRect content = contentRect(option);
    const QFont font = mathFont(option.font, kMathDisplayScale);
    MathParser parser(block.text);
    MathBox root = layoutMathNode(parser.parse(), font);

    const int width = qCeil(root.rect.width());
    const int x = content.left() + qMax(0, (content.width() - width) / 2);
    const int y = content.top() + kMathBlockTopPadding;

    MathLayout layout;
    layout.root = std::move(root);
    layout.textRect = QRect(x, y, width, qCeil(layout.root.rect.height()));
    layout.height = layout.textRect.height() + kMathVerticalPadding * 2;
    return layout;
}

QString normalizedDisplayMathSource(QString source)
{
    source = source.trimmed();

    bool stripped = true;
    while (stripped) {
        stripped = false;
        if (source.startsWith(QStringLiteral("\\[")) && source.endsWith(QStringLiteral("\\]"))) {
            source = source.mid(2, source.size() - 4).trimmed();
            stripped = true;
        } else if (source.startsWith(QStringLiteral("\\(")) && source.endsWith(QStringLiteral("\\)"))) {
            source = source.mid(2, source.size() - 4).trimmed();
            stripped = true;
        } else if (source.startsWith(QStringLiteral("$$")) && source.endsWith(QStringLiteral("$$"))) {
            source = source.mid(2, source.size() - 4).trimmed();
            stripped = true;
        } else if (source.startsWith(QLatin1Char('$')) && source.endsWith(QLatin1Char('$')) && source.size() >= 2) {
            source = source.mid(1, source.size() - 2).trimmed();
            stripped = true;
        }
    }

    static const QRegularExpression labelExpression(
            QStringLiteral(R"(\\label\s*\{[^{}]*\})"));
    static const QRegularExpression unnumberedExpression(
            QStringLiteral(R"(\\(?:notag|nonumber)\b)"));
    source.remove(labelExpression);
    source.remove(unnumberedExpression);
    source = source.trimmed();

    const auto stripEnvironment = [](const QString &input, const QString &name) -> QString {
        const QString escapedName = QRegularExpression::escape(name);
        const QRegularExpression environmentExpression(
                QStringLiteral(R"(^\\begin\{%1\}(.*)\\end\{%1\}$)").arg(escapedName),
                QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch match = environmentExpression.match(input);
        return match.hasMatch() ? match.captured(1).trimmed() : input;
    };

    bool strippedEnvironment = true;
    while (strippedEnvironment) {
        strippedEnvironment = false;
        const QString before = source;
        source = stripEnvironment(source, QStringLiteral("equation"));
        source = stripEnvironment(source, QStringLiteral("equation*"));
        strippedEnvironment = source != before;
    }

    return source;
}

QString xitsMathFamilyFallback()
{
    static const QString family = [] {
        const QStringList families = QFontDatabase::families();
        const auto xits = std::find_if(families.cbegin(), families.cend(), [](const QString &candidate) {
            return candidate.contains(QStringLiteral("XITS Math"));
        });
        return xits == families.cend() ? QString() : *xits;
    }();
    return family;
}

void configureMathText(JKQTMathText &mathText,
                       const QFont &font,
                       int dpiY,
                       const QColor &color,
                       qreal scale = kMathDisplayScale)
{
    const qreal baseSize = qMax<qreal>(kMinFontPixelSize, fontPixelSize(font));
    mathText.useXITS();
    QString caligraphicFont = mathText.getFallbackFontSymbols();
    if (caligraphicFont.isEmpty()) {
        caligraphicFont = xitsMathFamilyFallback();
    }
    if (!caligraphicFont.isEmpty()) {
        mathText.setFontCaligraphic(caligraphicFont, JKQTMathTextFontEncoding::MTFEUnicode);
    }
    mathText.setFontSize(pointSizeForPixelSize(baseSize * scale, dpiY));
    mathText.setFontColor(color);
}

void configureMathText(JKQTMathText &mathText,
                       const QStyleOptionViewItem &option,
                       const QColor &color,
                       qreal scale = kMathDisplayScale)
{
    configureMathText(mathText, option.font, deviceDpiY(option), color, scale);
}

bool parseDisplayMath(JKQTMathText &mathText, const QString &source)
{
    const QString latex = QStringLiteral("\\displaystyle ") + normalizedDisplayMathSource(source);
    return mathText.parse(latex,
                          JKQTMathText::LatexParser,
                          JKQTMathText::ParseOptions(JKQTMathText::StartWithMathMode |
                                                     JKQTMathText::AllowLinebreaks));
}

QSizeF displayMathRenderedSize(const QFont &font,
                               int dpiY,
                               const QString &source,
                               const QColor &color,
                               qreal scale,
                               bool *parsed)
{
    static QCache<QString, MathSizeCacheEntry> cache(2048);
    const QString cacheKey = mathSizeCacheKey(font, dpiY, normalizedDisplayMathSource(source), scale);
    if (const MathSizeCacheEntry *cached = cache.object(cacheKey)) {
        *parsed = cached->parsed;
        return cached->size;
    }

    QImage metricImage(1, 1, QImage::Format_ARGB32_Premultiplied);
    setImageDpi(metricImage, dpiY);
    metricImage.fill(Qt::transparent);
    QPainter metricPainter(&metricImage);

    JKQTMathText mathText;
    configureMathText(mathText, font, dpiY, color, scale);
    *parsed = parseDisplayMath(mathText, source);
    const QSizeF formulaSize = *parsed ? paddedDisplayMathSize(mathText.getSize(metricPainter)) : QSizeF();
    metricPainter.end();

    auto *entry = new MathSizeCacheEntry;
    entry->size = formulaSize;
    entry->parsed = *parsed;
    cache.insert(cacheKey, entry, qMax(1, source.size() / 64));
    return formulaSize;
}

QString normalizedInlineMathSource(QString source)
{
    source = source.trimmed();
    if (source.startsWith(QStringLiteral("\\(")) && source.endsWith(QStringLiteral("\\)"))) {
        source = source.mid(2, source.size() - 4).trimmed();
    } else if (source.startsWith(QLatin1Char('$')) && source.endsWith(QLatin1Char('$')) && source.size() >= 2) {
        source = source.mid(1, source.size() - 2).trimmed();
    }
    return source;
}

bool parseInlineMath(JKQTMathText &mathText, const QString &source)
{
    return mathText.parse(normalizedInlineMathSource(source),
                          JKQTMathText::LatexParser,
                          JKQTMathText::ParseOptions(JKQTMathText::StartWithMathMode));
}

QPixmap renderMathPixmap(const QFont &font,
                         int dpiY,
                         qreal devicePixelRatio,
                         const QString &source,
                         const QColor &color,
                         qreal scale,
                         bool displayMode,
                         QSizeF *logicalSize,
                         bool *parsed)
{
    static QCache<QString, MathPixmapCacheEntry> cache(32768);
    const QString normalizedSource = displayMode
                                         ? normalizedDisplayMathSource(source)
                                         : normalizedInlineMathSource(source);
    const QString cacheKey = mathPixmapCacheKey(font,
                                                dpiY,
                                                devicePixelRatio,
                                                color,
                                                normalizedSource,
                                                scale,
                                                displayMode);
    if (const MathPixmapCacheEntry *cached = cache.object(cacheKey)) {
        *logicalSize = cached->logicalSize;
        *parsed = cached->parsed;
        return cached->pixmap;
    }

    JKQTMathText mathText;
    configureMathText(mathText, font, dpiY, color, scale);
    *parsed = displayMode ? parseDisplayMath(mathText, source)
                          : parseInlineMath(mathText, source);
    if (!*parsed) {
        auto *entry = new MathPixmapCacheEntry;
        entry->parsed = false;
        cache.insert(cacheKey, entry, qMax(1, source.size() / 64));
        *logicalSize = QSizeF();
        return {};
    }

    QImage metricImage(1, 1, QImage::Format_ARGB32_Premultiplied);
    metricImage.setDevicePixelRatio(devicePixelRatio);
    setImageDpi(metricImage, dpiY);
    metricImage.fill(Qt::transparent);
    QPainter metricPainter(&metricImage);
    metricPainter.setRenderHint(QPainter::Antialiasing, true);
    metricPainter.setRenderHint(QPainter::TextAntialiasing, true);
    const JKQTMathTextNodeSize formulaSizeDetail = mathText.getSizeDetail(metricPainter);
    const QSizeF formulaSize = formulaSizeDetail.getSize();
    const qreal paddingX = displayMode ? kMathRasterPaddingX : kInlineMathRasterPaddingX;
    const qreal paddingY = displayMode ? kMathRasterPaddingY : kInlineMathRasterPaddingY;
    const QSizeF paddedSize = paddedMathSize(formulaSize, paddingX, paddingY);
    metricPainter.end();

    if (paddedSize.isEmpty()) {
        auto *entry = new MathPixmapCacheEntry;
        entry->parsed = true;
        cache.insert(cacheKey, entry, qMax(1, source.size() / 64));
        *logicalSize = QSizeF();
        return {};
    }

    const qreal dpr = qMax<qreal>(1.0, devicePixelRatio);
    QImage image(qMax(1, qCeil(paddedSize.width() * dpr)),
                 qMax(1, qCeil(paddedSize.height() * dpr)),
                 QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    setImageDpi(image, dpiY);
    image.fill(Qt::transparent);

    QPainter imagePainter(&image);
    imagePainter.setRenderHint(QPainter::Antialiasing, true);
    imagePainter.setRenderHint(QPainter::TextAntialiasing, true);
    imagePainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    mathText.draw(imagePainter,
                  paddingX,
                  paddingY + formulaSizeDetail.baselineHeight,
                  false);
    imagePainter.end();

    const QPixmap pixmap = QPixmap::fromImage(image);

    auto *entry = new MathPixmapCacheEntry;
    entry->pixmap = pixmap;
    entry->logicalSize = paddedSize;
    entry->parsed = true;
    const int cacheCost = qMax(1,
                               qCeil(paddedSize.width() * paddedSize.height() * dpr * dpr / 4096.0));
    cache.insert(cacheKey, entry, cacheCost);

    *logicalSize = paddedSize;
    return pixmap;
}

QSizeF inlineMathSize(const QFont &font,
                      int dpiY,
                      const QString &source,
                      const QColor &color,
                      bool *parsed,
                      qreal *baseline = nullptr)
{
    static QCache<QString, MathSizeCacheEntry> cache(2048);
    const QString cacheKey = mathSizeCacheKey(font, dpiY, normalizedInlineMathSource(source), kMathInlineScale);
    if (const MathSizeCacheEntry *cached = cache.object(cacheKey)) {
        *parsed = cached->parsed;
        if (baseline) {
            *baseline = cached->baseline;
        }
        return cached->size;
    }

    QImage metricImage(1, 1, QImage::Format_ARGB32_Premultiplied);
    setImageDpi(metricImage, dpiY);
    metricImage.fill(Qt::transparent);
    QPainter metricPainter(&metricImage);

    JKQTMathText mathText;
    configureMathText(mathText, font, dpiY, color, kMathInlineScale);
    *parsed = parseInlineMath(mathText, source);
    QSizeF formulaSize;
    qreal formulaBaseline = 0.0;
    if (*parsed) {
        const JKQTMathTextNodeSize formulaSizeDetail = mathText.getSizeDetail(metricPainter);
        formulaSize = paddedInlineMathSize(formulaSizeDetail.getSize());
        formulaBaseline = kInlineMathRasterPaddingY + formulaSizeDetail.baselineHeight;
    }
    metricPainter.end();

    auto *entry = new MathSizeCacheEntry;
    entry->size = formulaSize;
    entry->baseline = formulaBaseline;
    entry->parsed = *parsed;
    cache.insert(cacheKey, entry, qMax(1, source.size() / 64));
    if (baseline) {
        *baseline = formulaBaseline;
    }
    return formulaSize;
}

QSizeF inlineMathRenderedSize(const QFont &font,
                              int dpiY,
                              const QString &source,
                              const QColor &color)
{
    bool parsed = false;
    const QSizeF formulaSize = inlineMathSize(font, dpiY, source, color, &parsed);
    return parsed ? formulaSize : QSizeF();
}

QSizeF inlineMathReservedSize(const QFont &font,
                              int dpiY,
                              const QString &source,
                              const QColor &color)
{
    const QSizeF formulaSize = inlineMathRenderedSize(font, dpiY, source, color);
    if (formulaSize.isEmpty()) {
        return {};
    }
    return QSizeF(formulaSize.width() + kInlineMathHorizontalPadding * 2.0,
                  formulaSize.height());
}

struct InlineMathLineInsets {
    qreal top = 0.0;
    qreal bottom = 0.0;
};

InlineMathLineInsets inlineMathLineInsets(const QTextLine &line,
                                          const QVector<MarkdownRenderer::InlineSpan> &spans,
                                          const QFont &font,
                                          int dpiY,
                                          const QColor &color)
{
    InlineMathLineInsets insets;
    const int lineStart = line.textStart();
    const int lineEnd = lineStart + line.textLength();
    for (const MarkdownRenderer::InlineSpan &span : spans) {
        if (!(span.style & MarkdownRenderer::Math) || span.length <= 0) {
            continue;
        }
        const int spanStart = span.start;
        const int spanEnd = span.start + span.length;
        if (spanStart < lineStart || spanEnd > lineEnd) {
            continue;
        }

        bool parsed = false;
        qreal formulaBaseline = 0.0;
        const QSizeF formulaSize = inlineMathSize(font,
                                                  dpiY,
                                                  span.href,
                                                  color,
                                                  &parsed,
                                                  &formulaBaseline);
        if (!parsed || formulaSize.isEmpty()) {
            continue;
        }

        insets.top = qMax(insets.top, formulaBaseline - line.ascent());
        insets.bottom = qMax(insets.bottom, formulaSize.height() - formulaBaseline - line.descent());
    }
    insets.top = qMax<qreal>(0.0, insets.top);
    insets.bottom = qMax<qreal>(0.0, insets.bottom);
    return insets;
}

void drawInlineMath(QPainter *painter,
                    const QStyleOptionViewItem &option,
                    const QTextLayout *layout,
                    const QPoint &origin,
                    const QVector<MarkdownRenderer::InlineSpan> &spans,
                    const QColor &color)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const int dpiY = painter->device() ? qMax(1, painter->device()->logicalDpiY()) : deviceDpiY(option);

    for (const MarkdownRenderer::InlineSpan &span : spans) {
        if (!(span.style & MarkdownRenderer::Math) || span.length <= 0) {
            continue;
        }

        const int spanStart = span.start;
        const int spanEnd = span.start + span.length;
        for (int lineIndex = 0; lineIndex < layout->lineCount(); ++lineIndex) {
            const QTextLine line = layout->lineAt(lineIndex);
            const int lineStart = line.textStart();
            const int lineEnd = lineStart + line.textLength();
            if (spanStart < lineStart || spanEnd > lineEnd) {
                continue;
            }

            const qreal left = line.cursorToX(spanStart);
            const qreal right = line.cursorToX(spanEnd);
            const qreal reservedWidth = qMax<qreal>(1.0, right - left);
            bool parsed = false;
            qreal formulaBaseline = 0.0;
            const QFont layoutFont = layout->font();
            const QSizeF formulaSize = inlineMathSize(layoutFont,
                                                      dpiY,
                                                      span.href,
                                                      color,
                                                      &parsed,
                                                      &formulaBaseline);
            if (!parsed || formulaSize.isEmpty()) {
                painter->setFont(option.font);
                painter->setPen(color);
                painter->drawText(QRectF(origin.x() + line.position().x() + left,
                                         origin.y() + line.position().y(),
                                         reservedWidth,
                                         line.height()),
                                  Qt::AlignVCenter | Qt::AlignLeft,
                                  normalizedInlineMathSource(span.href));
                break;
            }

            QSizeF pixmapSize;
            const qreal devicePixelRatio = painter->device()
                                               ? qMax<qreal>(1.0, painter->device()->devicePixelRatioF())
                                               : 1.0;
            const QPixmap pixmap = renderMathPixmap(layoutFont,
                                                    dpiY,
                                                    devicePixelRatio,
                                                    span.href,
                                                    color,
                                                    kMathInlineScale,
                                                    false,
                                                    &pixmapSize,
                                                    &parsed);
            if (!parsed || pixmap.isNull() || pixmapSize.isEmpty()) {
                painter->setFont(option.font);
                painter->setPen(color);
                painter->drawText(QRectF(origin.x() + line.position().x() + left,
                                         origin.y() + line.position().y(),
                                         reservedWidth,
                                         line.height()),
                                  Qt::AlignVCenter | Qt::AlignLeft,
                                  normalizedInlineMathSource(span.href));
                break;
            }

            const qreal formulaTop = origin.y() +
                                     line.position().y() +
                                     line.ascent() -
                                     formulaBaseline +
                                     kInlineMathBaselineOffsetY;
            const qreal formulaLeft = origin.x() +
                                      line.position().x() +
                                      left +
                                      qMax<qreal>(0.0, (reservedWidth - pixmapSize.width()) / 2.0);
            const QRectF formulaRect(formulaLeft,
                                     formulaTop,
                                     pixmapSize.width(),
                                     pixmapSize.height());
            painter->save();
            painter->setClipRect(formulaRect);
            painter->drawPixmap(formulaRect.topLeft(), pixmap);
            painter->restore();
            break;
        }
    }

    painter->restore();
}

JkMathLayout buildJkMathLayout(const QStyleOptionViewItem &option, const MarkdownRenderer::Block &block)
{
    QRect content = contentRect(option);
    if (block.quote) {
        content.adjust(kQuoteIndent, 0, 0, 0);
    }
    const QColor color = textColor(option.palette, block);

    static QCache<QString, MathSizeCacheEntry> cache(1024);
    const QString cacheKey = mathSizeCacheKey(option, normalizedDisplayMathSource(block.text), kMathDisplayScale);
    bool parsed = false;
    QSizeF formulaSize;
    if (const MathSizeCacheEntry *cached = cache.object(cacheKey)) {
        parsed = cached->parsed;
        formulaSize = cached->size;
    } else {
        QImage metricImage(1, 1, QImage::Format_ARGB32_Premultiplied);
        setImageDpi(metricImage, deviceDpiY(option));
        metricImage.fill(Qt::transparent);
        QPainter metricPainter(&metricImage);

        JKQTMathText mathText;
        configureMathText(mathText, option, color);
        parsed = parseDisplayMath(mathText, block.text);
        formulaSize = parsed ? paddedDisplayMathSize(mathText.getSize(metricPainter)) : QSizeF();
        metricPainter.end();

        auto *entry = new MathSizeCacheEntry;
        entry->size = formulaSize;
        entry->parsed = parsed;
        cache.insert(cacheKey, entry, qMax(1, block.text.size() / 64));
    }

    if (!parsed || formulaSize.isEmpty()) {
        const QFontMetricsF metrics(option.font);
        const QRectF fallbackBounds = metrics.boundingRect(normalizedDisplayMathSource(block.text));
        formulaSize = QSizeF(qMax<qreal>(1.0, fallbackBounds.width()),
                             qMax<qreal>(metrics.height(), fallbackBounds.height()));
    }

    const int width = qCeil(qMax<qreal>(1.0, formulaSize.width()));
    const int height = qCeil(qMax<qreal>(1.0, formulaSize.height()));
    const int x = content.left() + qMax(0, (content.width() - width) / 2);
    const int y = content.top() + kMathBlockTopPadding;

    JkMathLayout layout;
    layout.source = block.text;
    layout.parsed = parsed;
    layout.textRect = QRect(x, y, width, height);
    layout.height = height + kMathBlockTopPadding + kMathBlockBottomPadding;
    return layout;
}

void drawJkMathSelection(QPainter *painter,
                         const JkMathLayout &layout,
                         int selectionStart,
                         int selectionEnd)
{
    if (selectionStart < 0 || selectionEnd <= selectionStart) {
        return;
    }

    const int sourceLength = layout.source.size();
    if (sourceLength <= 0) {
        return;
    }

    const int start = qBound(0, selectionStart, sourceLength);
    const int end = qBound(0, selectionEnd, sourceLength);
    if (end <= start) {
        return;
    }

    const qreal leftRatio = static_cast<qreal>(start) / sourceLength;
    const qreal rightRatio = static_cast<qreal>(end) / sourceLength;
    const QRectF selectionRect(layout.textRect.left() + layout.textRect.width() * leftRatio,
                               layout.textRect.top() - 2,
                               qMax<qreal>(1.0, layout.textRect.width() * (rightRatio - leftRatio)),
                               layout.textRect.height() + 4);

    painter->fillRect(selectionRect,
                      selectionHighlightColor());
}

void drawJkMath(QPainter *painter,
                const QStyleOptionViewItem &option,
                const MarkdownRenderer::Block &block,
                const JkMathLayout &layout)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    const QColor color = textColor(option.palette, block);
    painter->setPen(color);

    if (!layout.parsed) {
        painter->drawText(layout.textRect, Qt::AlignCenter, normalizedDisplayMathSource(block.text));
        painter->restore();
        return;
    }

    QSizeF pixmapSize;
    bool parsed = false;
    const int dpiY = painter->device() ? qMax(1, painter->device()->logicalDpiY()) : deviceDpiY(option);
    const qreal devicePixelRatio = painter->device()
                                       ? qMax<qreal>(1.0, painter->device()->devicePixelRatioF())
                                       : 1.0;
    const QPixmap pixmap = renderMathPixmap(option.font,
                                            dpiY,
                                            devicePixelRatio,
                                            block.text,
                                            color,
                                            kMathDisplayScale,
                                            true,
                                            &pixmapSize,
                                            &parsed);
    if (!parsed || pixmap.isNull() || pixmapSize.isEmpty()) {
        painter->drawText(layout.textRect, Qt::AlignCenter, normalizedDisplayMathSource(block.text));
        painter->restore();
        return;
    }

    const QPointF topLeft(layout.textRect.left() + (layout.textRect.width() - pixmapSize.width()) / 2.0,
                          layout.textRect.top() + (layout.textRect.height() - pixmapSize.height()) / 2.0);
    painter->drawPixmap(topLeft, pixmap);
    painter->restore();
}

void drawTableCellMath(QPainter *painter,
                       const QStyleOptionViewItem &option,
                       const TableCellLayout &cell)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    const QColor color = option.palette.color(QPalette::Text);
    painter->setPen(color);

    QSizeF pixmapSize;
    bool parsed = false;
    const int dpiY = painter->device() ? qMax(1, painter->device()->logicalDpiY()) : deviceDpiY(option);
    const qreal devicePixelRatio = painter->device()
                                       ? qMax<qreal>(1.0, painter->device()->devicePixelRatioF())
                                       : 1.0;
    const QPixmap pixmap = renderMathPixmap(cell.font,
                                            dpiY,
                                            devicePixelRatio,
                                            cell.mathSource,
                                            color,
                                            kTableMathScale,
                                            true,
                                            &pixmapSize,
                                            &parsed);
    if (!parsed || pixmap.isNull() || pixmapSize.isEmpty()) {
        painter->drawText(cell.textRect, Qt::AlignCenter, normalizedDisplayMathSource(cell.mathSource));
        painter->restore();
        return;
    }

    const QPointF topLeft(cell.mathRect.left() + (cell.mathRect.width() - pixmapSize.width()) / 2.0,
                          cell.mathRect.top() + (cell.mathRect.height() - pixmapSize.height()) / 2.0);
    painter->drawPixmap(topLeft, pixmap);
    painter->restore();
}

void drawTableCellMathSelection(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const TableCellLayout &cell)
{
    const int selectionStart = option.index.data(MarkdownDocumentModel::SelectionStartRole).toInt();
    const int selectionEnd = option.index.data(MarkdownDocumentModel::SelectionEndRole).toInt();
    if (selectionStart < 0 || selectionEnd <= selectionStart || cell.textLength <= 0) {
        return;
    }

    const int localStart = qMax(selectionStart, cell.textStart) - cell.textStart;
    const int localEnd = qMin(selectionEnd, cell.textStart + cell.textLength) - cell.textStart;
    if (localEnd <= localStart) {
        return;
    }

    const qreal leftRatio = static_cast<qreal>(localStart) / cell.textLength;
    const qreal rightRatio = static_cast<qreal>(localEnd) / cell.textLength;
    const QRectF selectionRect(cell.mathRect.left() + cell.mathRect.width() * leftRatio,
                               cell.mathRect.top() - 2,
                               qMax<qreal>(1.0, cell.mathRect.width() * (rightRatio - leftRatio)),
                               cell.mathRect.height() + 4);
    painter->fillRect(selectionRect, selectionHighlightColor());
}

int jkMathCursorForPosition(const JkMathLayout &layout,
                            const MarkdownRenderer::Block &block,
                            const QPoint &position)
{
    const QString source = block.text;
    if (source.isEmpty() || position.x() <= layout.textRect.left()) {
        return 0;
    }
    if (position.x() >= layout.textRect.right()) {
        return source.size();
    }

    const qreal ratio = qBound<qreal>(0.0,
                                      (position.x() - layout.textRect.left()) /
                                          qMax<qreal>(1.0, layout.textRect.width()),
                                      1.0);
    return qBound(0, qRound(ratio * source.size()), source.size());
}

bool jkMathHasTextAtPosition(const JkMathLayout &layout, const QPoint &position)
{
    return layout.textRect.adjusted(-3, -3, 3, 3).contains(position);
}

TableCellLayout buildTableCellLayout(const QStyleOptionViewItem &option,
                                     const MarkdownRenderer::TableCell &cell,
                                     bool header,
                                     MarkdownRenderer::TableAlignment alignment,
                                     const QRect &cellRect,
                                     int textStart)
{
    const QFont font = tableCellFont(option.font, header);
    const QColor color = option.palette.color(QPalette::Text);

    TableCellLayout result;
    result.font = font;
    result.cellRect = cellRect;
    result.spans = cell.spans;
    result.math = cell.math;
    result.mathSource = cell.mathSource;
    result.alignment = alignment;
    result.textStart = textStart;
    result.textLength = cell.text.size();
    result.innerRect = cellRect.adjusted(kTableCellPaddingX,
                                        kTableCellPaddingY,
                                        -kTableCellPaddingX,
                                        -kTableCellPaddingY);
    result.textRect = result.innerRect;
    if (cell.math) {
        bool parsed = false;
        result.mathSize = displayMathRenderedSize(font,
                                                  deviceDpiY(option),
                                                  cell.mathSource,
                                                  color,
                                                  kTableMathScale,
                                                  &parsed);
        if (result.mathSize.isEmpty()) {
            const QFontMetricsF metrics(font);
            result.mathSize = metrics.boundingRect(cell.text).size();
        }
        result.textHeight = qCeil(qMax<qreal>(QFontMetricsF(font).height(), result.mathSize.height()));
        const int mathWidth = qCeil(qMax<qreal>(1.0, result.mathSize.width()));
        result.mathRect = QRect(alignedContentLeft(result.innerRect, mathWidth, alignment),
                                result.innerRect.top(),
                                qMin(mathWidth, qMax(1, result.innerRect.width())),
                                result.textHeight);
        return result;
    }

    result.textLayout = std::make_unique<QTextLayout>(textWithInlineMathPlaceholders(cell.text, cell.spans), font);
    result.textLayout->setCacheEnabled(true);
    result.textLayout->setFormats(tableCellFormats(cell, font, color, option, textStart));

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setAlignment(tableTextAlignment(alignment));
    result.textLayout->setTextOption(textOption);
    result.textHeight = cell.text.isEmpty()
                            ? QFontMetrics(font).height()
                            : layoutText(result.textLayout.get(), result.textRect.width());
    return result;
}

TableLayout buildTableLayout(const QStyleOptionViewItem &option, const MarkdownRenderer::Block &block)
{
    TableLayout result;
    const QRect content = contentRect(option);
    const int columnCount = tableColumnCount(block);
    if (columnCount <= 0 || block.tableRows.isEmpty()) {
        result.tableRect = content;
        return result;
    }

    const QVector<int> columnWidths = tableColumnWidths(content.width(), columnCount);
    result.tableRect = QRect(content.left(), content.top(), content.width(), 0);

    result.rows.reserve(block.tableRows.size());
    int textOffset = 0;
    int bodyRowHeight = kTableMinRowHeight;
    for (int rowIndex = 0; rowIndex < block.tableRows.size(); ++rowIndex) {
        const MarkdownRenderer::TableRow &sourceRow = block.tableRows.at(rowIndex);
        TableRowLayout rowLayout;
        rowLayout.cells.reserve(columnCount);

        int x = content.left();
        int rowHeight = kTableMinRowHeight;
        for (int column = 0; column < columnCount; ++column) {
            const int cellTextStart = textOffset;
            const QRect cellRect(x, content.top(), columnWidths.at(column), kTableMinRowHeight);
            const MarkdownRenderer::TableCell emptyCell;
            const MarkdownRenderer::TableCell &cell = column < sourceRow.cells.size()
                                                          ? sourceRow.cells.at(column)
                                                          : emptyCell;
            const MarkdownRenderer::TableAlignment alignment = column < block.tableAlignments.size()
                                                                   ? block.tableAlignments.at(column)
                                                                   : MarkdownRenderer::TableAlignment::None;
            TableCellLayout cellLayout = buildTableCellLayout(option,
                                                              cell,
                                                              sourceRow.header,
                                                              alignment,
                                                              cellRect,
                                                              cellTextStart);
            rowHeight = qMax(rowHeight, cellLayout.textHeight + kTableCellPaddingY * 2);
            rowLayout.cells.push_back(std::move(cellLayout));
            x += columnWidths.at(column);
            textOffset += cell.text.size();
            if (column < columnCount - 1) {
                ++textOffset;
            }
        }

        if (!sourceRow.header) {
            bodyRowHeight = qMax(bodyRowHeight, rowHeight);
        }
        rowLayout.height = rowHeight;
        result.rows.push_back(std::move(rowLayout));
        if (rowIndex < block.tableRows.size() - 1) {
            ++textOffset;
        }
    }

    int y = content.top();
    for (int rowIndex = 0; rowIndex < static_cast<int>(result.rows.size()); ++rowIndex) {
        TableRowLayout &rowLayout = result.rows.at(rowIndex);
        const bool header = rowIndex < block.tableRows.size() && block.tableRows.at(rowIndex).header;
        const int rowHeight = header ? rowLayout.height : qMax(rowLayout.height, bodyRowHeight);
        for (TableCellLayout &cell : rowLayout.cells) {
            cell.cellRect.moveTop(y);
            cell.cellRect.setHeight(rowHeight);
            cell.innerRect = cell.cellRect.adjusted(kTableCellPaddingX,
                                                    kTableCellPaddingY,
                                                    -kTableCellPaddingX,
                                                    -kTableCellPaddingY);
            const int contentHeight = qMin(qMax(0, cell.textHeight), qMax(0, cell.innerRect.height()));
            const int contentTop = cell.innerRect.top() +
                                   qMax(0, (cell.innerRect.height() - contentHeight) / 2);
            cell.textRect = QRect(cell.innerRect.left(),
                                  contentTop,
                                  cell.innerRect.width(),
                                  contentHeight);
            if (cell.math) {
                const int mathWidth = qCeil(qMax<qreal>(1.0, cell.mathSize.width()));
                const int mathHeight = qCeil(qMax<qreal>(1.0, cell.mathSize.height()));
                cell.mathRect = QRect(alignedContentLeft(cell.innerRect,
                                                         mathWidth,
                                                         cell.alignment),
                                      cell.innerRect.top() +
                                          qMax(0, (cell.innerRect.height() - mathHeight) / 2),
                                      qMin(mathWidth, qMax(1, cell.innerRect.width())),
                                      qMin(mathHeight, qMax(1, cell.innerRect.height())));
            }
        }
        rowLayout.height = rowHeight;
        y += rowHeight;
    }

    result.height = y - content.top();
    result.tableRect.setHeight(result.height);
    return result;
}

QRect contentRect(const QStyleOptionViewItem &option)
{
    return option.rect.adjusted(kHorizontalPadding,
                                kVerticalPadding,
                                -kHorizontalPadding,
                                -kVerticalPadding);
}

QRect codeContainerRect(const QStyleOptionViewItem &option)
{
    return contentRect(option).adjusted(0, 2, 0, -2);
}

QRect codeHeaderRect(const QStyleOptionViewItem &option)
{
    const QRect codeRect = codeContainerRect(option);
    int maxTop = codeRect.bottom() - kCodeHeaderHeight + 1;
    if (maxTop < codeRect.top()) {
        maxTop = codeRect.top();
    }

    const int stickyTop = 0;
    const int headerTop = qBound(codeRect.top(), stickyTop, maxTop);
    return QRect(codeRect.left(), headerTop, codeRect.width(), kCodeHeaderHeight);
}

bool isCodeHeaderSticky(const QStyleOptionViewItem &option)
{
    return codeHeaderRect(option).top() != codeContainerRect(option).top();
}

QRect codeCopyButtonRect(const QStyleOptionViewItem &option)
{
    const QRect header = codeHeaderRect(option);
    return QRect(header.right() - kCodeHeaderHorizontalPadding - kCodeCopyButtonSize + 1,
                 header.top() + (header.height() - kCodeCopyButtonSize) / 2,
                 kCodeCopyButtonSize,
                 kCodeCopyButtonSize);
}

QRect centeredSquareRect(const QRect& rect, int side)
{
    const int boundedSide = qMax(1, qMin(side, qMin(rect.width(), rect.height())));
    return QRect(rect.left() + (rect.width() - boundedSide) / 2,
                 rect.top() + (rect.height() - boundedSide) / 2,
                 boundedSide,
                 boundedSide);
}

QRect codeLoadingSpinnerRect(const QStyleOptionViewItem& option)
{
    const QRect header = codeHeaderRect(option);
    const QRect iconSlot(header.left() + kCodeHeaderHorizontalPadding,
                         header.top(),
                         kCodeHeaderIconWidth,
                         header.height());
    return centeredSquareRect(iconSlot, kCodeHeaderSpinnerSize);
}

int taskCheckboxSize(const QStyleOptionViewItem &option, const MarkdownRenderer::Block &block)
{
    const QFontMetrics metrics(blockFont(option.font, block));
    return qBound(13, metrics.height() - 3, 20);
}

QString listMarkerText(const MarkdownRenderer::Block &block)
{
    return block.type == MarkdownRenderer::BlockType::OrderedList
               ? QString::number(block.number) + QLatin1Char('.')
               : QStringLiteral("•");
}

int listTextMarkerWidth(const QStyleOptionViewItem &option, const MarkdownRenderer::Block &block)
{
    return QFontMetrics(blockFont(option.font, block)).horizontalAdvance(listMarkerText(block));
}

int listMarkerWidth(const QStyleOptionViewItem &option, const MarkdownRenderer::Block &block)
{
    if (block.type != MarkdownRenderer::BlockType::UnorderedList &&
        block.type != MarkdownRenderer::BlockType::OrderedList) {
        return 0;
    }

    const int textMarkerWidth = listTextMarkerWidth(option, block);
    if (block.taskListItem) {
        return textMarkerWidth + kListMarkerGap + taskCheckboxSize(option, block) + kTaskCheckboxTextGap;
    }

    return textMarkerWidth + kListMarkerGap;
}

int listIndentForBlock(const MarkdownRenderer::Block &block)
{
    return kListIndent + qMax(0, block.level) * kNestedListIndent;
}

bool hasInlineMathSpan(const QVector<MarkdownRenderer::InlineSpan> &spans)
{
    return std::any_of(spans.cbegin(), spans.cend(), [](const MarkdownRenderer::InlineSpan &span) {
        return span.style & MarkdownRenderer::Math;
    });
}

QRect textRectForBlock(const QStyleOptionViewItem &option, const MarkdownRenderer::Block &block)
{
    QRect rect = contentRect(option);

    if (block.type == MarkdownRenderer::BlockType::UnorderedList ||
        block.type == MarkdownRenderer::BlockType::OrderedList) {
        rect.adjust(listIndentForBlock(block) + listMarkerWidth(option, block), 0, 0, 0);
    } else if (block.type == MarkdownRenderer::BlockType::BlockQuote) {
        rect.adjust(kQuoteIndent, 0, 0, 0);
    } else if (block.type == MarkdownRenderer::BlockType::CodeBlock) {
        rect.adjust(kCodePadding,
                    kCodeHeaderHeight + kCodePadding,
                    -kCodePadding,
                    -kCodePadding);
    }

    return rect;
}

void drawQuoteBar(QPainter *painter,
                  const QStyleOptionViewItem &option,
                  const QModelIndex &index,
                  const MarkdownRenderer::Block &block)
{
    if (!isQuoteDecoratedBlock(block)) {
        return;
    }

    QRect bar = contentRect(option);
    if (index.model()) {
        const QModelIndex parent = index.parent();
        const int rowCount = index.model()->rowCount(parent);
        const QModelIndex previous = index.row() > 0
                                         ? index.model()->index(index.row() - 1, index.column(), parent)
                                         : QModelIndex();
        const QModelIndex next = index.row() + 1 < rowCount
                                     ? index.model()->index(index.row() + 1, index.column(), parent)
                                     : QModelIndex();
        if (isQuoteDecoratedIndex(previous) && shouldConnectQuoteBars(block, modelBlock(previous))) {
            bar.setTop(option.rect.top());
        }
        if (isQuoteDecoratedIndex(next) && shouldConnectQuoteBars(block, modelBlock(next))) {
            bar.setBottom(option.rect.bottom());
        }
    }

    painter->fillRect(QRect(bar.left(), bar.top(), 4, bar.height()),
                      ThemeManager::instance().color(ThemeColor::Divider));
}

void drawTaskListMarker(QPainter *painter,
                        const QRect &markerRect,
                        const QStyleOptionViewItem &option,
                        const MarkdownRenderer::Block &block)
{
    const int textMarkerWidth = listTextMarkerWidth(option, block);
    const QRect textMarkerRect(markerRect.left(),
                               markerRect.top(),
                               textMarkerWidth + kListMarkerGap,
                               markerRect.height());
    painter->drawText(textMarkerRect,
                      Qt::AlignVCenter | Qt::AlignLeft,
                      listMarkerText(block));

    const int size = taskCheckboxSize(option, block);
    QRect box(QPoint(0, 0), QSize(size, size));
    box.moveLeft(textMarkerRect.right() + 1);
    box.moveCenter(QPoint(box.center().x(), markerRect.center().y()));

    const QColor text = textColor(option.palette, block);
    const QColor border = block.taskChecked
                              ? ThemeManager::instance().color(ThemeColor::Accent)
                              : QColor(text.red(), text.green(), text.blue(), 150);
    const QColor fill = block.taskChecked
                            ? ThemeManager::instance().color(ThemeColor::Accent)
                            : Qt::transparent;
    const QColor check = block.taskChecked
                             ? ThemeManager::textColorOn(fill)
                             : Qt::transparent;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(border, 1.4));
    painter->setBrush(fill);
    painter->drawRoundedRect(QRectF(box).adjusted(0.7, 0.7, -0.7, -0.7), 3.0, 3.0);

    if (block.taskChecked) {
        QPainterPath checkPath;
        checkPath.moveTo(box.left() + size * 0.25, box.top() + size * 0.53);
        checkPath.lineTo(box.left() + size * 0.43, box.top() + size * 0.70);
        checkPath.lineTo(box.left() + size * 0.76, box.top() + size * 0.33);
        painter->setPen(QPen(check, qMax<qreal>(1.7, size * 0.14), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(checkPath);
    }

    painter->restore();
}

BlockLayout buildLayout(const QStyleOptionViewItem &option, const QModelIndex &index)
{
    const MarkdownRenderer::Block block = modelBlock(index);
    const QFont font = blockFont(option.font, block);
    const QColor color = textColor(option.palette, block);
    const QRect textRect = textRectForBlock(option, block);

    BlockLayout result;
    result.textLayout = std::make_unique<QTextLayout>(layoutTextForBlock(block), font);
    result.textRect = textRect;
    result.textLayout->setCacheEnabled(true);
    result.textLayout->setFormats(inlineFormats(block, font, color, option));

    QTextOption textOption;
    textOption.setWrapMode(block.type == MarkdownRenderer::BlockType::CodeBlock
                               ? QTextOption::WrapAnywhere
                               : QTextOption::WrapAtWordBoundaryOrAnywhere);
    result.textLayout->setTextOption(textOption);
    result.textLayout->beginLayout();

    qreal y = 0;
    qreal codeLineHeight = QFontMetricsF(font).lineSpacing();
    while (true) {
        QTextLine line = result.textLayout->createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(qMax(20, textRect.width()));
        if (block.type == MarkdownRenderer::BlockType::CodeBlock) {
            codeLineHeight = qMax(codeLineHeight, line.height());
        } else {
            const InlineMathLineInsets mathInsets = inlineMathLineInsets(line,
                                                                         block.spans,
                                                                         font,
                                                                         deviceDpiY(option),
                                                                         color);
            line.setPosition(QPointF(0, y + mathInsets.top));
            y += mathInsets.top + line.height() + mathInsets.bottom;
        }
    }

    result.textLayout->endLayout();
    if (block.type == MarkdownRenderer::BlockType::CodeBlock) {
        result.lineHeight = codeLineHeight;
        y = 0;
        for (int lineIndex = 0; lineIndex < result.textLayout->lineCount(); ++lineIndex) {
            QTextLine line = result.textLayout->lineAt(lineIndex);
            line.setPosition(QPointF(0, y));
            y += codeLineHeight;
        }
    }

    result.height = qCeil(y);
    return result;
}

int blockVerticalExtra(const MarkdownRenderer::Block &block)
{
    if (block.type == MarkdownRenderer::BlockType::CodeBlock) {
        return kCodeHeaderHeight + kCodePadding * 2 + kCodeBlockExtra;
    }
    return 4;
}

int tableCursorForPosition(const TableLayout &tableLayout,
                           const MarkdownRenderer::Block &block,
                           const QPoint &position)
{
    if (tableLayout.rows.empty()) {
        return 0;
    }

    if (position.y() <= tableLayout.tableRect.top()) {
        return 0;
    }
    if (position.y() >= tableLayout.tableRect.bottom()) {
        return block.text.size();
    }

    for (const TableRowLayout &row : tableLayout.rows) {
        if (row.cells.empty()) {
            continue;
        }
        if (position.y() < row.cells.front().cellRect.top() ||
            position.y() > row.cells.front().cellRect.bottom()) {
            continue;
        }

        if (position.x() <= row.cells.front().cellRect.left()) {
            return row.cells.front().textStart;
        }
        if (position.x() >= row.cells.back().cellRect.right()) {
            const TableCellLayout &cell = row.cells.back();
            return cell.textStart + cell.textLength;
        }

        for (const TableCellLayout &cell : row.cells) {
            if (!cell.cellRect.adjusted(0, 0, 1, 0).contains(position)) {
                continue;
            }
            if (cell.math) {
                if (cell.textLength <= 0 || position.x() <= cell.mathRect.left()) {
                    return cell.textStart;
                }
                if (position.x() >= cell.mathRect.right()) {
                    return cell.textStart + cell.textLength;
                }

                const qreal ratio = qBound<qreal>(0.0,
                                                  (position.x() - cell.mathRect.left()) /
                                                      qMax<qreal>(1.0, cell.mathRect.width()),
                                                  1.0);
                return cell.textStart + qBound(0, qRound(ratio * cell.textLength), cell.textLength);
            }

            const QPoint local = position - cell.textRect.topLeft();
            if (local.y() <= 0) {
                return cell.textStart;
            }
            if (local.y() >= cell.textHeight) {
                return cell.textStart + cell.textLength;
            }

            for (int lineIndex = 0; lineIndex < cell.textLayout->lineCount(); ++lineIndex) {
                const QTextLine line = cell.textLayout->lineAt(lineIndex);
                const QRectF lineRect(line.position(),
                                      QSizeF(qMax(1, cell.textRect.width()), line.height()));
                if (local.y() <= lineRect.bottom() || lineIndex == cell.textLayout->lineCount() - 1) {
                    return cell.textStart + line.xToCursor(local.x(), QTextLine::CursorBetweenCharacters);
                }
            }

            return cell.textStart + cell.textLength;
        }
    }

    return block.text.size();
}

bool tableHasTextAtPosition(const TableLayout &tableLayout, const QPoint &position)
{
    for (const TableRowLayout &row : tableLayout.rows) {
        for (const TableCellLayout &cell : row.cells) {
            if (cell.math) {
                if (cell.textLength > 0 && cell.mathRect.adjusted(-2, -2, 2, 2).contains(position)) {
                    return true;
                }
                continue;
            }
            if (cell.textLength <= 0 || !cell.textRect.adjusted(-2, 0, 2, 0).contains(position)) {
                continue;
            }

            const QPoint local = position - cell.textRect.topLeft();
            for (int lineIndex = 0; lineIndex < cell.textLayout->lineCount(); ++lineIndex) {
                const QTextLine line = cell.textLayout->lineAt(lineIndex);
                const qreal startX = line.cursorToX(line.textStart());
                const qreal endX = line.cursorToX(line.textStart() + line.textLength());
                const QRectF lineRect(line.position().x() + qMin(startX, endX),
                                      line.position().y(),
                                      qMax<qreal>(1.0, qAbs(endX - startX)),
                                      line.height());
                if (lineRect.adjusted(-2, 0, 2, 0).contains(QPointF(local))) {
                    return true;
                }
            }
        }
    }
    return false;
}

qreal painterDevicePixelRatio(QPainter *painter)
{
    if (!painter || !painter->device()) {
        return 1.0;
    }
    return qMax<qreal>(1.0, painter->device()->devicePixelRatioF());
}

bool drawRasterIcon(QPainter *painter,
                    const QString &source,
                    const QRect &iconRect,
                    const QSize &iconSize,
                    bool inverted = false)
{
    QPixmap icon = ImageService::instance().scaled(source,
                                                   iconSize,
                                                   Qt::KeepAspectRatio,
                                                   painterDevicePixelRatio(painter));
    if (!icon.isNull()) {
        if (inverted) {
            QImage image = icon.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
            image.invertPixels(QImage::InvertRgb);
            QPixmap invertedIcon = QPixmap::fromImage(image);
            invertedIcon.setDevicePixelRatio(icon.devicePixelRatio());
            icon = invertedIcon;
        }

        QRect target(QPoint(0, 0), iconSize);
        target.moveCenter(iconRect.center());
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawPixmap(target, icon);
        painter->restore();
        return true;
    }
    return false;
}

QMargins scaledTargetMargins(const QSize &sourceSize, const QSize &targetSize, const QMargins &sourceMargins)
{
    if (sourceSize.isEmpty() || targetSize.isEmpty()) {
        return {};
    }

    const qreal scale = static_cast<qreal>(targetSize.height()) / static_cast<qreal>(sourceSize.height());
    int left = qMax(1, qRound(sourceMargins.left() * scale));
    int right = qMax(1, qRound(sourceMargins.right() * scale));
    int top = qMax(1, qRound(sourceMargins.top() * scale));
    int bottom = qMax(1, qRound(sourceMargins.bottom() * scale));

    const int maxHorizontalMargin = qMax(1, targetSize.width() / 2 - 1);
    const int maxVerticalMargin = qMax(1, targetSize.height() / 2 - 1);
    left = qMin(left, maxHorizontalMargin);
    right = qMin(right, maxHorizontalMargin);
    top = qMin(top, maxVerticalMargin);
    bottom = qMin(bottom, maxVerticalMargin);
    return {left, top, right, bottom};
}

void drawNinePatch(QPainter *painter,
                   const QPixmap &pixmap,
                   const QRect &targetRect,
                   const QMargins &sourceMargins = QMargins(5, 5, 5, 5))
{
    if (!painter || pixmap.isNull() || targetRect.isEmpty()) {
        return;
    }

    const QRect sourceRect(QPoint(0, 0), pixmap.size());
    const QMargins margins(qMin(sourceMargins.left(), sourceRect.width() / 2 - 1),
                           qMin(sourceMargins.top(), sourceRect.height() / 2 - 1),
                           qMin(sourceMargins.right(), sourceRect.width() / 2 - 1),
                           qMin(sourceMargins.bottom(), sourceRect.height() / 2 - 1));
    const QMargins targetMargins = scaledTargetMargins(sourceRect.size(), targetRect.size(), margins);

    const int sourceXs[] = {
        sourceRect.left(),
        sourceRect.left() + margins.left(),
        sourceRect.right() - margins.right() + 1,
        sourceRect.right() + 1
    };
    const int sourceYs[] = {
        sourceRect.top(),
        sourceRect.top() + margins.top(),
        sourceRect.bottom() - margins.bottom() + 1,
        sourceRect.bottom() + 1
    };
    const int targetXs[] = {
        targetRect.left(),
        targetRect.left() + targetMargins.left(),
        targetRect.right() - targetMargins.right() + 1,
        targetRect.right() + 1
    };
    const int targetYs[] = {
        targetRect.top(),
        targetRect.top() + targetMargins.top(),
        targetRect.bottom() - targetMargins.bottom() + 1,
        targetRect.bottom() + 1
    };

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const QRect sourcePart(QPoint(sourceXs[column], sourceYs[row]),
                                   QPoint(sourceXs[column + 1] - 1, sourceYs[row + 1] - 1));
            const QRect targetPart(QPoint(targetXs[column], targetYs[row]),
                                   QPoint(targetXs[column + 1] - 1, targetYs[row + 1] - 1));
            if (!sourcePart.isEmpty() && !targetPart.isEmpty()) {
                painter->drawPixmap(targetPart, pixmap, sourcePart);
            }
        }
    }
}

void drawCopyIcon(QPainter *painter,
                  const QRect &buttonRect,
                  const QColor &color,
                  const QColor &copiedColor,
                  bool copied,
                  bool invertRasterIcon)
{
    if (copied) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(copiedColor, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPoint left(buttonRect.left() + 6, buttonRect.center().y());
        const QPoint middle(buttonRect.left() + 10, buttonRect.center().y() + 4);
        const QPoint right(buttonRect.right() - 4, buttonRect.top() + 6);
        painter->drawLine(left, middle);
        painter->drawLine(middle, right);
        painter->restore();
        return;
    }

    const QString iconSource = copied
                                   ? QStringLiteral(":/resources/icon/check.png")
                                   : QStringLiteral(":/resources/icon/copy.svg");
    const QSize iconSize(copied ? 16 : 15, copied ? 16 : 15);
    if (drawRasterIcon(painter, iconSource, buttonRect, iconSize, invertRasterIcon)) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(color, 1.3));
    painter->setBrush(Qt::NoBrush);

    const QRect backSheet(buttonRect.left() + 6,
                          buttonRect.top() + 5,
                          9,
                          11);
    const QRect frontSheet(buttonRect.left() + 9,
                           buttonRect.top() + 8,
                           9,
                           11);
    painter->drawRoundedRect(backSheet, 2, 2);
    painter->fillRect(frontSheet.adjusted(0, 0, 1, 1), codeBlockPalette().background);
    painter->drawRoundedRect(frontSheet, 2, 2);
    painter->restore();
}

void drawCodeIcon(QPainter *painter, const QRect &iconRect, const CodeBlockPalette &palette)
{
    if (drawRasterIcon(painter,
                       QStringLiteral(":/resources/icon/coding.svg"),
                       iconRect,
                       QSize(kCodeHeaderIconSize, kCodeHeaderIconSize),
                       ThemeManager::instance().isDark())) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(palette.icon, 1.6));
    const QRect fallbackRect(QPoint(0, 0), QSize(kCodeHeaderIconSize, kCodeHeaderIconSize));
    QRect iconBounds = fallbackRect;
    iconBounds.moveCenter(iconRect.center());
    const int cy = iconBounds.center().y();
    const int left = iconBounds.left() + 2;
    const int right = iconBounds.right() - 2;
    painter->drawLine(left + 4, cy - 5, left, cy);
    painter->drawLine(left, cy, left + 4, cy + 5);
    painter->drawLine(right - 4, cy - 5, right, cy);
    painter->drawLine(right, cy, right - 4, cy + 5);
    painter->drawLine(iconBounds.center().x() + 3, cy - 6, iconBounds.center().x() - 3, cy + 6);
    painter->restore();
}

QPainterPath topRoundedRectPath(const QRectF &rect, qreal radius)
{
    const qreal r = qMin(radius, qMin(rect.width(), rect.height()) / 2.0);
    QPainterPath path;
    path.moveTo(rect.left(), rect.bottom());
    path.lineTo(rect.left(), rect.top() + r);
    path.quadTo(rect.left(), rect.top(), rect.left() + r, rect.top());
    path.lineTo(rect.right() - r, rect.top());
    path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + r);
    path.lineTo(rect.right(), rect.bottom());
    path.closeSubpath();
    return path;
}

QRect settingContainerRect(const QStyleOptionViewItem &option)
{
    return contentRect(option).adjusted(0, 2, 0, -2);
}

QRect settingHeaderRect(const QStyleOptionViewItem &option)
{
    const QRect container = settingContainerRect(option);
    return QRect(container.left(), container.top(), container.width(), kCodeHeaderHeight);
}

QRect settingBodyRect(const QStyleOptionViewItem &option)
{
    const QRect container = settingContainerRect(option);
    return QRect(container.left(),
                 container.top() + kCodeHeaderHeight,
                 container.width(),
                 qMax(0, container.height() - kCodeHeaderHeight));
}

QRect settingControlRect(const QStyleOptionViewItem &option)
{
    const QRect body = settingBodyRect(option).adjusted(kSettingBodyPadding,
                                                        kSettingBodyPadding,
                                                        -kSettingBodyPadding,
                                                        -kSettingBodyPadding);
    const QSize size(qMin(kSettingControlMaxWidth, qMax(0, body.width())),
                     kSettingControlHeight);
    QRect control(QPoint(0, 0), size);
    control.moveCenter(body.center());
    return control;
}

QRect settingActionButtonRect(const QStyleOptionViewItem &option)
{
    const QRect header = settingHeaderRect(option);
    return QRect(header.right() - kCodeHeaderHorizontalPadding - kSettingActionButtonSize + 1,
                 header.top() + (header.height() - kSettingActionButtonSize) / 2,
                 kSettingActionButtonSize,
                 kSettingActionButtonSize);
}

QRect settingActionHitRect(const QStyleOptionViewItem &option)
{
    const QRect header = settingHeaderRect(option);
    return QRect(header.right() - kCodeHeaderHorizontalPadding - 40 + 1,
                 header.top(),
                 40,
                 header.height());
}

bool isSettingRevoked(const QStyleOptionViewItem &option, int row)
{
    for (const QObject *object = option.widget; object; object = object->parent()) {
        const QVariant value = object->property("markdownRevokedSettingRows");
        if (!value.isValid()) {
            continue;
        }
        const QVariantList rows = value.toList();
        for (const QVariant &rowValue : rows) {
            if (rowValue.toInt() == row) {
                return true;
            }
        }
    }
    return false;
}

void drawMinecraftButtonPreview(QPainter *painter,
                                const QRect &buttonRect,
                                const QString &text,
                                const QFont &baseFont)
{
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QPixmap pixmap = ImageService::instance().pixmap(kMinecraftButtonSource);
    if (pixmap.isNull()) {
        painter->fillRect(buttonRect, QColor(0x75, 0x75, 0x75));
    } else {
        drawNinePatch(painter, pixmap, buttonRect);
    }

    AppFonts::configurePainterForText(*painter);
    QFont textFont = AppFonts::pixelSizedFont(baseFont, 13, true);
    painter->setFont(textFont);
    const QRect textRect = buttonRect.adjusted(10, 0, -10, 0);
    painter->setPen(QColor(0, 0, 0, 180));
    painter->drawText(textRect.translated(1, 1), Qt::AlignCenter, text);
    painter->setPen(QColor(0xFF, 0xFF, 0xFF));
    painter->drawText(textRect, Qt::AlignCenter, text);
    painter->restore();
}

void drawMinecraftSliderPreview(QPainter *painter,
                                const QRect &sliderRect,
                                const MarkdownRenderer::Block &block,
                                const QFont &baseFont)
{
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QPixmap sliderPixmap = ImageService::instance().pixmap(kMinecraftSliderSource);
    if (sliderPixmap.isNull()) {
        painter->fillRect(sliderRect, QColor(0x75, 0x75, 0x75));
    } else {
        drawNinePatch(painter, sliderPixmap, sliderRect);
    }

    const QPixmap handlePixmap = ImageService::instance().pixmap(kMinecraftSliderHandleSource);
    const int range = qMax(1, block.settingMaximum - block.settingMinimum);
    const int fallbackHandleWidth = qMax(10, sliderRect.height() / 2);
    const int handleWidth = handlePixmap.isNull() || handlePixmap.height() <= 0
            ? fallbackHandleWidth
            : qBound(10,
                     qRound(static_cast<qreal>(handlePixmap.width()) * sliderRect.height()
                            / static_cast<qreal>(handlePixmap.height())),
                     qMax(10, sliderRect.width()));
    const int availableWidth = qMax(0, sliderRect.width() - handleWidth);
    const qreal progress = static_cast<qreal>(block.settingValue - block.settingMinimum)
            / static_cast<qreal>(range);
    const QRect handle(sliderRect.left() + qRound(progress * availableWidth),
                       sliderRect.top(),
                       handleWidth,
                       sliderRect.height());
    if (handlePixmap.isNull()) {
        painter->fillRect(handle, QColor(0xC6, 0xC6, 0xC6));
    } else {
        painter->drawPixmap(handle, handlePixmap, handlePixmap.rect());
    }

    AppFonts::configurePainterForText(*painter);
    QFont textFont = AppFonts::pixelSizedFont(baseFont, 13, true);
    painter->setFont(textFont);
    const QRect textRect = sliderRect.adjusted(8, 0, -8, 0);
    painter->setPen(QColor(0, 0, 0, 180));
    painter->drawText(textRect.translated(1, 1), Qt::AlignCenter, block.settingLabel);
    painter->setPen(QColor(0xFF, 0xFF, 0xFF));
    painter->drawText(textRect, Qt::AlignCenter, block.settingLabel);
    painter->restore();
}

QString settingLabelForValue(const MarkdownRenderer::Block &block, const QString &value)
{
    const QString normalizedAction = block.settingAction.trimmed().toLower();
    const QString normalizedValue = value.trimmed().toLower();
    if (normalizedAction == QStringLiteral("settings.appearance.mode")) {
        if (normalizedValue == QStringLiteral("light") || normalizedValue == QStringLiteral("浅色模式")) {
            return QStringLiteral("外观模式：浅色模式");
        }
        if (normalizedValue == QStringLiteral("dark") || normalizedValue == QStringLiteral("深色模式")) {
            return QStringLiteral("外观模式：深色模式");
        }
        if (normalizedValue == QStringLiteral("follow-system") ||
                normalizedValue == QStringLiteral("system") ||
                normalizedValue == QStringLiteral("跟随系统")) {
            return QStringLiteral("外观模式：跟随系统");
        }
    }
    if (normalizedAction == QStringLiteral("settings.appearance.theme_color") ||
            normalizedAction == QStringLiteral("settings.appearance.themecolor")) {
        return QStringLiteral("主题颜色：%1").arg(value.trimmed().toUpper());
    }
    return {};
}

QString settingDisplayLabel(const MarkdownRenderer::Block &block, bool revoked)
{
    if (!revoked) {
        return block.settingLabel;
    }
    if (!block.settingPreviousLabel.isEmpty()) {
        return block.settingPreviousLabel;
    }
    const QString fallback = settingLabelForValue(block, block.settingPreviousValueText);
    return fallback.isEmpty() ? block.settingLabel : fallback;
}

void drawSettingBlock(QPainter *painter,
                      const QStyleOptionViewItem &option,
                      const MarkdownRenderer::Block &block,
                      int row)
{
    const QRect container = settingContainerRect(option);
    const QRect header = settingHeaderRect(option);
    const QRect actionButton = settingActionButtonRect(option);
    const QRect control = settingControlRect(option);
    const CodeBlockPalette palette = codeBlockPalette();
    const bool revoked = isSettingRevoked(option, row);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPainterPath containerPath;
    containerPath.addRoundedRect(QRectF(container), kCodeBlockRadius, kCodeBlockRadius);
    painter->fillPath(containerPath, ThemeManager::instance().color(ThemeColor::SettingsFallbackBackground));

    painter->save();
    painter->setClipPath(containerPath);
    const QPixmap background = ImageService::instance().pixmap(kSettingsBackgroundSource);
    if (!background.isNull()) {
        painter->drawTiledPixmap(container, background);
    }
    painter->fillRect(container, ThemeManager::instance().color(ThemeColor::SettingsOverlay));
    painter->restore();

    painter->fillPath(topRoundedRectPath(QRectF(header), kCodeBlockRadius), palette.headerBackground);
    painter->setPen(QPen(palette.headerBorder, 1));
    painter->drawLine(header.left(), header.bottom(), header.right(), header.bottom());

    AppFonts::configurePainterForText(*painter);
    QFont titleFont = option.font;
    titleFont.setPixelSize(qMax(kMinFontPixelSize, fontPixelSize(option.font)));
    titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont);
    painter->setPen(palette.headerText);
    painter->drawText(header.adjusted(kCodeHeaderHorizontalPadding,
                                      0,
                                      -(kCodeHeaderHorizontalPadding + kSettingActionButtonSize + kCodeHeaderGap),
                                      0),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      QStringLiteral("Setting"));

    drawRasterIcon(painter,
                   revoked ? kSettingReapplySource : kSettingUndoSource,
                   actionButton,
                   QSize(15, 15),
                   ThemeManager::instance().isDark());

    const QString controlLabel = settingDisplayLabel(block, revoked);
    if (block.settingControl == MarkdownRenderer::SettingControlType::Slider) {
        MarkdownRenderer::Block displayedBlock = block;
        displayedBlock.settingLabel = controlLabel;
        drawMinecraftSliderPreview(painter, control, displayedBlock, option.font);
    } else {
        drawMinecraftButtonPreview(painter, control, controlLabel, option.font);
    }

    painter->setPen(QPen(palette.border, 1));
    painter->drawPath(containerPath);
    painter->restore();
}

int copiedCodeRow(const QStyleOptionViewItem &option)
{
    for (const QObject *object = option.widget; object; object = object->parent()) {
        bool ok = false;
        const int row = object->property("markdownCopiedCodeRow").toInt(&ok);
        if (ok) {
            return row;
        }
    }
    return -1;
}

bool isCodeBlockLoading(const QStyleOptionViewItem &option)
{
    return option.index.data(MarkdownDocumentModel::CodeBlockLoadingRole).toBool();
}

void drawCodeHeader(QPainter *painter,
                    const QStyleOptionViewItem &option,
                    const MarkdownRenderer::Block &block,
                    int row)
{
    const QRect header = codeHeaderRect(option);
    const QRect button = codeCopyButtonRect(option);
    const bool sticky = isCodeHeaderSticky(option);
    const bool copied = row >= 0 && row == copiedCodeRow(option);
    const bool loading = isCodeBlockLoading(option);
    const CodeBlockPalette palette = codeBlockPalette();

    painter->save();
    painter->setClipRect(codeContainerRect(option).adjusted(0, 0, 0, 1));
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (sticky) {
        painter->fillRect(header, palette.headerStickyBackground);
    } else {
        painter->fillPath(topRoundedRectPath(QRectF(header), kCodeBlockRadius),
                          palette.headerBackground);
    }
    painter->setPen(QPen(palette.headerBorder, 1));
    painter->drawLine(header.left(), header.bottom(), header.right(), header.bottom());

    const int baseSize = qMax(kMinFontPixelSize, fontPixelSize(option.font));

    const QRect iconRect(header.left() + kCodeHeaderHorizontalPadding,
                         header.top(),
                         kCodeHeaderIconWidth,
                         header.height());
    if (loading) {
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() % 1000;
        LoadingSpinnerRenderer::drawCircularSpinner(painter,
                                                    codeLoadingSpinnerRect(option),
                                                    palette.icon,
                                                    elapsed / 1000.0);
    } else {
        drawCodeIcon(painter, iconRect, palette);
    }

    QFont labelFont = option.font;
    labelFont.setPixelSize(qMax(kMinFontPixelSize, baseSize));
    labelFont.setWeight(QFont::DemiBold);

    const QRect labelRect(iconRect.right() + kCodeHeaderGap,
                          header.top(),
                          qMax(0, button.left() - iconRect.right() - kCodeHeaderGap * 2),
                          header.height());
    painter->setFont(labelFont);
    painter->setPen(palette.headerText);
    painter->drawText(labelRect,
                      Qt::AlignVCenter | Qt::AlignLeft,
                      CodeSyntaxHighlighter::displayName(block.language));

    drawCopyIcon(painter,
                 button,
                 palette.copyIcon,
                 palette.copyIconCopied,
                 copied,
                 ThemeManager::instance().isDark());

    painter->restore();
}

} // namespace

MarkdownDelegate::MarkdownDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void MarkdownDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    QStyleOptionViewItem viewOption(option);
    initStyleOption(&viewOption, index);
    viewOption.text.clear();
    viewOption.index = index;

    QStyle *style = viewOption.widget ? viewOption.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &viewOption, painter, viewOption.widget);

    const MarkdownRenderer::Block block = modelBlock(index);
    const QRect content = contentRect(viewOption);

    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (block.type == MarkdownRenderer::BlockType::HorizontalRule) {
        painter->setPen(QPen(ThemeManager::instance().color(ThemeColor::Divider), 1));
        const int y = content.center().y();
        painter->drawLine(content.left(), y, content.right(), y);
        painter->restore();
        return;
    }

    if (block.type == MarkdownRenderer::BlockType::MathBlock) {
        const JkMathLayout mathLayout = buildJkMathLayout(viewOption, block);
        const int selectionStart = index.data(MarkdownDocumentModel::SelectionStartRole).toInt();
        const int selectionEnd = index.data(MarkdownDocumentModel::SelectionEndRole).toInt();
        drawQuoteBar(painter, viewOption, index, block);
        drawJkMathSelection(painter,
                            mathLayout,
                            selectionStart,
                            selectionEnd);
        drawJkMath(painter, viewOption, block, mathLayout);
        painter->restore();
        return;
    }

    if (block.type == MarkdownRenderer::BlockType::SettingBlock) {
        drawSettingBlock(painter, viewOption, block, index.row());
        painter->restore();
        return;
    }

    if (block.type == MarkdownRenderer::BlockType::Table) {
        const TableLayout tableLayout = buildTableLayout(viewOption, block);
        for (const TableRowLayout &row : tableLayout.rows) {
            for (const TableCellLayout &cell : row.cells) {
                painter->save();
                painter->setClipRect(cell.textRect);
                painter->setPen(viewOption.palette.color(QPalette::Text));
                if (cell.math) {
                    drawTableCellMathSelection(painter, viewOption, cell);
                    drawTableCellMath(painter, viewOption, cell);
                } else {
                    drawInlineCodeBackgrounds(painter, cell.textLayout.get(), cell.textRect.topLeft(), cell.spans);
                    cell.textLayout->draw(painter, cell.textRect.topLeft());
                    drawInlineMath(painter,
                                   viewOption,
                                   cell.textLayout.get(),
                                   cell.textRect.topLeft(),
                                   cell.spans,
                                   viewOption.palette.color(QPalette::Text));
                }
                painter->restore();
            }
        }

        for (int rowIndex = 0; rowIndex < static_cast<int>(tableLayout.rows.size()) - 1; ++rowIndex) {
            const TableRowLayout &row = tableLayout.rows.at(rowIndex);
            if (row.cells.empty()) {
                continue;
            }

            const QRect firstText = row.cells.front().textRect;
            const QRect lastText = row.cells.back().textRect;
            const int y = row.cells.front().cellRect.bottom();
            const QColor divider = ThemeManager::instance().color(ThemeColor::Divider);
            const QColor lineColor = rowIndex == 0 && !ThemeManager::instance().isDark()
                                         ? QColor(QStringLiteral("#c2c8d0"))
                                         : divider;
            painter->setPen(QPen(lineColor, 1));
            painter->drawLine(firstText.left(), y, lastText.right(), y);
        }

        painter->restore();
        return;
    }

    BlockLayout layout = buildLayout(viewOption, index);

    if (block.type == MarkdownRenderer::BlockType::BlockQuote) {
        drawQuoteBar(painter, viewOption, index, block);
    } else if (block.type == MarkdownRenderer::BlockType::CodeBlock) {
        const QRect codeRect = codeContainerRect(viewOption);
        const CodeBlockPalette palette = codeBlockPalette();
        QPainterPath codePath;
        codePath.addRoundedRect(QRectF(codeRect), kCodeBlockRadius, kCodeBlockRadius);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->fillPath(codePath, palette.background);
        painter->setPen(QPen(palette.border, 1));
        painter->drawPath(codePath);
        painter->restore();
    } else if (block.type == MarkdownRenderer::BlockType::UnorderedList ||
               block.type == MarkdownRenderer::BlockType::OrderedList) {
        painter->setFont(blockFont(viewOption.font, block));
        painter->setPen(textColor(viewOption.palette, block));
        QRect markerRect(content.left() + listIndentForBlock(block),
                         content.top(),
                         listMarkerWidth(viewOption, block),
                         QFontMetrics(blockFont(viewOption.font, block)).height());
        if (layout.textLayout && layout.textLayout->lineCount() > 0) {
            const QTextLine firstLine = layout.textLayout->lineAt(0);
            const int markerOffset = hasInlineMathSpan(block.spans)
                                         ? kInlineMathListMarkerOffsetY
                                         : 0;
            markerRect.setTop(qRound(layout.textRect.top() + firstLine.position().y()) + markerOffset);
            markerRect.setHeight(qCeil(firstLine.height()));
        }
        if (block.taskListItem) {
            drawTaskListMarker(painter, markerRect, viewOption, block);
        } else {
            painter->drawText(markerRect,
                              Qt::AlignVCenter | Qt::AlignLeft,
                              listMarkerText(block));
        }
    }

    painter->setPen(textColor(viewOption.palette, block));
    if (block.type == MarkdownRenderer::BlockType::CodeBlock) {
        painter->save();
        painter->setClipRect(codeContainerRect(viewOption));
        layout.textLayout->draw(painter, layout.textRect.topLeft());
        painter->restore();
        drawCodeHeader(painter, viewOption, block, index.row());
    } else {
        drawInlineCodeBackgrounds(painter, layout.textLayout.get(), layout.textRect.topLeft(), block.spans);
        layout.textLayout->draw(painter, layout.textRect.topLeft());
        drawInlineMath(painter,
                       viewOption,
                       layout.textLayout.get(),
                       layout.textRect.topLeft(),
                       block.spans,
                       textColor(viewOption.palette, block));
    }
    painter->restore();
}

QSize MarkdownDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem viewOption(option);
    viewOption.index = index;

    if (index.data(MarkdownDocumentModel::TypeRole).toInt() ==
            static_cast<int>(MarkdownRenderer::BlockType::SettingBlock)) {
        return QSize(qMax(80, option.rect.width()),
                     kCodeHeaderHeight + kSettingBodyPadding * 2 + kSettingControlHeight
                             + kVerticalPadding * 2 + 4);
    }

    const MarkdownRenderer::Block block = modelBlock(index);
    static QCache<QString, QSize> sizeCache(8192);
    const QString cacheKey = sizeHintCacheKey(viewOption, block);
    if (const QSize *cachedSize = sizeCache.object(cacheKey)) {
        return *cachedSize;
    }

    QSize size;
    if (block.type == MarkdownRenderer::BlockType::HorizontalRule) {
        size = QSize(qMax(80, option.rect.width()), 28);
    } else if (block.type == MarkdownRenderer::BlockType::Table) {
        const TableLayout tableLayout = buildTableLayout(viewOption, block);
        size = QSize(qMax(80, option.rect.width()),
                     tableLayout.height + kVerticalPadding * 2 + 4);
    } else if (block.type == MarkdownRenderer::BlockType::MathBlock) {
        const JkMathLayout mathLayout = buildJkMathLayout(viewOption, block);
        size = QSize(qMax(80, option.rect.width()),
                     mathLayout.height + kVerticalPadding * 2);
    } else {
        BlockLayout layout = buildLayout(viewOption, index);
        size = QSize(qMax(80, option.rect.width()),
                     layout.height + kVerticalPadding * 2 + blockVerticalExtra(block));
    }

    sizeCache.insert(cacheKey, new QSize(size), qMax(1, size.height() / 32));
    return size;
}

int MarkdownDelegate::cursorForPosition(const QStyleOptionViewItem &option,
                                        const QModelIndex &index,
                                        const QPoint &position) const
{
    QStyleOptionViewItem viewOption(option);
    viewOption.index = index;
    const MarkdownRenderer::Block block = modelBlock(index);
    if (block.type == MarkdownRenderer::BlockType::Table) {
        return tableCursorForPosition(buildTableLayout(viewOption, block), block, position);
    }
    if (block.type == MarkdownRenderer::BlockType::MathBlock) {
        return jkMathCursorForPosition(buildJkMathLayout(viewOption, block), block, position);
    }
    if (block.type == MarkdownRenderer::BlockType::SettingBlock) {
        return 0;
    }

    BlockLayout layout = buildLayout(viewOption, index);

    const QPoint local = position - layout.textRect.topLeft();
    if (local.y() <= 0) {
        return 0;
    }

    for (int i = 0; i < layout.textLayout->lineCount(); ++i) {
        const QTextLine line = layout.textLayout->lineAt(i);
        const qreal lineHeight = block.type == MarkdownRenderer::BlockType::CodeBlock && layout.lineHeight > 0
                                     ? layout.lineHeight
                                     : line.height();
        const QRectF naturalRect(line.position(), QSizeF(line.naturalTextWidth(), lineHeight));
        if (local.y() <= naturalRect.bottom() || i == layout.textLayout->lineCount() - 1) {
            return line.xToCursor(local.x(), QTextLine::CursorBetweenCharacters);
        }
    }

    return modelBlock(index).text.size();
}

bool MarkdownDelegate::hasTextAtPosition(const QStyleOptionViewItem &option,
                                         const QModelIndex &index,
                                         const QPoint &position) const
{
    QStyleOptionViewItem viewOption(option);
    viewOption.index = index;
    const MarkdownRenderer::Block block = modelBlock(index);
    if (block.type == MarkdownRenderer::BlockType::Table) {
        return tableHasTextAtPosition(buildTableLayout(viewOption, block), position);
    }
    if (block.type == MarkdownRenderer::BlockType::MathBlock) {
        return jkMathHasTextAtPosition(buildJkMathLayout(viewOption, block), position);
    }
    if (block.type == MarkdownRenderer::BlockType::SettingBlock) {
        return false;
    }
    if (block.type == MarkdownRenderer::BlockType::CodeBlock && codeHeaderRect(viewOption).contains(position)) {
        return false;
    }

    BlockLayout layout = buildLayout(viewOption, index);

    const QPoint local = position - layout.textRect.topLeft();
    if (local.y() < 0 || local.y() > layout.height) {
        return false;
    }

    for (int i = 0; i < layout.textLayout->lineCount(); ++i) {
        const QTextLine line = layout.textLayout->lineAt(i);
        const qreal lineHeight = block.type == MarkdownRenderer::BlockType::CodeBlock && layout.lineHeight > 0
                                     ? layout.lineHeight
                                     : line.height();
        const QRectF lineRect(line.position(), QSizeF(qMax<qreal>(line.naturalTextWidth(), 1.0), lineHeight));
        if (lineRect.adjusted(-2, 0, 2, 0).contains(QPointF(local))) {
            return true;
        }
    }

    return false;
}

QString MarkdownDelegate::linkAtPosition(const QStyleOptionViewItem &option,
                                         const QModelIndex &index,
                                         const QPoint &position) const
{
    const MarkdownRenderer::Block block = modelBlock(index);
    if (block.type == MarkdownRenderer::BlockType::SettingBlock) {
        return {};
    }
    if (block.type == MarkdownRenderer::BlockType::Table) {
        QStyleOptionViewItem viewOption(option);
        viewOption.index = index;
        const TableLayout tableLayout = buildTableLayout(viewOption, block);
        for (int rowIndex = 0; rowIndex < static_cast<int>(tableLayout.rows.size()); ++rowIndex) {
            const TableRowLayout &row = tableLayout.rows.at(rowIndex);
            if (rowIndex >= block.tableRows.size()) {
                continue;
            }

            for (int column = 0; column < static_cast<int>(row.cells.size()); ++column) {
                const TableCellLayout &cellLayout = row.cells.at(column);
                if (!cellLayout.textRect.adjusted(-2, 0, 2, 0).contains(position)) {
                    continue;
                }
                if (column >= block.tableRows.at(rowIndex).cells.size()) {
                    continue;
                }

                const MarkdownRenderer::TableCell &cell = block.tableRows.at(rowIndex).cells.at(column);
                if (cell.math || !cellLayout.textLayout) {
                    continue;
                }
                const QPoint local = position - cellLayout.textRect.topLeft();
                for (int lineIndex = 0; lineIndex < cellLayout.textLayout->lineCount(); ++lineIndex) {
                    const QTextLine line = cellLayout.textLayout->lineAt(lineIndex);
                    const qreal startX = line.cursorToX(line.textStart());
                    const qreal endX = line.cursorToX(line.textStart() + line.textLength());
                    const QRectF lineRect(line.position().x() + qMin(startX, endX),
                                          line.position().y(),
                                          qMax<qreal>(1.0, qAbs(endX - startX)),
                                          line.height());
                    if (!lineRect.adjusted(-2, 0, 2, 0).contains(QPointF(local))) {
                        continue;
                    }

                    const int cursor = line.xToCursor(local.x(), QTextLine::CursorBetweenCharacters);
                    for (const MarkdownRenderer::InlineSpan &span : cell.spans) {
                        if ((span.style & MarkdownRenderer::Link) &&
                            cursor >= span.start &&
                            cursor < span.start + span.length) {
                            return span.href;
                        }
                    }
                }
            }
        }

        return {};
    }

    BlockLayout layout = buildLayout(option, index);
    const QPoint local = position - layout.textRect.topLeft();

    for (int i = 0; i < layout.textLayout->lineCount(); ++i) {
        const QTextLine line = layout.textLayout->lineAt(i);
        const QRectF lineRect(line.position(), QSizeF(qMax<qreal>(line.naturalTextWidth(), 1.0), line.height()));
        if (!lineRect.adjusted(-2, 0, 2, 0).contains(QPointF(local))) {
            continue;
        }

        const int cursor = line.xToCursor(local.x(), QTextLine::CursorBetweenCharacters);
        for (const MarkdownRenderer::InlineSpan &span : block.spans) {
            if ((span.style & MarkdownRenderer::Link) &&
                cursor >= span.start &&
                cursor < span.start + span.length) {
                return span.href;
            }
        }
    }

    return {};
}

bool MarkdownDelegate::isCodeCopyButtonAtPosition(const QStyleOptionViewItem &option,
                                                  const QModelIndex &index,
                                                  const QPoint &position) const
{
    QStyleOptionViewItem viewOption(option);
    viewOption.index = index;
    const MarkdownRenderer::Block block = modelBlock(index);
    if (block.type != MarkdownRenderer::BlockType::CodeBlock) {
        return false;
    }

    return codeCopyButtonRect(viewOption).contains(position);
}

bool MarkdownDelegate::isCodeCopyButtonEnabledAtPosition(const QStyleOptionViewItem &option,
                                                         const QModelIndex &index,
                                                         const QPoint &position) const
{
    QStyleOptionViewItem viewOption(option);
    viewOption.index = index;
    if (!isCodeCopyButtonAtPosition(viewOption, index, position)) {
        return false;
    }

    return !isCodeBlockLoading(viewOption);
}

bool MarkdownDelegate::isSettingActionButtonAtPosition(const QStyleOptionViewItem &option,
                                                       const QModelIndex &index,
                                                       const QPoint &position) const
{
    QStyleOptionViewItem viewOption(option);
    viewOption.index = index;
    if (index.data(MarkdownDocumentModel::TypeRole).toInt() !=
            static_cast<int>(MarkdownRenderer::BlockType::SettingBlock)) {
        return false;
    }

    return settingActionHitRect(viewOption).contains(position);
}

QRect MarkdownDelegate::codeBlockLoadingUpdateRect(const QStyleOptionViewItem &option,
                                                   const QModelIndex &index) const
{
    QStyleOptionViewItem viewOption(option);
    viewOption.index = index;
    const MarkdownRenderer::Block block = modelBlock(index);
    if (block.type != MarkdownRenderer::BlockType::CodeBlock ||
            !index.data(MarkdownDocumentModel::CodeBlockLoadingRole).toBool()) {
        return {};
    }

    return codeLoadingSpinnerRect(viewOption).adjusted(-3, -3, 3, 3)
            .intersected(codeHeaderRect(viewOption));
}
