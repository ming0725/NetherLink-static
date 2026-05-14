#include "SettingsWindow.h"

#include "NetherLinkCreditsWindow.h"
#include "shared/services/ImageService.h"
#include "shared/services/AudioService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/MinecraftButton.h"
#include "shared/ui/MinecraftSlider.h"

#ifdef Q_OS_MACOS
#include "features/post/ui/PostApplicationBar.h"
#include "platform/macos/MacFloatingInputBarBridge_p.h"
#include "platform/macos/MacPostBarBridge_p.h"
#include "platform/macos/MacStyledActionMenuBridge_p.h"
#include "shared/ui/FloatingInputBar.h"
#endif

#include <QApplication>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QStackedWidget>

namespace {

// ---- Layout constants (preserved from original) ----
constexpr int kSettingsTitleLeft   = 42;
constexpr int kSettingsTitleTop    = 50;
constexpr int kSettingsTitleHeight = 34;
constexpr int kSettingsColumnWidth = 220;
constexpr int kSettingsColumnGap   = 24;
constexpr int kSettingsButtonHeight = 40;
constexpr int kSettingsDoneWidth   = 340;
constexpr int kSettingsFirstRowGap = 48;
constexpr int kSettingsBodyRowGap  = 6;
constexpr int kSettingsDoneGap     = 20;
constexpr int kSubPageRowGap       = 12;
constexpr int kAnimationDuration   = 400;
constexpr int kTitleFontSize       = 18;

const QString kSettingsBackgroundSource(QStringLiteral(":/resources/icon/options_background.png"));
const QString kSettingsFontSource(QStringLiteral(":/resources/font/MinecraftAE.ttf"));
const QStringList kFontSizeLabels = {
    QStringLiteral("特小"),
    QStringLiteral("小"),
    QStringLiteral("中"),
    QStringLiteral("大"),
    QStringLiteral("特大"),
};

QFont settingsFont(const QFont& fallback)
{
    static const QString family = [] {
        const int fontId = QFontDatabase::addApplicationFont(kSettingsFontSource);
        if (fontId < 0) {
            return QString();
        }
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        return families.isEmpty() ? QString() : families.first();
    }();

    QFont font = fallback;
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    return font;
}

ThemeManager::Mode indexToMode(int index)
{
    return static_cast<ThemeManager::Mode>(index + 1);
}

int modeToIndex(ThemeManager::Mode mode)
{
    return static_cast<int>(mode) - 1;
}

// ---- Choice lists for sub-page toggles ----
QStringList proxyChoices()     { return {QStringLiteral("无"), QStringLiteral("HTTP"), QStringLiteral("SOCKS5")}; }
QStringList timeoutChoices()   { return {QStringLiteral("短"), QStringLiteral("中"), QStringLiteral("长")}; }
QStringList portChoices()      { return {QStringLiteral("默认"), QStringLiteral("自定义")}; }
QStringList transparencyChoices() { return {QStringLiteral("低"), QStringLiteral("中"), QStringLiteral("高")}; }
QStringList volumeChoices()    { return {QStringLiteral("低"), QStringLiteral("中"), QStringLiteral("高")}; }
QStringList modelChoices()     { return {QStringLiteral("默认"), QStringLiteral("GPT"), QStringLiteral("Claude")}; }
QStringList responseLenChoices() { return {QStringLiteral("短"), QStringLiteral("中"), QStringLiteral("长")}; }
QStringList onOffChoices()     { return {QStringLiteral("开启"), QStringLiteral("关闭")}; }
QStringList cacheChoices()     { return {QStringLiteral("手动"), QStringLiteral("自动")}; }
QStringList imgQualityChoices(){ return {QStringLiteral("原始"), QStringLiteral("压缩")}; }
QStringList historyChoices()   { return {QStringLiteral("保留全部"), QStringLiteral("保留30天"), QStringLiteral("保留7天")}; }
QStringList sendKeyChoices()   { return {QStringLiteral("Enter"), QStringLiteral("Ctrl+Enter")}; }
QStringList verifyChoices()    { return {QStringLiteral("关闭"), QStringLiteral("开启")}; }

#ifdef Q_OS_MACOS
QVariantList floatingInputBarModeValues(const QVector<MacFloatingInputBarBridge::Mode>& modes)
{
    QVariantList values;
    values.reserve(modes.size());
    for (MacFloatingInputBarBridge::Mode mode : modes) {
        values.push_back(static_cast<int>(mode));
    }
    return values;
}

QStringList floatingInputBarModeChoices(const QVector<MacFloatingInputBarBridge::Mode>& modes)
{
    QStringList choices;
    choices.reserve(modes.size());
    for (MacFloatingInputBarBridge::Mode mode : modes) {
        switch (mode) {
        case MacFloatingInputBarBridge::Mode::LiquidGlass:
            choices.push_back(QStringLiteral("液态玻璃"));
            break;
        case MacFloatingInputBarBridge::Mode::NativeBlur:
            choices.push_back(QStringLiteral("高斯模糊"));
            break;
        case MacFloatingInputBarBridge::Mode::QtFallback:
            choices.push_back(QStringLiteral("默认"));
            break;
        }
    }
    return choices;
}

QVariantList postBarModeValues(const QVector<MacPostBarBridge::Mode>& modes)
{
    QVariantList values;
    values.reserve(modes.size());
    for (MacPostBarBridge::Mode mode : modes) {
        values.push_back(static_cast<int>(mode));
    }
    return values;
}

QStringList postBarModeChoices(const QVector<MacPostBarBridge::Mode>& modes)
{
    QStringList choices;
    choices.reserve(modes.size());
    for (MacPostBarBridge::Mode mode : modes) {
        switch (mode) {
        case MacPostBarBridge::Mode::LiquidGlass:
            choices.push_back(QStringLiteral("液态玻璃"));
            break;
        case MacPostBarBridge::Mode::NativeBlur:
            choices.push_back(QStringLiteral("高斯模糊"));
            break;
        case MacPostBarBridge::Mode::QtFallback:
            choices.push_back(QStringLiteral("默认"));
            break;
        }
    }
    return choices;
}

QVariantList styledActionMenuModeValues(const QVector<MacStyledActionMenuBridge::Mode>& modes)
{
    QVariantList values;
    values.reserve(modes.size());
    for (MacStyledActionMenuBridge::Mode mode : modes) {
        values.push_back(static_cast<int>(mode));
    }
    return values;
}

QStringList styledActionMenuModeChoices(const QVector<MacStyledActionMenuBridge::Mode>& modes)
{
    QStringList choices;
    choices.reserve(modes.size());
    for (MacStyledActionMenuBridge::Mode mode : modes) {
        switch (mode) {
        case MacStyledActionMenuBridge::Mode::NativeMenu:
            choices.push_back(QStringLiteral("macOS"));
            break;
        case MacStyledActionMenuBridge::Mode::QtFallback:
            choices.push_back(QStringLiteral("默认"));
            break;
        }
    }
    return choices;
}

int indexForModeValue(const QVariantList& values, int modeValue)
{
    for (int i = 0; i < values.size(); ++i) {
        if (values.at(i).toInt() == modeValue) {
            return i;
        }
    }
    return values.isEmpty() ? -1 : 0;
}

int modeValueForToggle(const MinecraftButton* button)
{
    if (!button) {
        return 0;
    }

    const QVariantList values = button->property("modeValues").toList();
    const int index = button->property("toggleIndex").toInt();
    if (index < 0 || index >= values.size()) {
        return 0;
    }
    return values.at(index).toInt();
}
#endif

} // namespace

