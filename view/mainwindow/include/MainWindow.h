#pragma once

#include "DefaultPage.h"
#include "ApplicationBar.h"
#include "FriendApplication.h"
#include "SystemWindow.h"
#include <QMouseEvent>
#include <QMoveEvent>
#include <QStackedWidget>

class QPushButton;

class MainWindow : public SystemWindow {
public:
    MainWindow(QWidget *parent = nullptr);
protected:
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject* watched, QEvent* ev) override;
    void showEvent(QShowEvent* event) override;
private slots:
    void onBarItemClicked(ApplicationBarItem* item);
private:
    void layoutWindow();

    ApplicationBar *appBar;
    QWidget *titleBar;
    QPushButton *btnMinimize;
    QPushButton *btnMaximize;
    QPushButton *btnClose;
    QIcon        iconClose, iconCloseHover;
    QStackedWidget* stack;
};
