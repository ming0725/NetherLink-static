#include "SelectableText.h"

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextBoundaryFinder>
#include <QTextDocument>
#include <QTextLayout>

#if defined(Q_OS_DARWIN)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace SelectableText {
namespace {

bool isAsciiWordChar(QChar ch)
{
    const ushort code = ch.unicode();
    return (code >= 'a' && code <= 'z') ||
            (code >= 'A' && code <= 'Z') ||
            (code >= '0' && code <= '9') ||
            code == '_' ||
            code == '\'';
}

bool isCjkChar(QChar ch)
{
    const ushort code = ch.unicode();
    return (code >= 0x3400 && code <= 0x4dbf) ||
            (code >= 0x4e00 && code <= 0x9fff) ||
            (code >= 0xf900 && code <= 0xfaff);
}

bool isSelectableWordChar(QChar ch)
{
    return ch.isLetterOrNumber() || ch == QLatin1Char('_') || isCjkChar(ch);
}

bool containsSelectableWordChar(const QString& text, int start, int end)
{
    for (int i = start; i < end; ++i) {
        if (isSelectableWordChar(text.at(i))) {
            return true;
        }
    }
    return false;
}

int characterIndexForWordSelection(const QString& text, int cursor)
{
    if (text.isEmpty()) {
        return -1;
    }

    const int forward = qBound(0, cursor, text.size() - 1);
    if (isSelectableWordChar(text.at(forward)) || isAsciiWordChar(text.at(forward))) {
        return forward;
    }

    if (cursor > 0) {
        const int backward = qMin(cursor - 1, text.size() - 1);
        if (isSelectableWordChar(text.at(backward)) || isAsciiWordChar(text.at(backward))) {
            return backward;
        }
    }

    return -1;
}

Range asciiWordRangeAt(const QString& text, int character)
{
    if (character < 0 || character >= text.size() || !isAsciiWordChar(text.at(character))) {
        return {};
    }

    int start = character;
    while (start > 0 && isAsciiWordChar(text.at(start - 1))) {
        --start;
    }

    int end = character + 1;
    while (end < text.size() && isAsciiWordChar(text.at(end))) {
        ++end;
    }

    while (start < end && text.at(start) == QLatin1Char('\'')) {
        ++start;
    }
    while (end > start && text.at(end - 1) == QLatin1Char('\'')) {
        --end;
    }

    return {start, end};
}

Range unicodeWordRangeAt(const QString& text, int character)
{
    QTextBoundaryFinder finder(QTextBoundaryFinder::Word, text);
    finder.toStart();

    int start = 0;
    while (true) {
        int end = finder.toNextBoundary();
        if (end < 0) {
            break;
        }

        if (character >= start && character < end) {
            while (start < end && !isSelectableWordChar(text.at(start))) {
                ++start;
            }
            while (end > start && !isSelectableWordChar(text.at(end - 1))) {
                --end;
            }

            if (character >= start && character < end &&
                    containsSelectableWordChar(text, start, end)) {
                return {start, end};
            }
            break;
        }

        start = end;
    }

    return {};
}

Range systemCjkWordRangeAt(const QString& text, int character)
{
#if defined(Q_OS_DARWIN)
    if (character < 0 || character >= text.size() || !isCjkChar(text.at(character))) {
        return {};
    }

    CFStringRef cfText = CFStringCreateWithCharacters(kCFAllocatorDefault,
                                                      reinterpret_cast<const UniChar*>(text.constData()),
                                                      text.size());
    if (!cfText) {
        return {};
    }

    CFLocaleRef locale = CFLocaleCreate(kCFAllocatorDefault, CFSTR("zh_CN"));
    CFStringTokenizerRef tokenizer = CFStringTokenizerCreate(kCFAllocatorDefault,
                                                            cfText,
                                                            CFRangeMake(0, text.size()),
                                                            kCFStringTokenizerUnitWord,
                                                            locale);

    Range range;
    if (tokenizer) {
        const CFStringTokenizerTokenType tokenType =
                CFStringTokenizerGoToTokenAtIndex(tokenizer, character);
        if (tokenType != kCFStringTokenizerTokenNone) {
            const CFRange tokenRange = CFStringTokenizerGetCurrentTokenRange(tokenizer);
            const int start = static_cast<int>(tokenRange.location);
            const int end = start + static_cast<int>(tokenRange.length);
            if (tokenRange.location != kCFNotFound &&
                    start >= 0 && end <= text.size() &&
                    character >= start && character < end &&
                    containsSelectableWordChar(text, start, end)) {
                range = {start, end};
            }
        }
    }

    if (tokenizer) {
        CFRelease(tokenizer);
    }
    if (locale) {
        CFRelease(locale);
    }
    CFRelease(cfText);

    return range;
#else
    Q_UNUSED(text)
    Q_UNUSED(character)
    return {};
#endif
}

Range fallbackCjkWordRangeAt(const QString& text, int character)
{
    if (character < 0 || character >= text.size() || !isCjkChar(text.at(character))) {
        return {};
    }

    if (character > 0 && isCjkChar(text.at(character - 1))) {
        return {character - 1, character + 1};
    }

    if (character + 1 < text.size() && isCjkChar(text.at(character + 1))) {
        return {character, character + 2};
    }

    return {character, character + 1};
}

} // namespace

