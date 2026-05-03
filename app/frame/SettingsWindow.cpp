#include "SettingsWindow.h"

#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/MinecraftButton.h"
#include "shared/ui/MinecraftSlider.h"

#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QStringList>

namespace {

constexpr int kSettingsWindowWidth = 880;
constexpr int kSettingsWindowHeight = 600;
constexpr int kSettingsWindowMinWidth = 426;
constexpr int kSettingsWindowMinHeight = 320;
constexpr int kSettingsTitleLeft = 42;
constexpr int kSettingsTitleTop = 38;
constexpr int kSettingsTitleHeight = 34;
constexpr int kSettingsColumnWidth = 220;
constexpr int kSettingsColumnGap = 24;
constexpr int kSettingsButtonHeight = 40;
constexpr int kSettingsDoneWidth = 340;
constexpr int kSettingsFirstRowGap = 48;
constexpr int kSettingsBodyRowGap = 6;
constexpr int kSettingsDoneGap = 20;

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

} // namespace

SettingsWindow::SettingsWindow(QWidget* parent)
    : SystemWindow(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAutoFillBackground(false);
    setWindowFlag(Qt::Window, true);
    setMinimumSize(kSettingsWindowMinWidth, kSettingsWindowMinHeight);
    resize(kSettingsWindowWidth, kSettingsWindowHeight);
    setDragTitleBar(this);
    setFont(settingsFont(font()));

    m_fontSizeSlider = new MinecraftSlider(this);
    m_fontSizeSlider->setRange(0, kFontSizeLabels.size() - 1);
    updateFontSizeText(2);
    m_fontSizeSlider->setValue(2);
    connect(m_fontSizeSlider, &MinecraftSlider::valueChanged,
            this, &SettingsWindow::updateFontSizeText);

    m_networkButton = createMinecraftButton(QStringLiteral("网络设置..."));
    m_appearanceButton = createMinecraftButton(QStringLiteral("外观设置..."));
    m_soundButton = createMinecraftButton(QStringLiteral("音乐与声音..."));
    m_aiButton = createMinecraftButton(QStringLiteral("AI设置..."));
    m_notificationButton = createMinecraftButton(QStringLiteral("消息通知..."));
    m_storageButton = createMinecraftButton(QStringLiteral("存储管理..."));
    m_keyBindingButton = createMinecraftButton(QStringLiteral("按键设置..."));
    m_loginButton = createMinecraftButton(QStringLiteral("登录设置..."));
    m_aboutButton = createMinecraftButton(QStringLiteral("关于 NetherLink"));
    m_doneButton = createMinecraftButton(QStringLiteral("完成"));
    connect(m_doneButton, &QPushButton::clicked, this, &SettingsWindow::close);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, QOverload<>::of(&SettingsWindow::update));

    layoutControls();
}

void SettingsWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    SystemWindow::keyPressEvent(event);
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
    titleFont.setPixelSize(22);
    titleFont.setBold(true);
    painter.setFont(titleFont);

    const QRect titleRect(kSettingsTitleLeft,
                          kSettingsTitleTop,
                          qMax(0, width() - kSettingsTitleLeft * 2),
                          kSettingsTitleHeight);
    painter.setPen(QColor(0, 0, 0, 150));
    painter.drawText(titleRect.translated(1, 1), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("设置"));
    painter.setPen(Qt::white);
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("设置"));
}

void SettingsWindow::resizeEvent(QResizeEvent* event)
{
    SystemWindow::resizeEvent(event);
    layoutControls();
}

MinecraftButton* SettingsWindow::createMinecraftButton(const QString& text)
{
    auto* button = new MinecraftButton(text, this);
    return button;
}

void SettingsWindow::updateFontSizeText(int value)
{
    if (!m_fontSizeSlider) {
        return;
    }

    const int maxLabelIndex = static_cast<int>(kFontSizeLabels.size()) - 1;
    const int labelIndex = qBound(0, value, maxLabelIndex);
    m_fontSizeSlider->setText(QStringLiteral("字号：%1").arg(kFontSizeLabels.at(labelIndex)));
}

void SettingsWindow::layoutControls()
{
    if (!m_fontSizeSlider || !m_doneButton) {
        return;
    }

    const int contentWidth = kSettingsColumnWidth * 2 + kSettingsColumnGap;
    const int rowsHeight = kSettingsButtonHeight
                           + kSettingsFirstRowGap
                           + kSettingsButtonHeight * 4
                           + kSettingsBodyRowGap * 3;
    const int totalHeight = rowsHeight + kSettingsDoneGap + kSettingsButtonHeight;
    const int left = qMax(kSettingsTitleLeft, (width() - contentWidth) / 2);
    const int top = qMax(kSettingsTitleTop + kSettingsTitleHeight + 12,
                         (height() - totalHeight) / 2);
    const int rightColumnLeft = left + kSettingsColumnWidth + kSettingsColumnGap;

    int y = top;
    m_fontSizeSlider->setGeometry(left, y, kSettingsColumnWidth, kSettingsButtonHeight);
    m_networkButton->setGeometry(rightColumnLeft, y, kSettingsColumnWidth, kSettingsButtonHeight);

    y += kSettingsButtonHeight + kSettingsFirstRowGap;
    m_appearanceButton->setGeometry(left, y, kSettingsColumnWidth, kSettingsButtonHeight);
    m_soundButton->setGeometry(rightColumnLeft, y, kSettingsColumnWidth, kSettingsButtonHeight);

    y += kSettingsButtonHeight + kSettingsBodyRowGap;
    m_aiButton->setGeometry(left, y, kSettingsColumnWidth, kSettingsButtonHeight);
    m_notificationButton->setGeometry(rightColumnLeft, y, kSettingsColumnWidth, kSettingsButtonHeight);

    y += kSettingsButtonHeight + kSettingsBodyRowGap;
    m_storageButton->setGeometry(left, y, kSettingsColumnWidth, kSettingsButtonHeight);
    m_keyBindingButton->setGeometry(rightColumnLeft, y, kSettingsColumnWidth, kSettingsButtonHeight);

    y += kSettingsButtonHeight + kSettingsBodyRowGap;
    m_loginButton->setGeometry(left, y, kSettingsColumnWidth, kSettingsButtonHeight);
    m_aboutButton->setGeometry(rightColumnLeft, y, kSettingsColumnWidth, kSettingsButtonHeight);

    y += kSettingsButtonHeight + kSettingsDoneGap;
    m_doneButton->setGeometry(left + (contentWidth - kSettingsDoneWidth) / 2,
                              y,
                              kSettingsDoneWidth,
                              kSettingsButtonHeight);
}
