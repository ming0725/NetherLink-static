#include "markdownrenderer.h"

#include <QHash>
#include <QRegularExpression>

namespace {

struct InlineParseResult {
    QString text;
    QVector<MarkdownRenderer::InlineSpan> spans;
};

struct BlockQuoteLine {
    bool isQuote = false;
    QString text;
};

BlockQuoteLine parseBlockQuoteLine(const QString &trimmed)
{
    static const QRegularExpression quoteExpression(QStringLiteral(R"(^>\s?(.*)$)"));
    const QRegularExpressionMatch quoteMatch = quoteExpression.match(trimmed);
    if (!quoteMatch.hasMatch()) {
        return {};
    }

    BlockQuoteLine result;
    result.isQuote = true;
    result.text = quoteMatch.captured(1);
    return result;
}

bool isDisplayMathBlockDelimiter(const QString &trimmed)
{
    return trimmed == QStringLiteral("$$") || trimmed == QStringLiteral("\\[");
}

bool isAutoDisplayMathEnvironment(const QString &name)
{
    return name == QStringLiteral("align") ||
           name == QStringLiteral("align*") ||
           name == QStringLiteral("equation") ||
           name == QStringLiteral("equation*");
}

bool isEscapedAt(const QString &source, int position)
{
    int slashCount = 0;
    for (int i = position - 1; i >= 0 && source.at(i) == QLatin1Char('\\'); --i) {
        ++slashCount;
    }
    return slashCount % 2 == 1;
}

int backtickRunLengthAt(const QString &source, int position)
{
    int length = 0;
    while (position + length < source.size() &&
           source.at(position + length) == QLatin1Char('`')) {
        ++length;
    }
    return length;
}

bool isInsideInlineCodeSpan(const QString &source, int position)
{
    int inlineCodeTicks = 0;
    for (int i = 0; i < qMin(position, source.size());) {
        if (source.at(i) != QLatin1Char('`') || isEscapedAt(source, i)) {
            ++i;
            continue;
        }

        const int ticks = backtickRunLengthAt(source, i);
        if (inlineCodeTicks == 0) {
            inlineCodeTicks = ticks;
        } else if (ticks == inlineCodeTicks) {
            inlineCodeTicks = 0;
        }
        i += ticks;
    }
    return inlineCodeTicks > 0;
}

struct DisplayMathEnvironmentSpan {
    bool found = false;
    int start = -1;
    int end = -1;
    QString name;
};

DisplayMathEnvironmentSpan findDisplayMathEnvironmentStart(const QString &line)
{
    static const QRegularExpression beginExpression(
            QStringLiteral(R"(\\begin\{([A-Za-z]+\*?)\})"));
    QRegularExpressionMatchIterator it = beginExpression.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch beginMatch = it.next();
        if (isInsideInlineCodeSpan(line, beginMatch.capturedStart(0))) {
            continue;
        }

        const QString name = beginMatch.captured(1);
        if (!isAutoDisplayMathEnvironment(name)) {
            continue;
        }

        DisplayMathEnvironmentSpan span;
        span.found = true;
        span.start = beginMatch.capturedStart(0);
        span.end = beginMatch.capturedEnd(0);
        span.name = name;
        return span;
    }
    return {};
}

DisplayMathEnvironmentSpan findClosedDisplayMathEnvironment(const QString &line)
{
    const DisplayMathEnvironmentSpan start = findDisplayMathEnvironmentStart(line);
    if (!start.found) {
        return {};
    }

    const QString endMarker = QStringLiteral("\\end{%1}").arg(start.name);
    const int endStart = line.indexOf(endMarker, start.end, Qt::CaseSensitive);
    if (endStart < 0) {
        return {};
    }

    DisplayMathEnvironmentSpan span = start;
    span.end = endStart + endMarker.size();
    return span;
}

