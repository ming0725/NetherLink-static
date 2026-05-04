#pragma once

#include <QWidget>
#include <QStringList>
#include <QVector>

class QKeyEvent;
class QPaintEvent;
class QResizeEvent;
class QStackedWidget;

class MinecraftButton;
class MinecraftSlider;

class SettingsWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);
    void showAnimated();
    void hideAnimated();

signals:
    void closeRequested();

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
        QVector<QWidget*> leftItems;
        QVector<QWidget*> rightItems;
        QWidget* doneWidget = nullptr;
        int firstRowGap = 12;
        int bodyRowGap = 12;
    };

    void buildAllPages();
    void navigateTo(int page);
    void goBack();
    void layoutCurrentPage();

    // Unified layout — all pages use this
    void layoutPageItems(const QVector<QWidget*>& leftItems,
                         const QVector<QWidget*>& rightItems,
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

    // Appearance — real ThemeManager integration
    void applyAppearance();

    QStackedWidget* m_stack = nullptr;
    QString m_currentTitle;
    QVector<PageLayout> m_layouts;

    // Cached pointers
    MinecraftButton* m_appearanceModeToggle = nullptr;
    MinecraftSlider* m_fontSizeSlider = nullptr;

    static inline const QStringList kAppearanceChoices = {
        QStringLiteral("浅色模式"),
        QStringLiteral("深色模式"),
        QStringLiteral("跟随系统")
    };
};