// ============================================================
// Constructor
// ============================================================

SettingsWindow::SettingsWindow(QWidget* parent)
    : QWidget(parent)
    , m_stack(new QStackedWidget(this))
    , m_layouts(PageCount)
    , m_pageCreated(PageCount, false)
{
    setAutoFillBackground(false);
    setFont(settingsFont(font()));

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, QOverload<>::of(&SettingsWindow::update));

    buildInitialPages();
    m_currentTitle = QStringLiteral("设置");
    m_stack->setCurrentIndex(PageMain);
}

// ============================================================
// Animation
// ============================================================

void SettingsWindow::showAnimated()
{
    QWidget* p = parentWidget();
    if (!p) { show(); return; }

    const int w = p->width();
    const int h = p->height();
    setGeometry(0, h, w, h);
    show();
    raise();
    setFocus();

    auto* anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(kAnimationDuration);
    anim->setStartValue(QPoint(0, h));
    anim->setEndValue(QPoint(0, 0));
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start();
}

void SettingsWindow::hideAnimated()
{
    QWidget* p = parentWidget();
    if (!p) {
        emit hideAnimationFinished();
        deleteLater();
        return;
    }

    const int h = p->height();
    auto* anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(kAnimationDuration);
    anim->setStartValue(pos());
    anim->setEndValue(QPoint(0, h));
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        emit hideAnimationFinished();
        deleteLater();
    });
    anim->start();
}

