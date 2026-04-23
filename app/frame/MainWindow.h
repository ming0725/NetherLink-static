#pragma once

#include "DefaultPage.h"
#include "ApplicationBar.h"
#include "platform/SystemWindow.h"
#include <QPointer>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QStackedWidget>

class QPushButton;
class MessageApplication;
class FriendApplication;
class PostApplication;
class AiChatApplication;

class MainWindow : public SystemWindow {
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
protected:
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject* watched, QEvent* ev) override;
    void showEvent(QShowEvent* event) override;
private slots:
    void onBarItemClicked(ApplicationBarItem* item);
private:
    QWidget* createPlaceholderPage() const;
    void replaceStackPage(int index, QWidget* widget);
    void ensureApplicationLoaded(int index);
    void layoutWindow();

    ApplicationBar *appBar;
    QWidget *titleBar;
    QPushButton *btnMinimize;
    QPushButton *btnMaximize;
    QPushButton *btnClose;
    QIcon        iconClose, iconCloseHover;
    QStackedWidget* stack;
    MessageApplication* m_messageApp = nullptr;
    FriendApplication* m_friendApp = nullptr;
    PostApplication* m_postApp = nullptr;
    AiChatApplication* m_aiChatApp = nullptr;
    DefaultPage* m_defaultPage = nullptr;
    QPointer<QWidget> m_pendingFocusClear;
};