bool nextNonEmptyLineStartsDisplayMathBlock(const QStringList &lines, int index)
{
    for (int i = index + 1; i < lines.size(); ++i) {
        QString line = lines.at(i);
        line.remove(QLatin1Char('\r'));
        const QString trimmed = line.trimmed();
        const BlockQuoteLine quoteLine = parseBlockQuoteLine(trimmed);
        const QString effectiveTrimmed = quoteLine.isQuote ? quoteLine.text.trimmed() : trimmed;
        if (effectiveTrimmed.isEmpty()) {
            continue;
        }
        return isDisplayMathBlockDelimiter(effectiveTrimmed) ||
               findClosedDisplayMathEnvironment(effectiveTrimmed).found;
    }
    return false;
}

bool isHorizontalRule(const QString &trimmed)
{
    static const QRegularExpression rule(QStringLiteral(R"(^([*\-_])(?:\s*\1){2,}\s*$)"));
    return rule.match(trimmed).hasMatch();
}

int listLevelFromIndent(const QString &indent)
{
    int columns = 0;
    for (const QChar ch : indent) {
        columns += ch == QLatin1Char('\t') ? 4 : 1;
    }
    return columns / 2;
}

void appendStyled(InlineParseResult &result, const QString &text, int style, const QString &href = {})
{
    const int start = result.text.size();
    result.text += text;
    if (!text.isEmpty() && style != MarkdownRenderer::NoInlineStyle) {
        MarkdownRenderer::InlineSpan span;
        span.start = start;
        span.length = text.size();
        span.style = style;
        span.href = href;
        result.spans.append(span);
    }
}

bool isEscaped(const QString &source, int position)
{
    int slashCount = 0;
    for (int i = position - 1; i >= 0 && source.at(i) == QLatin1Char('\\'); --i) {
        ++slashCount;
    }
    return slashCount % 2 == 1;
}

int backtickRunLength(const QString &source, int position)
{
    int length = 0;
    while (position + length < source.size() &&
           source.at(position + length) == QLatin1Char('`')) {
        ++length;
    }
    return length;
}

int findUnescapedDoubleDollarOutsideCode(const QString &source, int start = 0)
{
    int inlineCodeTicks = 0;
    for (int i = qMax(0, start); i + 1 < source.size();) {
        if (source.at(i) == QLatin1Char('`') && !isEscaped(source, i)) {
            const int ticks = backtickRunLength(source, i);
            if (inlineCodeTicks == 0) {
                inlineCodeTicks = ticks;
            } else if (ticks == inlineCodeTicks) {
                inlineCodeTicks = 0;
            }
            i += ticks;
            continue;
        }

        if (inlineCodeTicks > 0) {
            ++i;
            continue;
        }

        if (source.at(i) == QLatin1Char('$') &&
            source.at(i + 1) == QLatin1Char('$') &&
            !isEscaped(source, i)) {
            return i;
        }
        ++i;
    }
    return -1;
}

bool hasUnescapedPipe(const QString &line);

QStringList splitDisplayMathDelimiterLines(const QString &markdown)
{
    QStringList result;
    bool inCodeBlock = false;

    const QStringList lines = markdown.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line.remove(QLatin1Char('\r'));
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("```"))) {
            result.append(line);
            inCodeBlock = !inCodeBlock;
            continue;
        }

        if (inCodeBlock) {
            result.append(line);
            continue;
        }

        if (hasUnescapedPipe(line) && findUnescapedDoubleDollarOutsideCode(line) < 0) {
            result.append(line);
            continue;
        }

        int cursor = 0;
        bool splitLine = false;
        while (true) {
            const int delimiter = findUnescapedDoubleDollarOutsideCode(line, cursor);
            if (delimiter < 0) {
                break;
            }

            const QString before = line.mid(cursor, delimiter - cursor);
            if (!before.isEmpty()) {
                result.append(before);
            }
            result.append(QStringLiteral("$$"));
            cursor = delimiter + 2;
            splitLine = true;
        }

        if (!splitLine) {
            result.append(line);
            continue;
        }

        const QString after = line.mid(cursor);
        if (!after.isEmpty()) {
            result.append(after);
        }
    }

    return result;
}