// ============================================================
// Events
// ============================================================

void SettingsWindow::keyPressEvent(QKeyEvent* event)
{
    if (m_creditsWindow && m_creditsWindow->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Escape:
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            m_creditsWindow->close();
            return;
        default:
            QWidget::keyPressEvent(event);
            return;
        }
    }

    if (event->key() == Qt::Key_Escape) {
        if (m_stack->currentIndex() != PageMain) {
            goBack();
        } else {
            emit closeRequested();
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

void SettingsWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QPixmap background = ImageService::instance().pixmap(kSettingsBackgroundSource);
    if (background.isNull()) {
        painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::SettingsFallbackBackground));
    } else {
        painter.drawTiledPixmap(rect(), background);
    }
    painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::SettingsOverlay));

    QFont titleFont = font();
    titleFont.setPixelSize(kTitleFontSize);
    titleFont.setBold(true);
    painter.setFont(titleFont);

    const QRect titleRect(kSettingsTitleLeft,
                          kSettingsTitleTop,
                          qMax(0, width() - kSettingsTitleLeft * 2),
                          kSettingsTitleHeight);
    painter.setPen(ThemeManager::instance().color(ThemeColor::OverlayStroke));
    painter.drawText(titleRect.translated(1, 1), Qt::AlignCenter, m_currentTitle);
    painter.setPen(ThemeManager::instance().color(ThemeColor::TextOnAccent));
    painter.drawText(titleRect, Qt::AlignCenter, m_currentTitle);
}

void SettingsWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_stack->setGeometry(0, 0, event->size().width(), event->size().height());
    if (m_creditsWindow) {
        m_creditsWindow->setGeometry(rect());
        m_creditsWindow->raise();
    }
    layoutCurrentPage();
}

// ============================================================
// Navigation
// ============================================================

void SettingsWindow::navigateTo(int page)
{
    ensurePageCreated(page);

    if (page == PageAppearance) {
        resetAppearanceControls();
    }
    m_stack->setCurrentIndex(page);
    layoutCurrentPage();
}

void SettingsWindow::goBack()
{
    m_currentTitle = QStringLiteral("设置");
    m_stack->setCurrentIndex(PageMain);
    layoutCurrentPage();
    update();
}

// ============================================================
// Unified layout (shared by all pages)
// ============================================================

void SettingsWindow::layoutCurrentPage()
{
    const int idx = m_stack->currentIndex();
    if (idx < 0 || idx >= m_layouts.size()) return;

    const PageLayout& pl = m_layouts[idx];
    layoutPageItems(pl.leftItems, pl.rightItems, pl.doneWidget,
                    pl.firstRowGap, pl.bodyRowGap);
}

void SettingsWindow::openCreditsWindow()
{
    if (m_creditsWindow) {
        m_creditsWindow->setGeometry(rect());
        m_creditsWindow->show();
        m_creditsWindow->raise();
        m_creditsWindow->setFocus();
        return;
    }

    auto* credits = new NetherLinkCreditsWindow(this);
    credits->setAttribute(Qt::WA_DeleteOnClose);
    credits->setGeometry(rect());
    m_creditsWindow = credits;
    connect(credits, &QObject::destroyed, this, [this]() {
        m_creditsWindow = nullptr;
    });
    credits->show();
    credits->raise();
    credits->setFocus();
}

