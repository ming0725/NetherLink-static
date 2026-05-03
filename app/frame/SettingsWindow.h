#pragma once

#include "platform/SystemWindow.h"

class QKeyEvent;
class QPaintEvent;
class QResizeEvent;
class QString;

class MinecraftButton;
class MinecraftSlider;

class SettingsWindow final : public SystemWindow
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    MinecraftButton* createMinecraftButton(const QString& text);
    void updateFontSizeText(int value);
    void layoutControls();

    MinecraftSlider* m_fontSizeSlider = nullptr;
    MinecraftButton* m_networkButton = nullptr;
    MinecraftButton* m_appearanceButton = nullptr;
    MinecraftButton* m_soundButton = nullptr;
    MinecraftButton* m_aiButton = nullptr;
    MinecraftButton* m_notificationButton = nullptr;
    MinecraftButton* m_storageButton = nullptr;
    MinecraftButton* m_keyBindingButton = nullptr;
    MinecraftButton* m_loginButton = nullptr;
    MinecraftButton* m_aboutButton = nullptr;
    MinecraftButton* m_doneButton = nullptr;
};
