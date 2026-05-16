#pragma once

#include <QWidget>
#include <QPointer>
#include <QStringList>
#include <QVector>

class QKeyEvent;
class QPaintEvent;
class QResizeEvent;
class QStackedWidget;

class MinecraftButton;
class MinecraftSlider;
class NetherLinkCreditsWindow;
class ThemeColorPalette;

class SettingsWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);
    void showAnimated();
    void hideAnimated();
    void showAppearancePage();

signals:
    void closeRequested();
    void hideAnimationFinished();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum Page {
        PageMain = 0,
        PageNetwork,
        PageAppearance,
        PageSound,
        PageAi,
        PageNotification,
        PageStorage,
        PageKeyBinding,
        PageLogin,
        PageAbout,
        PageCount
    };

    struct PageLayout {
        QVector<QPointer<QWidget>> leftItems;
        QVector<QPointer<QWidget>> rightItems;
        QPointer<QWidget> doneWidget;
        int firstRowGap = 12;
        int bodyRowGap = 12;
    };

    void buildInitialPages();
    void ensurePageCreated(int page);
    void installPage(int page, QWidget* widget);
    void navigateTo(int page);
    void goBack();
    void layoutCurrentPage();
    void openCreditsWindow();

    // Unified layout — all pages use this
    void layoutPageItems(const QVector<QPointer<QWidget>>& leftItems,
                         const QVector<QPointer<QWidget>>& rightItems,
                         QWidget* doneWidget,
                         int firstRowGap,
                         int bodyRowGap);

    // Page builders
    void createMainPage();
    void createNetworkPage();
    void createAppearancePage();
    void createSoundPage();
    void createAiPage();
    void createNotificationPage();
    void createStoragePage();
    void createKeyBindingPage();
    void createLoginPage();
    void createAboutPage();

    // Helpers
    MinecraftButton* createMenuButton(const QString& text, QWidget* parent);
    MinecraftButton* createToggleButton(const QString& label, const QStringList& choices, int index, QWidget* parent);
    int toggleCurrentIndex(const MinecraftButton* btn) const;
    void setToggleIndex(MinecraftButton* btn, int idx);
    void updateFontSizeText(int value);
    void updateThemeColorButtonText();
    void openThemeColorPalette();
    void positionThemeColorPalette();

    // Appearance — real ThemeManager integration
    void resetAppearanceControls();
    void applyAppearance();

    QStackedWidget* m_stack = nullptr;
    QString m_currentTitle;
    QVector<PageLayout> m_layouts;
    QVector<bool> m_pageCreated;

    // Cached pointers
    MinecraftButton* m_appearanceModeToggle = nullptr;
    MinecraftButton* m_inputBarStyleToggle = nullptr;
    MinecraftButton* m_postBarStyleToggle = nullptr;
    MinecraftButton* m_actionMenuStyleToggle = nullptr;
    MinecraftButton* m_themeColorButton = nullptr;
    MinecraftSlider* m_fontSizeSlider = nullptr;
    QPointer<NetherLinkCreditsWindow> m_creditsWindow;
    QPointer<ThemeColorPalette> m_themeColorPalette;

    static inline const QStringList kAppearanceChoices = {
        QStringLiteral("浅色模式"),
        QStringLiteral("深色模式"),
        QStringLiteral("跟随系统")
    };
};
