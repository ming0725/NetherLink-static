#ifndef TOPSEARCHWIDGET_H
#define TOPSEARCHWIDGET_H

#include <QWidget>
#include <QPushButton>
#include "LineEditComponent.h"

class TopSearchWidget : public QWidget {
    Q_OBJECT
public:
    explicit TopSearchWidget(QWidget *parent = nullptr);
    void resizeEvent(QResizeEvent *event) override;
protected:
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
private:
    LineEditComponent *searchBox;
    QPushButton *addButton;

    // 边距设置
    int topMargin = 24;
    int bottomMargin = 12;
    int leftMargin = 14;
    int rightMargin = 14;
    int spacing = 5; // 输入框和按钮之间的间距
};

#endif // TOPSEARCHWIDGET_H
