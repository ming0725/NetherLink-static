#pragma once

#include "DefaultPage.h"
#include "ApplicationBar.h"
#include "FriendApplication.h"
#include "FramelessWindow.h"
#include <QSplitter>
#include <QScreen>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QApplication>
#include <QStackedWidget>

class MainWindow : public FramelessWindow {
public:
    MainWindow(QWidget *parent = nullptr);
protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject* watched, QEvent* ev) override;
    void showEvent(QShowEvent* event) override;
private slots:
    void onBarItemClicked(ApplicationBarItem* item);
private:
    ApplicationBar *appBar;
    QWidget *rightContent;
    QWidget *titleBar;
    int contentFixedWidth;
    QSplitter *splitter;
    QPushButton *btnMinimize;
    QPushButton *btnMaximize;
    QPushButton *btnClose;
    QIcon        iconClose, iconCloseHover;
    QStackedWidget* stack;
    bool m_windowEffectInitialized = false;
};