int findInlineMathEnd(const QString &source, int start)
{
    for (int i = start + 1; i < source.size(); ++i) {
        if (source.at(i) != QLatin1Char('$') || isEscaped(source, i)) {
            continue;
        }
        if (i + 1 < source.size() && source.at(i + 1) == QLatin1Char('$')) {
            ++i;
            continue;
        }
        return i;
    }
    return -1;
}

InlineParseResult parseInline(const QString &source)
{
    InlineParseResult result;

    for (int i = 0; i < source.size();) {
        if (source.mid(i, 2) == QStringLiteral("**")) {
            const int end = source.indexOf(QStringLiteral("**"), i + 2);
            if (end > i + 2) {
                appendStyled(result, source.mid(i + 2, end - i - 2), MarkdownRenderer::Bold);
                i = end + 2;
                continue;
            }
        }

        if (source.mid(i, 2) == QStringLiteral("~~")) {
            const int end = source.indexOf(QStringLiteral("~~"), i + 2);
            if (end > i + 2) {
                appendStyled(result, source.mid(i + 2, end - i - 2), MarkdownRenderer::Strike);
                i = end + 2;
                continue;
            }
        }

        if (source.at(i) == QLatin1Char('`')) {
            const int end = source.indexOf(QLatin1Char('`'), i + 1);
            if (end > i + 1) {
                appendStyled(result, source.mid(i + 1, end - i - 1), MarkdownRenderer::Code);
                i = end + 1;
                continue;
            }
        }

        if (source.at(i) == QLatin1Char('$') &&
            (i + 1 >= source.size() || source.at(i + 1) != QLatin1Char('$')) &&
            !isEscaped(source, i)) {
            const int end = findInlineMathEnd(source, i);
            if (end > i + 1) {
                const QString mathSource = source.mid(i + 1, end - i - 1).trimmed();
                if (!mathSource.isEmpty()) {
                    appendStyled(result, mathSource, MarkdownRenderer::Math, mathSource);
                    i = end + 1;
                    continue;
                }
            }
        }

        if (source.at(i) == QLatin1Char('[')) {
            const int closeText = source.indexOf(QStringLiteral("]("), i + 1);
            if (closeText > i + 1) {
                const int closeUrl = source.indexOf(QLatin1Char(')'), closeText + 2);
                if (closeUrl > closeText + 2) {
                    appendStyled(result,
                                 source.mid(i + 1, closeText - i - 1),
                                 MarkdownRenderer::Link,
                                 source.mid(closeText + 2, closeUrl - closeText - 2));
                    i = closeUrl + 1;
                    continue;
                }
            }
        }

        if (source.at(i) == QLatin1Char('*') &&
            (i + 1 >= source.size() || source.at(i + 1) != QLatin1Char('*'))) {
            const int end = source.indexOf(QLatin1Char('*'), i + 1);
            if (end > i + 1) {
                appendStyled(result, source.mid(i + 1, end - i - 1), MarkdownRenderer::Italic);
                i = end + 1;
                continue;
            }
        }

        result.text += source.at(i);
        ++i;
    }

    return result;
}

bool hasUnescapedPipe(const QString &line)
{
    bool escaped = false;
    for (const QChar ch : line) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            escaped = true;
            continue;
        }
        if (ch == QLatin1Char('|')) {
            return true;
        }
    }
    return false;
}

QString strippedWholeCellMathSource(QString source)
{
    source = source.trimmed();
    if (source.size() >= 4 &&
        source.startsWith(QStringLiteral("$$")) &&
        source.endsWith(QStringLiteral("$$"))) {
        return source.mid(2, source.size() - 4).trimmed();
    }
    if (source.size() >= 4 &&
        source.startsWith(QStringLiteral("\\[")) &&
        source.endsWith(QStringLiteral("\\]"))) {
        return source.mid(2, source.size() - 4).trimmed();
    }
    if (source.size() >= 4 &&
        source.startsWith(QStringLiteral("\\(")) &&
        source.endsWith(QStringLiteral("\\)"))) {
        return source.mid(2, source.size() - 4).trimmed();
    }
    if (source.size() >= 2 &&
        source.startsWith(QLatin1Char('$')) &&
        source.endsWith(QLatin1Char('$')) &&
        !source.startsWith(QStringLiteral("$$"))) {
        return source.mid(1, source.size() - 2).trimmed();
    }
    return {};
}

