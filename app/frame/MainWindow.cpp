#include "MainWindow.h"
#include "features/chat/ui/MessageApplication.h"
#include "features/friend/ui/FriendApplication.h"
#include "features/aichat/ui/AiChatApplication.h"
#include "features/post/ui/PostApplication.h"
#include "SettingsWindow.h"
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/FloatingInputBar.h"
#include "shared/theme/ThemeManager.h"
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QApplication>
namespace {

constexpr int kMainWindowMinimumWidth = 820;

IconLineEdit* iconLineEditForWidget(QWidget* widget)
{
    QWidget* current = widget;
    while (current) {
        if (auto* input = qobject_cast<IconLineEdit*>(current)) {
            return input;
        }
        current = current->parentWidget();
    }
    return nullptr;
}

QWidget* focusedInnerLineEdit()
{
    if (auto* lineEdit = qobject_cast<QLineEdit*>(QApplication::focusWidget())) {
        return lineEdit;
    }
    if (auto* textEdit = qobject_cast<QTextEdit*>(QApplication::focusWidget())) {
        return textEdit;
    }
    return nullptr;
}

bool shouldClearLineEditFocus(QWidget* watched, const QPoint& globalPos)
{
    QWidget* focusedLineEdit = focusedInnerLineEdit();
    if (!focusedLineEdit) {
        return false;
    }

    if (!watched) {
        return true;
    }

    auto* focusedInput = iconLineEditForWidget(focusedLineEdit);
    if (focusedInput) {
        const QPoint localPos = focusedInput->mapFromGlobal(globalPos);
        return !focusedInput->rect().contains(localPos);
    }

    if (auto* focusedFloatingBar = qobject_cast<FloatingInputBar*>(focusedLineEdit->parentWidget())) {
        if (watched == focusedFloatingBar || focusedFloatingBar->isAncestorOf(watched) || watched->isAncestorOf(focusedFloatingBar)) {
            const QPoint localPos = focusedFloatingBar->mapFromGlobal(globalPos);
            return !focusedFloatingBar->rect().contains(localPos);
        }
    }

    if (focusedLineEdit == watched || watched->isAncestorOf(focusedLineEdit)) {
        return false;
    }

    return true;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : SystemWindow(parent)
    , stack(new QStackedWidget(this))
    , btnMinimize(nullptr)
    , btnMaximize(nullptr)
    , btnClose(nullptr)
{
    // 窗口基础设置
    setCompactTrafficLightsEnabled(true);
    resize(950, 650);
    setMinimumHeight(525);
    setMinimumWidth(kMainWindowMinimumWidth);
    setAttribute(Qt::WA_TranslucentBackground);

    updateBackdropTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        updateBackdropTheme();
    });

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
            background-color: palette(midlight);
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
    qApp->installEventFilter(this);

    m_messageApp = new MessageApplication(this);
    stack->addWidget(m_messageApp);
    stack->addWidget(createPlaceholderPage());
    stack->addWidget(createPlaceholderPage());
    stack->addWidget(createPlaceholderPage());
    m_defaultPage = new DefaultPage(this);
    stack->addWidget(m_defaultPage);
    stack->setCurrentIndex(0);

    // 绑定点击信号，切换栈页
    connect(appBar, &ApplicationBar::applicationClicked,
            this, &MainWindow::onBarItemClicked);
    connect(appBar, &ApplicationBar::settingsRequested,
            this, &MainWindow::openSettingsWindow);

    QScreen* screen = QGuiApplication::primaryScreen();
    QRect   sg     = screen->geometry();
    int     cx     = (sg.width()  - width())  / 2;
    int     cy     = (sg.height() - height()) / 2;
    move(cx, cy);
}

MainWindow::~MainWindow()
{
    delete m_settingsWindow;
    qApp->removeEventFilter(this);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    delete m_settingsWindow;
    SystemWindow::closeEvent(event);
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

    if (m_settingsWindow) {
        m_settingsWindow->setGeometry(0, 0, event->size().width(), event->size().height());
        m_settingsWindow->raise();
    }
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
    SystemWindow::mousePressEvent(event);
}


