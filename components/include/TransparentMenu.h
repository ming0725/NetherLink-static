#pragma once
#include <QMenu>
#include <QWidget>
#include <QPixmap>

class TransparentMenu : public QMenu
{
    Q_OBJECT
public:
    explicit TransparentMenu(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};