void SettingsWindow::layoutPageItems(const QVector<QPointer<QWidget>>& leftItems,
                                     const QVector<QPointer<QWidget>>& rightItems,
                                     QWidget* doneWidget,
                                     int firstRowGap,
                                     int bodyRowGap)
{
    const int rowCount = qMax(leftItems.size(), rightItems.size());
    if (rowCount == 0 && !doneWidget) return;

    const int contentWidth = kSettingsColumnWidth * 2 + kSettingsColumnGap;
    const int left = qMax(kSettingsTitleLeft, (width() - contentWidth) / 2);
    const int top  = kSettingsTitleTop + kSettingsTitleHeight + 32;
    const int rightCol = left + kSettingsColumnWidth + kSettingsColumnGap;

    int y = top;
    for (int i = 0; i < rowCount; ++i) {
        if (i < leftItems.size() && leftItems[i]) {
            leftItems[i]->setGeometry(left, y, kSettingsColumnWidth, kSettingsButtonHeight);
            leftItems[i]->setVisible(true);
        }
        if (i < rightItems.size() && rightItems[i]) {
            rightItems[i]->setGeometry(rightCol, y, kSettingsColumnWidth, kSettingsButtonHeight);
            rightItems[i]->setVisible(true);
        }
        y += kSettingsButtonHeight;
        if (rowCount >= 2) {
            y += (i == 0) ? firstRowGap : bodyRowGap;
        }
    }

    y += kSettingsDoneGap;
    if (doneWidget) {
        doneWidget->setGeometry(left + (contentWidth - kSettingsDoneWidth) / 2,
                                y, kSettingsDoneWidth, kSettingsButtonHeight);
        doneWidget->setVisible(true);
    }
}

// ============================================================
// Helpers
// ============================================================

MinecraftButton* SettingsWindow::createMenuButton(const QString& text, QWidget* parent)
{
    auto* btn = new MinecraftButton(text, parent);
    return btn;
}

MinecraftButton* SettingsWindow::createToggleButton(const QString& label, const QStringList& choices, int index, QWidget* parent)
{
    const QString fullText = (index >= 0 && index < choices.size())
        ? QStringLiteral("%1：%2").arg(label, choices.at(index))
        : label;
    auto* btn = new MinecraftButton(fullText, parent);
    btn->setProperty("toggleChoices", choices);
    btn->setProperty("toggleLabel", label);
    btn->setProperty("toggleIndex", index);

    connect(btn, &QPushButton::clicked, this, [this, btn]() {
        const QStringList c = btn->property("toggleChoices").toStringList();
        int i = btn->property("toggleIndex").toInt();
        i = (i + 1) % c.size();
        setToggleIndex(btn, i);
    });

    return btn;
}

int SettingsWindow::toggleCurrentIndex(const MinecraftButton* btn) const
{
    return btn->property("toggleIndex").toInt();
}

void SettingsWindow::setToggleIndex(MinecraftButton* btn, int idx)
{
    const QStringList choices = btn->property("toggleChoices").toStringList();
    const QString label = btn->property("toggleLabel").toString();
    if (idx >= 0 && idx < choices.size()) {
        btn->setText(QStringLiteral("%1：%2").arg(label, choices.at(idx)));
        btn->setProperty("toggleIndex", idx);
    }
}

void SettingsWindow::updateFontSizeText(int value)
{
    if (!m_fontSizeSlider) return;
    const int idx = qBound(0, value, static_cast<int>(kFontSizeLabels.size()) - 1);
    m_fontSizeSlider->setText(QStringLiteral("字号：%1").arg(kFontSizeLabels.at(idx)));
}

// ============================================================
// Build pages
// ============================================================

void SettingsWindow::buildInitialPages()
{
    for (int page = 0; page < PageCount; ++page) {
        auto* placeholder = new QWidget;
        placeholder->setAutoFillBackground(false);
        m_stack->addWidget(placeholder);
    }

    ensurePageCreated(PageMain);
}

