#pragma once

#include <QColor>
#include <QMenu>

class QAction;
class QPaintEvent;

class StyledActionMenu : public QMenu
{
    Q_OBJECT

public:
    explicit StyledActionMenu(QWidget* parent = nullptr);

    StyledActionMenu* addStyledMenu(const QString& title);

    void setItemHoverColor(const QColor& color);
    void setSeparatorColor(const QColor& color);

    static void setActionTextColor(QAction* action, const QColor& color);
    static void setActionHoverTextColor(QAction* action, const QColor& color);
    static void setActionHoverBackgroundColor(QAction* action, const QColor& color);
    static void setActionColors(QAction* action, const QColor& textColor,
                                const QColor& hoverTextColor);
    static void setActionColors(QAction* action, const QColor& textColor,
                                const QColor& hoverTextColor,
                                const QColor& hoverBackgroundColor);

protected:
    void paintEvent(QPaintEvent* event) override;
};
