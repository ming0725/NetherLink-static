#include "MainWindow.h"
#include "WindowEffect.h"
#include "MessageApplication.h"
#include "AiChatApplication.h"
#include "PostApplication.h"
#include "CurrentUser.h"
#include <QScreen>
#include <QGuiApplication>
#include <QPainterPath>
#include <QShowEvent>

MainWindow::MainWindow(QWidget* parent)
    : FramelessWindow(parent)
    , stack(new QStackedWidget(this))
{
    // 窗口基础设置
    resize(950, 650);
    setMinimumHeight(525);
    setMinimumWidth(685);
    setAttribute(Qt::WA_TranslucentBackground);

    CurrentUser::instance().setMainWindow(this);
    appBar = new ApplicationBar(this);
    appBar->setFixedWidth(54);

    titleBar = new QWidget(this);
    titleBar->setFixedHeight(32);
    // 窗口控制按钮
    btnMinimize = new QPushButton(titleBar);
    btnMaximize = new QPushButton(titleBar);
    btnClose    = new QPushButton(titleBar);
    iconClose       = QIcon(":/resources/icon/close.png");
    iconCloseHover  = QIcon(":/resources/icon/hovered_close.png");
    btnMinimize->setIcon(QIcon(":/resources/icon/minimize.png"));
    btnMaximize->setIcon(QIcon(":/resources/icon/maximize.png"));

    btnMinimize->setIconSize(QSize(16, 16));
    btnMaximize->setIconSize(QSize(16, 16));
    btnClose   ->setIconSize(QSize(16, 16));

    btnMinimize->setFixedSize(32, 32);
    btnMaximize->setFixedSize(32, 32);
    btnClose   ->setFixedSize(32, 32);

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

    // 方便换图标 hover 效果依旧装事件过滤器
    btnClose->installEventFilter(this);

    auto hl = new QHBoxLayout(titleBar);
    hl->setContentsMargins(0,0,0,0);
    hl->addStretch();            // 左侧空白
    hl->addWidget(btnMinimize);
    hl->addWidget(btnMaximize);
    hl->addWidget(btnClose);
    hl->setSpacing(0);

    // 3) 告诉 FramelessWindow：这是标题栏
    setTitleBar(titleBar);


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

    // 信号连接
    connect(btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(btnMaximize, &QPushButton::clicked, this, [=](){
        if (isMaximized()) showNormal();
        else showMaximized();
    });
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    QScreen* screen = QGuiApplication::primaryScreen();
    QRect   sg     = screen->geometry();
    int     cx     = (sg.width()  - width())  / 2;
    int     cy     = (sg.height() - height()) / 2;
    move(cx, cy);
}

void MainWindow::showEvent(QShowEvent* event)
{
    FramelessWindow::showEvent(event);

    if (m_windowEffectInitialized) {
        return;
    }

    WindowEffect effect;
    effect.setAcrylicEffect(this, QColor(248, 248, 252, 92));
    m_windowEffectInitialized = true;
}


void MainWindow::resizeEvent(QResizeEvent* event)
{
    FramelessWindow::resizeEvent(event);
    int w = width();
    int h = height();
    // 把左侧应用栏和中央 stack 摆好
    int barW = appBar->width();
    appBar->setGeometry(0, 0, barW, h);
    stack->setGeometry(barW, 0, w-barW, h);
    // 把 titleBar 铺满顶部
    titleBar->setGeometry(barW, 0, w-barW, titleBar->height());
}


void MainWindow::mousePressEvent(QMouseEvent* event)
{
    QWidget* fw = QApplication::focusWidget();
    if (qobject_cast<LineEditComponent*>(fw)
        || qobject_cast<QLineEdit*>(fw)) {
        fw->clearFocus();
    }
    FramelessWindow::mousePressEvent(event);
}


void MainWindow::onBarItemClicked(ApplicationBarItem *item)
{
    int idx = appBar->indexOfTopItem(item);
    if (idx >= 0 && idx < stack->count()) {
        stack->setCurrentIndex(idx);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *ev) {
    if (watched == btnClose) {
        if (ev->type() == QEvent::Enter) {
            btnClose->setIcon(iconCloseHover);
        }
        else if (ev->type() == QEvent::Leave) {
            btnClose->setIcon(iconClose);
        }
    }
    return FramelessWindow::eventFilter(watched, ev);
}