void SettingsWindow::ensurePageCreated(int page)
{
    if (page < 0 || page >= PageCount || m_pageCreated.at(page)) {
        return;
    }

    switch (page) {
    case PageMain:
        createMainPage();
        break;
    case PageNetwork:
        createNetworkPage();
        break;
    case PageAppearance:
        createAppearancePage();
        break;
    case PageSound:
        createSoundPage();
        break;
    case PageAi:
        createAiPage();
        break;
    case PageNotification:
        createNotificationPage();
        break;
    case PageStorage:
        createStoragePage();
        break;
    case PageKeyBinding:
        createKeyBindingPage();
        break;
    case PageLogin:
        createLoginPage();
        break;
    case PageAbout:
        createAboutPage();
        break;
    default:
        return;
    }
}

void SettingsWindow::installPage(int page, QWidget* widget)
{
    if (page < 0 || page >= PageCount || !widget) {
        delete widget;
        return;
    }

    QWidget* oldWidget = m_stack->widget(page);
    if (oldWidget) {
        m_stack->removeWidget(oldWidget);
        oldWidget->deleteLater();
    }

    m_stack->insertWidget(page, widget);
    m_pageCreated[page] = true;
}

// ============================================================
// Main page — original layout preserved
// ============================================================

void SettingsWindow::createMainPage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    // Font size slider
    m_fontSizeSlider = new MinecraftSlider(page);
    m_fontSizeSlider->setRange(0, kFontSizeLabels.size() - 1);
    m_fontSizeSlider->setValue(2);
    updateFontSizeText(2);
    connect(m_fontSizeSlider, &MinecraftSlider::valueChanged,
            this, &SettingsWindow::updateFontSizeText);

    // Left column
    auto* appearanceBtn = createMenuButton(QStringLiteral("外观设置..."), page);
    auto* aiBtn       = createMenuButton(QStringLiteral("AI设置..."), page);
    auto* storageBtn  = createMenuButton(QStringLiteral("存储管理..."), page);
    auto* loginBtn    = createMenuButton(QStringLiteral("登录设置..."), page);

    // Right column
    auto* networkBtn     = createMenuButton(QStringLiteral("网络设置..."), page);
    auto* soundBtn       = createMenuButton(QStringLiteral("音乐与声音..."), page);
    auto* notifBtn       = createMenuButton(QStringLiteral("消息通知..."), page);
    auto* keyBindingBtn  = createMenuButton(QStringLiteral("按键设置..."), page);
    auto* aboutBtn       = createMenuButton(QStringLiteral("关于 NetherLink"), page);

    // Done button
    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::closeRequested);

    // Navigation connections
    connect(appearanceBtn, &QPushButton::clicked, this, [this]() {
        m_currentTitle = QStringLiteral("外观设置");
        update();
        navigateTo(PageAppearance);
    });
    connect(aiBtn, &QPushButton::clicked, this, [this]() {
        m_currentTitle = QStringLiteral("AI设置");
        update();
        navigateTo(PageAi);
    });
    connect(storageBtn, &QPushButton::clicked, this, [this]() {
        m_currentTitle = QStringLiteral("存储管理");
        update();
        navigateTo(PageStorage);
    });
    connect(loginBtn, &QPushButton::clicked, this, [this]() {
        m_currentTitle = QStringLiteral("登录设置");
        update();
        navigateTo(PageLogin);
    });
    connect(networkBtn, &QPushButton::clicked, this, [this]() {
        m_currentTitle = QStringLiteral("网络设置");
        update();
        navigateTo(PageNetwork);
    });
    connect(soundBtn, &QPushButton::clicked, this, [this]() {
        m_currentTitle = QStringLiteral("音乐与声音");
        update();
        navigateTo(PageSound);
    });
    connect(notifBtn, &QPushButton::clicked, this, [this]() {
        m_currentTitle = QStringLiteral("消息通知");
        update();
        navigateTo(PageNotification);
    });
    connect(keyBindingBtn, &QPushButton::clicked, this, [this]() {
        m_currentTitle = QStringLiteral("按键设置");
        update();
        navigateTo(PageKeyBinding);
    });
    connect(aboutBtn, &QPushButton::clicked, this, [this]() {
        m_currentTitle = QStringLiteral("关于 NetherLink");
        update();
        navigateTo(PageAbout);
    });

    // Register layout — exact original positions
    PageLayout pl;
    pl.leftItems  = {m_fontSizeSlider, appearanceBtn, aiBtn, storageBtn, loginBtn};
    pl.rightItems = {networkBtn, soundBtn, notifBtn, keyBindingBtn, aboutBtn};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSettingsFirstRowGap;
    pl.bodyRowGap  = kSettingsBodyRowGap;
    m_layouts[PageMain] = pl;

    installPage(PageMain, page);
}

