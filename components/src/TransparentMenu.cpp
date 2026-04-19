#include "TransparentMenu.h"
#include <QPainter>
#include <QGraphicsBlurEffect>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QScreen>
#include <QApplication>
#include <QStyleOption>
#include <QStyle>

TransparentMenu::TransparentMenu(QWidget *parent) : QMenu(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    
    // 设置样式
    QString style = QString(
        "QMenu {"
        "    background: transparent;"
        "    border: none;"
        "    padding: 4px;"
        "}"
        "QMenu::item {"
        "    background: transparent;"
        "    padding: 8px 24px;"
        "    border-radius: 4px;"
        "    margin: 2px 4px;"
        "    color: #333333;"
        "    font-size: 14px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: rgba(0, 0, 0, 0.1);"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background: rgba(0, 0, 0, 0.1);"
        "    margin: 4px 8px;"
        "}"
    );
    setStyleSheet(style);
}



void TransparentMenu::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制半透明白色背景
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 220));
    painter.drawRoundedRect(rect(), 8, 8);
    
    // 绘制菜单项
    QStyleOptionMenuItem menuOpt;
    menuOpt.initFrom(this);
    menuOpt.state = QStyle::State_None;
    menuOpt.checkType = QStyleOptionMenuItem::NotCheckable;
    menuOpt.maxIconWidth = 0;
    
    for (QAction* action : actions()) {
        if (action->isSeparator()) {
            // 绘制分隔线
            QRect sepRect = actionGeometry(action);
            painter.setPen(QPen(QColor(0, 0, 0, 20), 1));
            painter.drawLine(sepRect.left() + 8, sepRect.center().y(),
                           sepRect.right() - 8, sepRect.center().y());
        } else {
            // 绘制菜单项
            QRect itemRect = actionGeometry(action);
            bool isSelected = itemRect.contains(mapFromGlobal(QCursor::pos()));
            
            if (isSelected) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 15));
                painter.drawRoundedRect(itemRect.adjusted(4, 2, -4, -2), 4, 4);
            }
            
            painter.setPen(QColor(51, 51, 51));
            QFont font = painter.font();
            font.setPixelSize(14);
            painter.setFont(font);
            painter.drawText(itemRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                           QString("   %1").arg(action->text()));
        }
    }
}