void MainWindow::onBarItemClicked(ApplicationBarItem *item)
{
    int idx = appBar->indexOfTopItem(item);
    if (idx >= 0 && idx < stack->count()) {
        ensureApplicationLoaded(idx);
        stack->setCurrentIndex(idx);
    }
}

QWidget* MainWindow::createPlaceholderPage() const
{
    auto* page = new QWidget(stack);
    page->setAutoFillBackground(true);
    QPalette pagePalette = page->palette();
    pagePalette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::PanelBackground));
    page->setPalette(pagePalette);
    return page;
}

void MainWindow::replaceStackPage(int index, QWidget* widget)
{
    if (!widget || index < 0 || index >= stack->count()) {
        return;
    }

    QWidget* existing = stack->widget(index);
    if (existing == widget) {
        return;
    }

    stack->removeWidget(existing);
    stack->insertWidget(index, widget);
    existing->deleteLater();
}

void MainWindow::ensureApplicationLoaded(int index)
{
    switch (index) {
    case 0:
        if (!m_messageApp) {
            m_messageApp = new MessageApplication(this);
            replaceStackPage(index, m_messageApp);
        }
        break;
    case 1:
        if (!m_friendApp) {
            m_friendApp = new FriendApplication(this);
            replaceStackPage(index, m_friendApp);
            connectFriendConversationRequests();
        }
        break;
    case 2:
        if (!m_postApp) {
            m_postApp = new PostApplication(this);
            replaceStackPage(index, m_postApp);
        }
        break;
    case 3:
        if (!m_aiChatApp) {
            m_aiChatApp = new AiChatApplication(this);
            replaceStackPage(index, m_aiChatApp);
        }
        break;
    default:
        break;
    }
}

void MainWindow::connectFriendConversationRequests()
{
    if (!m_friendApp) {
        return;
    }

    connect(m_friendApp, &FriendApplication::requestOpenConversation,
            this, &MainWindow::openConversationFromContacts,
            Qt::UniqueConnection);
}

void MainWindow::updateBackdropTheme()
{
    QColor backdropColor = ThemeManager::instance().color(ThemeColor::WindowBackground);
    backdropColor.setAlpha(92);
    setBackdropColor(backdropColor);
}

void MainWindow::openSettingsWindow()
{
    if (m_settingsWindow) {
        m_settingsWindow->raise();
        m_settingsWindow->setFocus();
        return;
    }

    auto* settings = new SettingsWindow(this);
    connect(settings, &SettingsWindow::closeRequested, this, &MainWindow::closeSettingsWindow);
    m_settingsWindow = settings;
    settings->showAnimated();
}

void MainWindow::closeSettingsWindow()
{
    if (!m_settingsWindow) {
        return;
    }

    SettingsWindow* settings = m_settingsWindow;
    m_settingsWindow = nullptr;
    settings->disconnect(this);
    settings->hideAnimated();
}

void MainWindow::openConversationFromContacts(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    ensureApplicationLoaded(0);
    appBar->setCurrentTopIndex(0);
    stack->setCurrentIndex(0);
    if (m_messageApp) {
        m_messageApp->openConversationFromContact(conversationId);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *ev) {
    if (ev->type() == QEvent::MouseButtonPress || ev->type() == QEvent::MouseButtonRelease) {
        if (auto* watchedWidget = qobject_cast<QWidget*>(watched)) {
            if (watchedWidget->window() != this) {
                return SystemWindow::eventFilter(watched, ev);
            }
            auto* mouseEvent = static_cast<QMouseEvent*>(ev);
            if (ev->type() == QEvent::MouseButtonPress) {
                if (stack->currentWidget() == m_messageApp && m_messageApp) {
                    m_messageApp->handleGlobalMousePress(mouseEvent->globalPosition().toPoint());
                }
                m_pendingFocusClear = nullptr;
                if (shouldClearLineEditFocus(watchedWidget, mouseEvent->globalPosition().toPoint())) {
                    m_pendingFocusClear = QApplication::focusWidget();
                }
            } else if (ev->type() == QEvent::MouseButtonRelease) {
                if (m_pendingFocusClear && QApplication::focusWidget() == m_pendingFocusClear) {
                    m_pendingFocusClear->clearFocus();
                }
                m_pendingFocusClear = nullptr;
            }
        }
    }

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
