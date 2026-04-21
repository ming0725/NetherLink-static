#include "TopSearchWidget.h"
#include <QPainter>

TopSearchWidget::TopSearchWidget(QWidget *parent)
    : QWidget(parent)
{
    searchBox = new LineEditComponent(this);
    addButton = new QPushButton("+", this);
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setStyleSheet(R"(
        QPushButton {
            background-color: #F5F5F5;
            border: none;
            border-radius: 8px;
            font-size: 18px;
        }
        QPushButton:pressed {
            background-color: #D7D7D7;
        }
        QPushButton:hover {
            background-color: #EBEBEB;
        }
    )");
    searchBox->setFixedHeight(26);
    addButton->setFixedHeight(26);
    setFixedHeight(topMargin + searchBox->height() + bottomMargin);
}

void TopSearchWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    int W = width();
    int H = height();
    int contentH = H - topMargin - bottomMargin;
    int y = topMargin;

    // 1. 先放 addButton：右侧留白 rightMargin
    int btnX = W - rightMargin - contentH;
    addButton->setGeometry(btnX, y, contentH, contentH);

    // 2. 再放 searchBox：从 leftMargin 到 addButton 左边，减去 spacing
    int editX = leftMargin;
    int editW = btnX - spacing - leftMargin;
    searchBox->setGeometry(editX, y, editW, contentH);
}

void TopSearchWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xFFFFFF));
    painter.drawRect(rect());
}