QStringList splitTableRow(const QString &line)
{
    QString trimmed = line.trimmed();
    if (trimmed.startsWith(QLatin1Char('|'))) {
        trimmed.remove(0, 1);
    }
    if (trimmed.endsWith(QLatin1Char('|')) &&
        (trimmed.size() < 2 || trimmed.at(trimmed.size() - 2) != QLatin1Char('\\'))) {
        trimmed.chop(1);
    }

    QStringList cells;
    QString cell;
    for (int i = 0; i < trimmed.size(); ++i) {
        const QChar ch = trimmed.at(i);
        if (ch == QLatin1Char('\\') &&
            i + 1 < trimmed.size() &&
            trimmed.at(i + 1) == QLatin1Char('|')) {
            cell += QLatin1Char('|');
            ++i;
            continue;
        }
        if (ch == QLatin1Char('|')) {
            cells.append(cell.trimmed());
            cell.clear();
            continue;
        }
        cell += ch;
    }

    cells.append(cell.trimmed());
    return cells;
}

bool isTableSeparatorCell(QString cell)
{
    cell.remove(QLatin1Char(' '));
    static const QRegularExpression separator(QStringLiteral(R"(^:?-{3,}:?$)"));
    return separator.match(cell).hasMatch();
}

MarkdownRenderer::TableAlignment tableAlignment(QString cell)
{
    cell.remove(QLatin1Char(' '));
    const bool left = cell.startsWith(QLatin1Char(':'));
    const bool right = cell.endsWith(QLatin1Char(':'));
    if (left && right) {
        return MarkdownRenderer::TableAlignment::Center;
    }
    if (left) {
        return MarkdownRenderer::TableAlignment::Left;
    }
    if (right) {
        return MarkdownRenderer::TableAlignment::Right;
    }
    return MarkdownRenderer::TableAlignment::None;
}

bool isTableSeparatorRow(const QString &line)
{
    if (!hasUnescapedPipe(line)) {
        return false;
    }

    const QStringList cells = splitTableRow(line);
    if (cells.size() < 2) {
        return false;
    }

    for (const QString &cell : cells) {
        if (!isTableSeparatorCell(cell)) {
            return false;
        }
    }
    return true;
}

bool isTableStart(const QStringList &lines, int index)
{
    if (index + 1 >= lines.size() || !hasUnescapedPipe(lines.at(index))) {
        return false;
    }

    const QStringList headerCells = splitTableRow(lines.at(index));
    return headerCells.size() >= 2 && isTableSeparatorRow(lines.at(index + 1));
}

MarkdownRenderer::TableCell tableCell(const QString &source)
{
    MarkdownRenderer::TableCell cell;
    const QString mathSource = strippedWholeCellMathSource(source);
    if (!mathSource.isEmpty()) {
        cell.text = mathSource;
        cell.mathSource = mathSource;
        cell.math = true;
        return cell;
    }

    const InlineParseResult parsed = parseInline(source);
    cell.text = parsed.text;
    cell.spans = parsed.spans;
    return cell;
}

MarkdownRenderer::TableRow tableRow(const QStringList &sources, int columnCount, bool header)
{
    MarkdownRenderer::TableRow row;
    row.header = header;
    for (int column = 0; column < columnCount; ++column) {
        row.cells.append(tableCell(column < sources.size() ? sources.at(column) : QString()));
    }
    return row;
}

QString tableRowText(const MarkdownRenderer::TableRow &row)
{
    QStringList cells;
    for (const MarkdownRenderer::TableCell &cell : row.cells) {
        cells.append(cell.text);
    }
    return cells.join(QLatin1Char('\t'));
}

