#include "SettingsWindow.h"

#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/MinecraftButton.h"
#include "shared/ui/MinecraftSlider.h"

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
const QColor kSettingsFallbackBackground(0xB0, 0xB0, 0xB0);
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

} // namespace

// ============================================================
// Constructor
// ============================================================

SettingsWindow::SettingsWindow(QWidget* parent)
    : QWidget(parent)
    , m_stack(new QStackedWidget(this))
    , m_layouts(PageCount)
{
    setAutoFillBackground(false);
    setFont(settingsFont(font()));

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, QOverload<>::of(&SettingsWindow::update));

    buildAllPages();
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
    if (!p) { deleteLater(); return; }

    const int h = p->height();
    auto* anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(kAnimationDuration);
    anim->setStartValue(pos());
    anim->setEndValue(QPoint(0, h));
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, this, &QWidget::deleteLater);
    anim->start();
}

// ============================================================
// Events
// ============================================================

void SettingsWindow::keyPressEvent(QKeyEvent* event)
{
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
    const QPixmap background = ImageService::instance().pixmap(kSettingsBackgroundSource);
    if (background.isNull()) {
        painter.fillRect(rect(), kSettingsFallbackBackground);
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
    painter.setPen(QColor(0, 0, 0, 150));
    painter.drawText(titleRect.translated(1, 1), Qt::AlignCenter, m_currentTitle);
    painter.setPen(Qt::white);
    painter.drawText(titleRect, Qt::AlignCenter, m_currentTitle);
}

void SettingsWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_stack->setGeometry(0, 0, event->size().width(), event->size().height());
    layoutCurrentPage();
}

// ============================================================
// Navigation
// ============================================================

void SettingsWindow::navigateTo(int page)
{
    if (page == PageAppearance) {
        const int idx = modeToIndex(ThemeManager::instance().configuredMode());
        setToggleIndex(m_appearanceModeToggle, idx);
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

void SettingsWindow::layoutPageItems(const QVector<QWidget*>& leftItems,
                                     const QVector<QWidget*>& rightItems,
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
// Build all pages
// ============================================================

void SettingsWindow::buildAllPages()
{
    createMainPage();
    createNetworkPage();
    createAppearancePage();
    createSoundPage();
    createAiPage();
    createNotificationPage();
    createStoragePage();
    createKeyBindingPage();
    createLoginPage();
    createAboutPage();
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

    m_stack->addWidget(page);
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

    m_stack->addWidget(page);
}

void SettingsWindow::createAppearancePage()
{
    auto* page = new QWidget;
    page->setAutoFillBackground(false);

    const int curModeIdx = modeToIndex(ThemeManager::instance().configuredMode());
    m_appearanceModeToggle = createToggleButton(QStringLiteral("外观模式"), kAppearanceChoices, curModeIdx, page);

    auto* transpToggle = createToggleButton(QStringLiteral("窗口透明度"), transparencyChoices(), 1, page);

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, [this]() {
        applyAppearance();
        goBack();
    });

    PageLayout pl;
    pl.leftItems  = {m_appearanceModeToggle};
    pl.rightItems = {transpToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageAppearance] = pl;

    m_stack->addWidget(page);
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

    auto* masterSlider = makeSlider(QStringLiteral("主音量"), 80);
    auto* sfxSlider    = makeSlider(QStringLiteral("音效"), 70);
    auto* voiceSlider  = makeSlider(QStringLiteral("语音音量"), 90);

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {soundToggle, masterSlider, sfxSlider, voiceSlider};
    pl.rightItems = {sysNotifToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageSound] = pl;

    m_stack->addWidget(page);
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

    m_stack->addWidget(page);
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

    m_stack->addWidget(page);
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

    m_stack->addWidget(page);
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

    m_stack->addWidget(page);
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

    m_stack->addWidget(page);
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

    auto* doneBtn = createMenuButton(QStringLiteral("完成"), page);
    connect(doneBtn, &QPushButton::clicked, this, &SettingsWindow::goBack);

    PageLayout pl;
    pl.leftItems  = {versionBtn, licenseToggle};
    pl.rightItems = {updateToggle};
    pl.doneWidget = doneBtn;
    pl.firstRowGap = kSubPageRowGap;
    pl.bodyRowGap  = kSubPageRowGap;
    m_layouts[PageAbout] = pl;

    m_stack->addWidget(page);
}

// ============================================================
// Appearance — real ThemeManager integration
// ============================================================

void SettingsWindow::applyAppearance()
{
    if (!m_appearanceModeToggle) return;

    const int idx = toggleCurrentIndex(m_appearanceModeToggle);
    ThemeManager::instance().setMode(indexToMode(idx));
}
