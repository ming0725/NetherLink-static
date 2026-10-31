#ifndef MARKDOWNRENDERER_H
#define MARKDOWNRENDERER_H

#include <QColor>
#include <QFont>
#include <QList>
#include <QString>
#include <QVector>

class MarkdownRenderer
{
public:
    enum InlineStyle {
        NoInlineStyle = 0,
        Bold = 1 << 0,
        Italic = 1 << 1,
        Code = 1 << 2,
        Strike = 1 << 3,
        Link = 1 << 4,
        Math = 1 << 5
    };

    struct InlineSpan {
        int start = 0;
        int length = 0;
        int style = NoInlineStyle;
        QString href;
    };

    enum class BlockType {
        Paragraph,
        Heading,
        BlockQuote,
        UnorderedList,
        OrderedList,
        HorizontalRule,
        CodeBlock,
        MathBlock,
        Table,
        SettingBlock,
        Blank
    };

    enum class SettingControlType {
        Button,
        Slider
    };

    enum class TableAlignment {
        None,
        Left,
        Center,
        Right
    };

    struct TableCell {
        QString text;
        QVector<InlineSpan> spans;
        QString mathSource;
        bool math = false;
    };

    struct TableRow {
        QVector<TableCell> cells;
        bool header = false;
    };

    struct Block {
        BlockType type = BlockType::Paragraph;
        QString text;
        QString language;
        QVector<InlineSpan> spans;
        QVector<TableRow> tableRows;
        QVector<TableAlignment> tableAlignments;
        SettingControlType settingControl = SettingControlType::Button;
        QString settingLabel;
        QString settingAction;
        QString settingValueText;
        QString settingPreviousValueText;
        QString settingPreviousLabel;
        int settingMinimum = 0;
        int settingMaximum = 100;
        int settingValue = 0;
        int level = 0;
        int number = 0;
        bool quote = false;
        bool taskListItem = false;
        bool taskChecked = false;
        bool open = false;
    };

    static QList<Block> parseBlocks(const QString &markdown);
    static Block parseInlineText(Block block, const QString &markdownText);
};

#endif // MARKDOWNRENDERER_H