MarkdownRenderer::Block parseTable(const QStringList &lines, int *index)
{
    const QStringList headerCells = splitTableRow(lines.at(*index));
    const QStringList separatorCells = splitTableRow(lines.at(*index + 1));
    int columnCount = qMax(headerCells.size(), separatorCells.size());

    QVector<QStringList> bodyRows;
    int cursor = *index + 2;
    while (cursor < lines.size()) {
        QString line = lines.at(cursor);
        line.remove(QLatin1Char('\r'));
        if (line.trimmed().isEmpty() || !hasUnescapedPipe(line) || isTableSeparatorRow(line)) {
            break;
        }

        const QStringList cells = splitTableRow(line);
        columnCount = qMax(columnCount, cells.size());
        bodyRows.append(cells);
        ++cursor;
    }

    MarkdownRenderer::Block block;
    block.type = MarkdownRenderer::BlockType::Table;
    block.tableAlignments.reserve(columnCount);
    for (int column = 0; column < columnCount; ++column) {
        block.tableAlignments.append(column < separatorCells.size()
                                         ? tableAlignment(separatorCells.at(column))
                                         : MarkdownRenderer::TableAlignment::None);
    }

    block.tableRows.append(tableRow(headerCells, columnCount, true));
    for (const QStringList &cells : bodyRows) {
        block.tableRows.append(tableRow(cells, columnCount, false));
    }

    QStringList rowTexts;
    for (const MarkdownRenderer::TableRow &row : block.tableRows) {
        rowTexts.append(tableRowText(row));
    }
    block.text = rowTexts.join(QLatin1Char('\n'));

    *index = cursor - 1;
    return block;
}

MarkdownRenderer::Block paragraphBlock(QStringList &paragraphLines)
{
    MarkdownRenderer::Block block;
    block.type = MarkdownRenderer::BlockType::Paragraph;
    block = MarkdownRenderer::parseInlineText(block, paragraphLines.join(QLatin1Char(' ')).trimmed());
    paragraphLines.clear();
    return block;
}

void applyTaskListState(MarkdownRenderer::Block &block, QString *text)
{
    if (!text) {
        return;
    }

    static const QRegularExpression taskExpression(QStringLiteral(R"(^\[( |x|X)\]\s+(.+)$)"));
    const QRegularExpressionMatch taskMatch = taskExpression.match(*text);
    if (taskMatch.hasMatch()) {
        block.taskListItem = true;
        block.taskChecked = taskMatch.captured(1).compare(QStringLiteral("x"), Qt::CaseInsensitive) == 0;
        *text = taskMatch.captured(2).trimmed();
    }
}

MarkdownRenderer::Block parseUnorderedListBlock(const QString &indent, const QString &source)
{
    MarkdownRenderer::Block block;
    block.type = MarkdownRenderer::BlockType::UnorderedList;
    block.level = listLevelFromIndent(indent);

    QString text = source.trimmed();
    applyTaskListState(block, &text);

    return MarkdownRenderer::parseInlineText(block, text);
}

QString codeLanguageFromFence(const QString &trimmedFence)
{
    QString info = trimmedFence.mid(3).trimmed();
    if (info.startsWith(QStringLiteral("{.")) && info.endsWith(QLatin1Char('}'))) {
        info = info.mid(2, info.size() - 3).trimmed();
    }

    static const QRegularExpression whitespace(QStringLiteral(R"(\s+)"));
    const QString language = info.split(whitespace, Qt::SkipEmptyParts).value(0).trimmed();
    return language.startsWith(QLatin1Char('.')) ? language.mid(1) : language;
}

QString displayBlockText(const QStringList &lines)
{
    QString text = lines.join(QLatin1Char('\n'));
    while (!text.isEmpty() && text.back().isSpace()) {
        text.chop(1);
    }
    return text;
}

QString normalizedSettingKey(QString key)
{
    return key.trimmed().toLower();
}

int settingIntValue(const QHash<QString, QString> &fields, const QString &key, int fallback)
{
    bool ok = false;
    const int value = fields.value(key).toInt(&ok);
    return ok ? value : fallback;
}