bool Range::isValid() const
{
    return start >= 0 && end > start;
}

int Range::length() const
{
    return qMax(0, end - start);
}

void Selection::set(int anchor, int cursor, int textLength)
{
    const int boundedLength = qMax(0, textLength);
    m_anchor = qBound(0, anchor, boundedLength);
    m_cursor = qBound(0, cursor, boundedLength);
}

void Selection::clear()
{
    m_anchor = -1;
    m_cursor = -1;
}

bool Selection::isValid() const
{
    return m_anchor >= 0 && m_cursor >= 0;
}

bool Selection::hasSelection() const
{
    return isValid() && m_anchor != m_cursor;
}

bool Selection::contains(int cursor) const
{
    return hasSelection() && cursor >= start() && cursor <= end();
}

int Selection::start() const
{
    return qMin(m_anchor, m_cursor);
}

int Selection::end() const
{
    return qMax(m_anchor, m_cursor);
}

QString Selection::selectedText(const QString& text) const
{
    if (!hasSelection()) {
        return {};
    }

    return text.mid(start(), end() - start());
}

int lineCursorAt(const QTextDocument& document,
                 const QPointF& local,
                 int textLength,
                 bool allowOuterWhitespace)
{
    const int boundedLength = qMax(0, textLength);
    const qreal documentWidth = qMax<qreal>(1.0, document.textWidth());

    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        const QTextLayout* layout = block.layout();
        if (!layout) {
            continue;
        }

        const QRectF blockRect = document.documentLayout()->blockBoundingRect(block);
        for (int i = 0; i < layout->lineCount(); ++i) {
            const QTextLine line = layout->lineAt(i);
            if (!line.isValid()) {
                continue;
            }

            const qreal lineTop = blockRect.top() + line.y();
            const qreal lineBottom = lineTop + line.height();
            if (local.y() < lineTop || local.y() > lineBottom) {
                continue;
            }

            const int lineStart = qBound(0,
                                         block.position() + line.textStart(),
                                         boundedLength);
            const int lineEnd = qBound(0,
                                       block.position() + line.textStart() + line.textLength(),
                                       boundedLength);
            const qreal lineStartX = blockRect.left() + line.x() +
                    line.cursorToX(line.textStart());
            const qreal lineEndX = blockRect.left() + line.x() +
                    line.cursorToX(line.textStart() + line.textLength());

            if (local.x() < lineStartX) {
                return allowOuterWhitespace ? lineStart : -1;
            }
            if (local.x() >= lineEndX) {
                if (!allowOuterWhitespace && local.x() > documentWidth) {
                    return -1;
                }
                return lineEnd;
            }

            const int cursor = block.position() +
                    line.xToCursor(local.x() - lineStartX,
                                   QTextLine::CursorBetweenCharacters);
            return qBound(lineStart, cursor, lineEnd);
        }
    }

    return -1;
}

Range wordRangeAt(const QString& text, int cursor)
{
    const int character = characterIndexForWordSelection(text, cursor);
    if (character < 0) {
        return {};
    }

    const Range asciiRange = asciiWordRangeAt(text, character);
    if (asciiRange.isValid()) {
        return asciiRange;
    }

    if (isCjkChar(text.at(character))) {
        const Range systemCjkRange = systemCjkWordRangeAt(text, character);
        if (systemCjkRange.isValid()) {
            return systemCjkRange;
        }

        const Range fallbackCjkRange = fallbackCjkWordRangeAt(text, character);
        if (fallbackCjkRange.isValid()) {
            return fallbackCjkRange;
        }
    }

    const Range unicodeRange = unicodeWordRangeAt(text, character);
    if (unicodeRange.isValid() && unicodeRange.length() > 1) {
        return unicodeRange;
    }

    return unicodeRange;
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

} // namespace SelectableText