// ============================================================
// Sub-pages — shared structure, same setGeometry layout
// ============================================================

void SettingsWindow::createNetworkPage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    auto* proxyToggle  = createToggleButton(QStringLiteral("代理类型"), proxyChoices(), 0, page);
    auto* portToggle   = createToggleButton(QStringLiteral("端口设置"), portChoices(), 0, page);
    auto* timeoutToggle = createToggleButton(QStringLiteral("连接超时"), timeoutChoices(), 1, page);

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {proxyToggle, timeoutToggle};
    pl.rightItems = {portToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageNetwork] = pl;

    installPage(PageNetwork, page);
}

void SettingsWindow::createAppearancePage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    const int curModeIdx = modeToIndex(ThemeManager::instance().configuredMode());
    m_appearanceModeToggle = createToggleButton(QStringLiteral("外观模式"), kAppearanceChoices, curModeIdx, page);

    auto* transpToggle = createToggleButton(QStringLiteral("窗口透明度"), transparencyChoices(), 1, page);

#ifdef Q_OS_MACOS
    const QVector<MacFloatingInputBarBridge::Mode> inputBarModes =
            MacFloatingInputBarBridge::supportedModes();
    if (inputBarModes.size() >= 2) {
        const QVariantList values = floatingInputBarModeValues(inputBarModes);
        m_inputBarStyleToggle = createToggleButton(
                QStringLiteral("聊天输入栏样式"),
                floatingInputBarModeChoices(inputBarModes),
                indexForModeValue(values, static_cast<int>(MacFloatingInputBarBridge::mode())),
                page);
        m_inputBarStyleToggle->setProperty("modeValues", values);
    }

    const QVector<MacPostBarBridge::Mode> postBarModes = MacPostBarBridge::supportedModes();
    if (postBarModes.size() >= 2) {
        const QVariantList values = postBarModeValues(postBarModes);
        m_postBarStyleToggle = createToggleButton(
                QStringLiteral("帖子导航栏"),
                postBarModeChoices(postBarModes),
                indexForModeValue(values, static_cast<int>(MacPostBarBridge::mode())),
                page);
        m_postBarStyleToggle->setProperty("modeValues", values);
    }

    const QVector<MacStyledActionMenuBridge::Mode> actionMenuModes =
            MacStyledActionMenuBridge::supportedModes();
    if (actionMenuModes.size() >= 2) {
        const QVariantList values = styledActionMenuModeValues(actionMenuModes);
        m_actionMenuStyleToggle = createToggleButton(
                QStringLiteral("右键菜单"),
                styledActionMenuModeChoices(actionMenuModes),
                indexForModeValue(values, static_cast<int>(MacStyledActionMenuBridge::mode())),
                page);
        m_actionMenuStyleToggle->setProperty("modeValues", values);
    }
#endif

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, [this]() {
        applyAppearance();
        goBack();
    });

    PageLayout pl;
    pl.leftItems = {m_appearanceModeToggle};
    pl.rightItems = {transpToggle};
