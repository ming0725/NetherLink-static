#pragma once

#include <QPointF>
#include <QString>

class QTextDocument;

namespace SelectableText {

struct Range {
    int start = -1;
    int end = -1;

    bool isValid() const;
    int length() const;
};

class Selection
{
public:
    void set(int anchor, int cursor, int textLength);
    void clear();

    bool isValid() const;
    bool hasSelection() const;
    bool contains(int cursor) const;
    int start() const;
    int end() const;
    QString selectedText(const QString& text) const;

private:
    int m_anchor = -1;
    int m_cursor = -1;
};

Range wordRangeAt(const QString& text, int cursor);
int lineCursorAt(const QTextDocument& document,
                 const QPointF& local,
                 int textLength,
                 bool allowOuterWhitespace);
QString trimmedUrlText(const QString& text);

} // namespace SelectableText
