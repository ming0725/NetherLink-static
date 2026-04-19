#include "MainWindow.h"
#include "MessageApplication.h"
#include "AiChatApplication.h"
#include "PostApplication.h"
#include "CurrentUser.h"
#include "LineEditComponent.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent)
    : SystemWindow(parent)
    , stack(new QStackedWidget(this))
    , btnMinimize(nullptr)
    , btnMaximize(nullptr)
    , btnClose(nullptr)
{
    // 窗口基础设置
    resize(950, 650);
    setMinimumHeight(525);
    setMinimumWidth(685);
    setAttribute(Qt::WA_TranslucentBackground);
    setBackdropColor(QColor(248, 248, 252, 92));

    CurrentUser::instance().setMainWindow(this);
    appBar = new ApplicationBar(this);
    appBar->setFixedWidth(54);

    titleBar = new QWidget(this);
    titleBar->setFixedHeight(32);
    titleBar->setAttribute(Qt::WA_StyledBackground, false);

#ifndef Q_OS_MACOS
    btnMinimize = new QPushButton(titleBar);
    btnMaximize = new QPushButton(titleBar);
    btnClose = new QPushButton(titleBar);
    iconClose = QIcon(":/resources/icon/close.png");
    iconCloseHover = QIcon(":/resources/icon/hovered_close.png");
    btnMinimize->setIcon(QIcon(":/resources/icon/minimize.png"));
    btnMaximize->setIcon(QIcon(":/resources/icon/maximize.png"));

    btnMinimize->setIconSize(QSize(16, 16));
    btnMaximize->setIconSize(QSize(16, 16));
    btnClose->setIconSize(QSize(16, 16));

    btnMinimize->setFixedSize(32, 32);
    btnMaximize->setFixedSize(32, 32);
    btnClose->setFixedSize(32, 32);

    auto btnStyle = R"(
        QPushButton {
            background-color: transparent;
            border: none;
        }
        QPushButton:hover {
            background-color: #E9E9E9;
        }
    )";
    btnMinimize->setStyleSheet(btnStyle);
    btnMaximize->setStyleSheet(btnStyle);
    btnClose->setStyleSheet(R"(
    QPushButton {
        background-color: transparent;
        border: none;
        qproperty-icon: url(:/resources/icon/close.png);
    }
    QPushButton:hover {
        background-color: #C42B1C;
        border: none;
    }
    )");

    btnClose->installEventFilter(this);

    auto hl = new QHBoxLayout(titleBar);
    hl->setContentsMargins(0,0,0,0);
    hl->addStretch();
    hl->addWidget(btnMinimize);
    hl->addWidget(btnMaximize);
    hl->addWidget(btnClose);
    hl->setSpacing(0);

    connect(btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(btnMaximize, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
    });
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
#endif

    setDragTitleBar(titleBar);


    stack->addWidget(new MessageApplication(this));
    stack->addWidget(new FriendApplication(this));
    stack->addWidget(new PostApplication(this));
    stack->addWidget(new AiChatApplication(this));
    stack->addWidget(new DefaultPage(this));
    // 默认选中第一个
    stack->setCurrentIndex(0);

    // 绑定点击信号，切换栈页
    connect(appBar, &ApplicationBar::applicationClicked,
            this, &MainWindow::onBarItemClicked);

    QScreen* screen = QGuiApplication::primaryScreen();
    QRect   sg     = screen->geometry();
    int     cx     = (sg.width()  - width())  / 2;
    int     cy     = (sg.height() - height()) / 2;
    move(cx, cy);
}

void MainWindow::showEvent(QShowEvent* event)
{
    SystemWindow::showEvent(event);
    layoutWindow();
}


void MainWindow::resizeEvent(QResizeEvent* event)
{
    SystemWindow::resizeEvent(event);
    layoutWindow();
}

void MainWindow::moveEvent(QMoveEvent* event)
{
    SystemWindow::moveEvent(event);
#ifdef Q_OS_MACOS
    layoutWindow();
#endif
}


void MainWindow::mousePressEvent(QMouseEvent* event)
{
    QWidget* fw = QApplication::focusWidget();
    if (qobject_cast<LineEditComponent*>(fw)
        || qobject_cast<QLineEdit*>(fw)) {
        fw->clearFocus();
    }
    SystemWindow::mousePressEvent(event);
}


void MainWindow::onBarItemClicked(ApplicationBarItem *item)
{
    int idx = appBar->indexOfTopItem(item);
    if (idx >= 0 && idx < stack->count()) {
        stack->setCurrentIndex(idx);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *ev) {
    if (btnClose && watched == btnClose) {
        if (ev->type() == QEvent::Enter) {
            btnClose->setIcon(iconCloseHover);
        }
        else if (ev->type() == QEvent::Leave) {
            btnClose->setIcon(iconClose);
        }
    }
    return SystemWindow::eventFilter(watched, ev);
}

void MainWindow::layoutWindow()
{
    const int w = width();
    const int h = height();
    const bool useSystemTitleButtons = usesSystemTitleButtons();
    const int titleInset = useSystemTitleButtons ? topInset() : 0;
    const int titleBarHeight = qMax(32, titleInset);
    const int barW = useSystemTitleButtons ? qMax(54, leadingInset()) : 54;
    const int titleBarX = useSystemTitleButtons ? 0 : barW;
    const int titleBarW = useSystemTitleButtons ? w : w - barW;

    if (titleBar->height() != titleBarHeight) {
        titleBar->setFixedHeight(titleBarHeight);
    }

    appBar->setFixedWidth(barW);
    appBar->setTopInset(titleInset);
    appBar->setGeometry(0, 0, barW, h);
    stack->setGeometry(barW, 0, w - barW, h);
    titleBar->setGeometry(titleBarX, 0, titleBarW, titleBar->height());
    titleBar->raise();
}