MarkdownRenderer::Block parseSettingBlock(const QStringList &lines)
{
    QHash<QString, QString> fields;
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        int separator = line.indexOf(QLatin1Char(':'));
        if (separator < 0) {
            separator = line.indexOf(QStringLiteral("："));
        }
        if (separator <= 0) {
            continue;
        }

        fields.insert(normalizedSettingKey(line.left(separator)),
                      line.mid(separator + 1).trimmed());
    }

    MarkdownRenderer::Block block;
    block.type = MarkdownRenderer::BlockType::SettingBlock;
    block.language = QStringLiteral("setting");
    block.settingControl = fields.value(QStringLiteral("type")).compare(QStringLiteral("slider"),
                                                                         Qt::CaseInsensitive) == 0
            ? MarkdownRenderer::SettingControlType::Slider
            : MarkdownRenderer::SettingControlType::Button;
    block.settingLabel = fields.value(QStringLiteral("label"), QStringLiteral("设置"));
    block.settingAction = fields.value(QStringLiteral("action"));
    block.settingValueText = fields.value(QStringLiteral("value"));
    block.settingPreviousValueText = fields.value(QStringLiteral("previous"));
    block.settingPreviousLabel = fields.value(QStringLiteral("previous-label"));
    block.settingMinimum = settingIntValue(fields, QStringLiteral("min"), 0);
    block.settingMaximum = settingIntValue(fields, QStringLiteral("max"), 100);
    if (block.settingMinimum > block.settingMaximum) {
        qSwap(block.settingMinimum, block.settingMaximum);
    }
    block.settingValue = qBound(block.settingMinimum,
                                settingIntValue(fields, QStringLiteral("value"), block.settingMinimum),
                                block.settingMaximum);
    block.text = block.settingLabel;
    return block;
}

} // namespace