#ifdef Q_OS_MACOS
    if (m_inputBarStyleToggle) {
        pl.leftItems.push_back(m_inputBarStyleToggle);
    }
    if (m_actionMenuStyleToggle) {
        pl.leftItems.push_back(m_actionMenuStyleToggle);
    }
    if (m_postBarStyleToggle) {
        pl.rightItems.push_back(m_postBarStyleToggle);
    }
#endif
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageAppearance] = pl;

    installPage(PageAppearance, page);
}

void SettingsWindow::createSoundPage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    auto* soundToggle    = createToggleButton(QStringLiteral("消息提示音"), onOffChoices(), 0, page);
    auto* sysNotifToggle = createToggleButton(QStringLiteral("系统通知"), onOffChoices(), 0, page);

    auto makeSlider = [&](const QString& label, int initial) {
        auto* slider = new MinecraftSlider(page);
        slider->setRange(0, 100);
        slider->setValue(initial);
        slider->setText(QStringLiteral("%1：%2%").arg(label).arg(initial));
        connect(slider, &MinecraftSlider::valueChanged, this, [slider, label](int value) {
            slider->setText(QStringLiteral("%1：%2%").arg(label).arg(value));
        });
        return slider;
    };

    auto* masterSlider = makeSlider(QStringLiteral("主音量"), AudioService::instance().masterVolume());
    auto* sfxSlider    = makeSlider(QStringLiteral("音效"), AudioService::instance().soundEffectVolume());
    auto* voiceSlider  = makeSlider(QStringLiteral("语音音量"), 100);

    connect(masterSlider, &MinecraftSlider::valueChanged, this, [](int value) {
        AudioService::instance().setMasterVolume(value);
    });
    connect(sfxSlider, &MinecraftSlider::valueChanged, this, [](int value) {
        AudioService::instance().setSoundEffectVolume(value);
    });

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {soundToggle, masterSlider, sfxSlider, voiceSlider};
    pl.rightItems = {sysNotifToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageSound] = pl;

    installPage(PageSound, page);
}

void SettingsWindow::createAiPage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    auto* modelToggle    = createToggleButton(QStringLiteral("AI模型"), modelChoices(), 0, page);
    auto* lenToggle     = createToggleButton(QStringLiteral("响应长度"), responseLenChoices(), 1, page);
    auto* contextToggle = createToggleButton(QStringLiteral("上下文记忆"), onOffChoices(), 0, page);

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {modelToggle, contextToggle};
    pl.rightItems = {lenToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageAi] = pl;

    installPage(PageAi, page);
}

void SettingsWindow::createNotificationPage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    auto* desktopToggle = createToggleButton(QStringLiteral("桌面通知"), onOffChoices(), 0, page);
    auto* previewToggle = createToggleButton(QStringLiteral("通知预览"), onOffChoices(), 0, page);
    auto* dndToggle     = createToggleButton(QStringLiteral("免打扰模式"), onOffChoices(), 1, page);

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {desktopToggle, dndToggle};
    pl.rightItems = {previewToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageNotification] = pl;

    installPage(PageNotification, page);
}

void SettingsWindow::createStoragePage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    auto* cacheToggle   = createToggleButton(QStringLiteral("缓存清理"), cacheChoices(), 0, page);
    auto* qualityToggle = createToggleButton(QStringLiteral("图片质量"), imgQualityChoices(), 0, page);
    auto* historyToggle = createToggleButton(QStringLiteral("历史记录"), historyChoices(), 0, page);

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {cacheToggle, historyToggle};
    pl.rightItems = {qualityToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageStorage] = pl;

    installPage(PageStorage, page);
}

void SettingsWindow::createKeyBindingPage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    auto* sendKeyToggle  = createToggleButton(QStringLiteral("发送消息"), sendKeyChoices(), 0, page);
    auto* quickReplyToggle = createToggleButton(QStringLiteral("快捷回复"), onOffChoices(), 0, page);
    auto* globalKeyToggle  = createToggleButton(QStringLiteral("全局快捷键"), onOffChoices(), 1, page);

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {sendKeyToggle, globalKeyToggle};
    pl.rightItems = {quickReplyToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageKeyBinding] = pl;

    installPage(PageKeyBinding, page);
}

