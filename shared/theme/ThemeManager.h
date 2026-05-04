#pragma once

#include <QColor>
#include <QObject>
#include <QPalette>
#include <QPointer>
#include <QString>

class QApplication;
class QEvent;

enum class ThemeColor {
    Accent,
    AccentHover,
    AccentPressed,
    WindowBackground,
    PageBackground,
    PanelBackground,
    PanelRaisedBackground,
    InputBackground,
    InputFocusBackground,
    PrimaryText,
    SecondaryText,
    TertiaryText,
    PlaceholderText,
    Divider,
    ListHover,
    ListPinned,
    ListSelected,
    ChatInfoPanelOverlay,
    SettingsOverlay,
    ChatInfoMemberListBackground,
    ChatInfoMemberListHover,
    ChatInfoMemberListPrimaryText,
    ChatInfoMemberListSecondaryText,
    ChatInfoMemberListHeaderText,
    DangerText,
    DangerControlHover,
    DangerControlPressed,
    AppBarItemBackground,
    AppBarItemSelectedBackground,
    PostBarItemSelectedBackground,
    ImagePlaceholder,
    ControlHover,
    ControlPressed
};

class ThemeManager final : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        Light = 1,
        Dark = 2,
        FollowSystem = 3
    };

    static ThemeManager& instance();

    Mode configuredMode() const;
    void setMode(Mode mode);
    bool isDark() const;
    QColor color(ThemeColor role) const;

    QPalette applicationPalette() const;
    void applyToApplication(QApplication& application);

signals:
    void themeChanged();

private:
    explicit ThemeManager(QObject* parent = nullptr);

    bool eventFilter(QObject* watched, QEvent* event) override;
    void installSystemThemeListener(QApplication& application);
    void handleSystemColorSchemeChanged();
    void refreshApplicationTheme();
    bool systemIsDark() const;

    QPointer<QApplication> m_application;
    Mode m_configuredMode = Mode::FollowSystem;
    bool m_refreshing = false;
};