QList<MarkdownRenderer::Block> MarkdownRenderer::parseBlocks(const QString &markdown)
{
    QList<Block> blocks;
    QStringList paragraphLines;
    bool inCodeBlock = false;
    bool inMathBlock = false;
    bool mathBlockQuoted = false;
    bool mathBlockInlineEndMarker = false;
    bool pendingQuoteContinuation = false;
    QStringList codeLines;
    QStringList mathLines;
    QString mathBlockEndMarker;
    QString codeLanguage;

    const QStringList lines = splitDisplayMathDelimiterLines(markdown);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        QString line = lines.at(lineIndex);
        line.remove(QLatin1Char('\r'));
        const QString trimmed = line.trimmed();
        const BlockQuoteLine quoteLine = parseBlockQuoteLine(trimmed);
        const QString effectiveMathTrimmed = quoteLine.isQuote
                                                ? quoteLine.text.trimmed()
                                                : trimmed;

        if (trimmed.startsWith(QStringLiteral("```"))) {
            if (!inCodeBlock) {
                if (!paragraphLines.isEmpty()) {
                    blocks.append(paragraphBlock(paragraphLines));
                }
                codeLines.clear();
                codeLanguage = codeLanguageFromFence(trimmed);
                inCodeBlock = true;
            } else {
                if (codeLanguage.compare(QStringLiteral("setting"), Qt::CaseInsensitive) == 0) {
                    blocks.append(parseSettingBlock(codeLines));
                } else {
                    Block block;
                    block.type = BlockType::CodeBlock;
                    block.text = displayBlockText(codeLines);
                    block.language = codeLanguage;
                    blocks.append(block);
                }
                codeLines.clear();
                codeLanguage.clear();
                inCodeBlock = false;
            }
            continue;
        }

        if (inCodeBlock) {
            codeLines.append(line);
            continue;
        }

        if (inMathBlock && mathBlockInlineEndMarker) {
            const QString mathLine = mathBlockQuoted && quoteLine.isQuote ? quoteLine.text : line;
            const int endStart = mathLine.indexOf(mathBlockEndMarker, 0, Qt::CaseSensitive);
            if (endStart < 0) {
                mathLines.append(mathLine);
                continue;
            }

            mathLines.append(mathLine.left(endStart + mathBlockEndMarker.size()));
            Block block;
            block.type = BlockType::MathBlock;
            block.text = displayBlockText(mathLines);
            block.quote = mathBlockQuoted;
            blocks.append(block);
            mathLines.clear();

            const QString after = mathLine.mid(endStart + mathBlockEndMarker.size()).trimmed();
            if (!after.isEmpty()) {
                Block afterBlock;
                afterBlock.type = quoteLine.isQuote ? BlockType::BlockQuote : BlockType::Paragraph;
                afterBlock.quote = quoteLine.isQuote;
                blocks.append(parseInlineText(afterBlock, after));
            }

            mathBlockEndMarker.clear();
            mathBlockQuoted = false;
            mathBlockInlineEndMarker = false;
            inMathBlock = false;
            continue;
        }

        if ((!inMathBlock && isDisplayMathBlockDelimiter(effectiveMathTrimmed)) ||
            (inMathBlock && effectiveMathTrimmed == mathBlockEndMarker)) {
            if (!inMathBlock) {
                if (!paragraphLines.isEmpty()) {
                    blocks.append(paragraphBlock(paragraphLines));
                }
                mathLines.clear();
                mathBlockEndMarker = effectiveMathTrimmed == QStringLiteral("$$")
                                         ? QStringLiteral("$$")
                                         : QStringLiteral("\\]");
                mathBlockQuoted = quoteLine.isQuote || pendingQuoteContinuation;
                mathBlockInlineEndMarker = false;
                pendingQuoteContinuation = false;
                inMathBlock = true;
            } else {
                Block block;
                block.type = BlockType::MathBlock;
                block.text = displayBlockText(mathLines);
                block.quote = mathBlockQuoted;
                blocks.append(block);
                mathLines.clear();
                mathBlockEndMarker.clear();
                mathBlockQuoted = false;
                mathBlockInlineEndMarker = false;
                inMathBlock = false;
            }
            continue;
        }

        if (inMathBlock) {
            mathLines.append(mathBlockQuoted && quoteLine.isQuote ? quoteLine.text : line);
            continue;
        }

        const QString displayMathLine = quoteLine.isQuote ? quoteLine.text : line;
        const DisplayMathEnvironmentSpan displayMathSpan =
                findClosedDisplayMathEnvironment(displayMathLine);
        if (displayMathSpan.found) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }

            const QString before = displayMathLine.left(displayMathSpan.start).trimmed();
            if (!before.isEmpty()) {
                Block block;
                block.type = quoteLine.isQuote ? BlockType::BlockQuote : BlockType::Paragraph;
                block.quote = quoteLine.isQuote;
                blocks.append(parseInlineText(block, before));
            }

            Block mathBlock;
            mathBlock.type = BlockType::MathBlock;
            mathBlock.text = displayMathLine.mid(displayMathSpan.start,
                                                 displayMathSpan.end - displayMathSpan.start).trimmed();
            mathBlock.quote = quoteLine.isQuote || pendingQuoteContinuation;
            blocks.append(mathBlock);

            const QString after = displayMathLine.mid(displayMathSpan.end).trimmed();
            if (!after.isEmpty()) {
                Block block;
                block.type = quoteLine.isQuote ? BlockType::BlockQuote : BlockType::Paragraph;
                block.quote = quoteLine.isQuote;
                blocks.append(parseInlineText(block, after));
            }
            pendingQuoteContinuation = false;
            continue;
        }

        const DisplayMathEnvironmentSpan displayMathStart =
                findDisplayMathEnvironmentStart(displayMathLine);
        if (displayMathStart.found) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }

            const QString before = displayMathLine.left(displayMathStart.start).trimmed();
            if (!before.isEmpty()) {
                Block block;
                block.type = quoteLine.isQuote ? BlockType::BlockQuote : BlockType::Paragraph;
                block.quote = quoteLine.isQuote;
                blocks.append(parseInlineText(block, before));
            }

            mathLines.clear();
            mathLines.append(displayMathLine.mid(displayMathStart.start));
            mathBlockEndMarker = QStringLiteral("\\end{%1}").arg(displayMathStart.name);
            mathBlockQuoted = quoteLine.isQuote || pendingQuoteContinuation;
            mathBlockInlineEndMarker = true;
            pendingQuoteContinuation = false;
            inMathBlock = true;
            continue;
        }

        if (quoteLine.isQuote && quoteLine.text.trimmed().isEmpty()) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }
            if (nextNonEmptyLineStartsDisplayMathBlock(lines, lineIndex)) {
                pendingQuoteContinuation = true;
            } else {
                Block block;
                block.type = BlockType::BlockQuote;
                block.quote = true;
                blocks.append(parseInlineText(block, QString()));
                pendingQuoteContinuation = false;
            }
            continue;
        }

        if (!quoteLine.isQuote && !trimmed.isEmpty()) {
            pendingQuoteContinuation = false;
        }

        if (isTableStart(lines, lineIndex)) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }
            blocks.append(parseTable(lines, &lineIndex));
            continue;
        }

        if (trimmed.isEmpty()) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }
            continue;
        }

        if (isHorizontalRule(trimmed)) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }
            Block block;
            block.type = BlockType::HorizontalRule;
            blocks.append(block);
            continue;
        }

        static const QRegularExpression headingExpression(QStringLiteral(R"(^(#{1,6})\s+(.+)$)"));
        const QRegularExpressionMatch headingMatch = headingExpression.match(trimmed);
        if (headingMatch.hasMatch()) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }
            Block block;
            block.type = BlockType::Heading;
            block.level = headingMatch.captured(1).size();
            blocks.append(parseInlineText(block, headingMatch.captured(2).trimmed()));
            continue;
        }

        if (quoteLine.isQuote) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }
            Block block;
            block.type = BlockType::BlockQuote;
            block.quote = true;
            blocks.append(parseInlineText(block, quoteLine.text.trimmed()));
            pendingQuoteContinuation = false;
            continue;
        }

        static const QRegularExpression unorderedExpression(QStringLiteral(R"(^(\s*)[-+*]\s+(.+)$)"));
        const QRegularExpressionMatch unorderedMatch = unorderedExpression.match(line);
        if (unorderedMatch.hasMatch()) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }
            blocks.append(parseUnorderedListBlock(unorderedMatch.captured(1),
                                                  unorderedMatch.captured(2)));
            continue;
        }

        static const QRegularExpression orderedExpression(QStringLiteral(R"(^(\s*)(\d+)[.)]\s+(.+)$)"));
        const QRegularExpressionMatch orderedMatch = orderedExpression.match(line);
        if (orderedMatch.hasMatch()) {
            if (!paragraphLines.isEmpty()) {
                blocks.append(paragraphBlock(paragraphLines));
            }
            Block block;
            block.type = BlockType::OrderedList;
            block.level = listLevelFromIndent(orderedMatch.captured(1));
            block.number = orderedMatch.captured(2).toInt();
            QString text = orderedMatch.captured(3).trimmed();
            applyTaskListState(block, &text);
            blocks.append(parseInlineText(block, text));
            continue;
        }

        paragraphLines.append(trimmed);
    }

    if (inCodeBlock) {
        if (codeLanguage.compare(QStringLiteral("setting"), Qt::CaseInsensitive) == 0) {
            blocks.append(parseSettingBlock(codeLines));
        } else {
            Block block;
            block.type = BlockType::CodeBlock;
            block.text = displayBlockText(codeLines);
            block.language = codeLanguage;
            block.open = true;
            blocks.append(block);
        }
    }

    if (inMathBlock) {
        Block block;
        block.type = BlockType::MathBlock;
        block.text = displayBlockText(mathLines);
        block.quote = mathBlockQuoted;
        blocks.append(block);
        mathBlockEndMarker.clear();
    }

    if (!paragraphLines.isEmpty()) {
        blocks.append(paragraphBlock(paragraphLines));
    }

    return blocks;
}

MarkdownRenderer::Block MarkdownRenderer::parseInlineText(Block block, const QString &markdownText)
{
    const InlineParseResult result = parseInline(markdownText);
    block.text = result.text;
    block.spans = result.spans;
    return block;
}