void SettingsWindow::createLoginPage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    auto* autoLoginToggle  = createToggleButton(QStringLiteral("自动登录"), onOffChoices(), 0, page);
    auto* rememberPwdToggle = createToggleButton(QStringLiteral("记住密码"), onOffChoices(), 0, page);
    auto* verifyToggle      = createToggleButton(QStringLiteral("登录验证"), verifyChoices(), 1, page);

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {autoLoginToggle, verifyToggle};
    pl.rightItems = {rememberPwdToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageLogin] = pl;

    installPage(PageLogin, page);
}

void SettingsWindow::createAboutPage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    // Version display — disabled button with dedicated disabled texture
    auto* versionBtn = createMenuButton(QStringLiteral("版本：v1.0.0"), page);
    versionBtn->setDisabledImage(QStringLiteral(":/resources/icon/mc_button_disabled.png"));
    versionBtn->setEnabled(false);

    auto* updateToggle  = createToggleButton(QStringLiteral("检查更新"), onOffChoices(), 0, page);
    auto* licenseToggle = createToggleButton(QStringLiteral("开源协议"), QStringList{QStringLiteral("MIT")}, 0, page);
    auto* creditsBtn = createMenuButton(QStringLiteral("鸣谢名单"), page);
    connect(creditsBtn, &QPushButton::clicked, this, &SettingsWindow::openCreditsWindow);

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {versionBtn, licenseToggle};
    pl.rightItems = {updateToggle, creditsBtn};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageAbout] = pl;

    installPage(PageAbout, page);
}

// ============================================================
// Appearance — real ThemeManager integration
// ============================================================

void SettingsWindow::resetAppearanceControls()
{
    const int idx = modeToIndex(ThemeManager::instance().configuredMode());
    setToggleIndex(m_appearanceModeToggle, idx);

#ifdef Q_OS_MACOS
    if (m_inputBarStyleToggle) {
        setToggleIndex(
                m_inputBarStyleToggle,
                indexForModeValue(m_inputBarStyleToggle->property("modeValues").toList(),
                                  static_cast<int>(MacFloatingInputBarBridge::mode())));
    }
    if (m_postBarStyleToggle) {
        setToggleIndex(
                m_postBarStyleToggle,
                indexForModeValue(m_postBarStyleToggle->property("modeValues").toList(),
                                  static_cast<int>(MacPostBarBridge::mode())));
    }
    if (m_actionMenuStyleToggle) {
        setToggleIndex(
                m_actionMenuStyleToggle,
                indexForModeValue(m_actionMenuStyleToggle->property("modeValues").toList(),
                                  static_cast<int>(MacStyledActionMenuBridge::mode())));
    }
#endif
}

void SettingsWindow::applyAppearance()
{
    if (!m_appearanceModeToggle) return;

    const int idx = toggleCurrentIndex(m_appearanceModeToggle);
    ThemeManager::instance().setMode(indexToMode(idx));

#ifdef Q_OS_MACOS
    if (m_inputBarStyleToggle) {
        MacFloatingInputBarBridge::setMode(static_cast<MacFloatingInputBarBridge::Mode>(
                modeValueForToggle(m_inputBarStyleToggle)));
    }
    if (m_postBarStyleToggle) {
        MacPostBarBridge::setMode(static_cast<MacPostBarBridge::Mode>(
                modeValueForToggle(m_postBarStyleToggle)));
    }
    if (m_actionMenuStyleToggle) {
        MacStyledActionMenuBridge::setMode(static_cast<MacStyledActionMenuBridge::Mode>(
                modeValueForToggle(m_actionMenuStyleToggle)));
    }

    const QWidgetList widgets = QApplication::allWidgets();
    for (QWidget* widget : widgets) {
        if (auto* inputBar = qobject_cast<FloatingInputBar*>(widget)) {
            inputBar->refreshPlatformAppearance();
        } else if (auto* postBar = qobject_cast<PostApplicationBar*>(widget)) {
            postBar->refreshPlatformAppearance();
        }
    }
#endif
}
