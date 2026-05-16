#include "ThemeManager.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QEvent>
#include <QGuiApplication>
#include <QScopedValueRollback>
#include <QStyle>
#include <QStyleHints>
#include <QWidget>
#include <QtMath>
#include <QtGlobal>

namespace {

qreal linearizedSrgb(int value)
{
    const qreal channel = qBound(0, value, 255) / 255.0;
    return channel <= 0.03928
            ? channel / 12.92
            : qPow((channel + 0.055) / 1.055, 2.4);
}

qreal relativeLuminance(const QColor& color)
{
    const QColor rgb = color.toRgb();
    return 0.2126 * linearizedSrgb(rgb.red())
            + 0.7152 * linearizedSrgb(rgb.green())
            + 0.0722 * linearizedSrgb(rgb.blue());
}

QColor selectionColorOn(const QColor& background)
{
    const QColor textColor = ThemeManager::textColorOn(background);
    return textColor == QColor(Qt::black)
            ? QColor(0, 0, 0, 52)
            : QColor(255, 255, 255, 88);
}

} // namespace

ThemeManager& ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
}

ThemeManager::Mode ThemeManager::configuredMode() const
{
    return m_configuredMode;
}

void ThemeManager::setMode(Mode mode)
{
    if (m_configuredMode == mode) {
        return;
    }
    m_configuredMode = mode;
    refreshApplicationTheme();
}

bool ThemeManager::isDark() const
{
    const Mode mode = configuredMode();
    if (mode == Mode::Light) {
        return false;
    }
    if (mode == Mode::Dark) {
        return true;
    }
    return systemIsDark();
}

QColor ThemeManager::themeColor() const
{
    return m_themeColor;
}

void ThemeManager::setThemeColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }

    QColor rgb = color.toRgb();
    rgb.setAlpha(255);
    if (m_themeColor == rgb) {
        return;
    }

    m_themeColor = rgb;
    refreshApplicationTheme();
}

bool ThemeManager::qtFallbackLiquidGlassEnabled() const
{
    return m_qtFallbackLiquidGlassEnabled;
}

void ThemeManager::setQtFallbackLiquidGlassEnabled(bool enabled)
{
    if (m_qtFallbackLiquidGlassEnabled == enabled) {
        return;
    }

    m_qtFallbackLiquidGlassEnabled = enabled;
    refreshApplicationTheme();
}

QColor ThemeManager::textColorOn(const QColor& background, int alpha)
{
    QColor textColor = relativeLuminance(background) > 0.46
            ? QColor(Qt::black)
            : QColor(Qt::white);
    textColor.setAlpha(qBound(0, alpha, 255));
    return textColor;
}

bool ThemeManager::isAccentColorRole(ThemeColor role)
{
    switch (role) {
    case ThemeColor::Accent:
    case ThemeColor::AccentHover:
    case ThemeColor::AccentPressed:
    case ThemeColor::AccentBubble:
    case ThemeColor::AccentLinkText:
    case ThemeColor::AccentLinkTextOnAccent:
    case ThemeColor::AccentTextSelection:
    case ThemeColor::AccentTextSelectionOnAccent:
    case ThemeColor::TextOnAccent:
    case ThemeColor::SecondaryTextOnAccent:
    case ThemeColor::ListSelected:
        return true;
    default:
        return false;
    }
}

QColor ThemeManager::accentColor(ThemeColor role, bool dark) const
{
    const QColor accent = m_themeColor.isValid() ? m_themeColor : QColor(0x00, 0x99, 0xff);
    const QColor accentBackground = dark ? accent.darker(112) : accent;
    if (!dark) {
        switch (role) {
        case ThemeColor::Accent:
        case ThemeColor::ListSelected:
            return accent;
        case ThemeColor::AccentHover:
            return accent.darker(108);
        case ThemeColor::AccentPressed:
            return accent.darker(120);
        case ThemeColor::AccentBubble:
            return accent;
        case ThemeColor::AccentLinkText:
            return accent.darker(135);
        case ThemeColor::AccentLinkTextOnAccent:
            return textColorOn(accentBackground);
        case ThemeColor::AccentTextSelection:
            return QColor(accent.red(), accent.green(), accent.blue(), 130);
        case ThemeColor::AccentTextSelectionOnAccent:
            return selectionColorOn(accentBackground);
        case ThemeColor::TextOnAccent:
            return textColorOn(accentBackground);
        case ThemeColor::SecondaryTextOnAccent:
            return textColorOn(accentBackground, 165);
        default:
            return QColor();
        }
    }

    switch (role) {
    case ThemeColor::Accent:
    case ThemeColor::ListSelected:
        return accent.darker(112);
    case ThemeColor::AccentHover:
        return accent;
    case ThemeColor::AccentPressed:
        return accent.darker(125);
    case ThemeColor::AccentBubble:
        return accent.darker(108);
    case ThemeColor::AccentLinkText:
        return accent.lighter(145);
    case ThemeColor::AccentLinkTextOnAccent:
        return textColorOn(accentBackground);
    case ThemeColor::AccentTextSelection:
        return QColor(accent.red(), accent.green(), accent.blue(), 150);
    case ThemeColor::AccentTextSelectionOnAccent:
        return selectionColorOn(accentBackground);
    case ThemeColor::TextOnAccent:
        return textColorOn(accentBackground);
    case ThemeColor::SecondaryTextOnAccent:
        return textColorOn(accentBackground, 165);
    default:
        return QColor();
    }
}

QColor ThemeManager::color(ThemeColor role) const
{
    const bool dark = isDark();
    return isAccentColorRole(role) ? accentColor(role, dark) : fixedColor(role, dark);
}

QColor ThemeManager::fixedColor(ThemeColor role, bool dark) const
{
    if (!dark) {
        switch (role) {
        case ThemeColor::WindowBackground:
            return QColor(0xf8, 0xf8, 0xfc);
        case ThemeColor::PageBackground:
            return QColor(0xf2, 0xf2, 0xf2);
        case ThemeColor::PanelBackground:
            return QColor(0xff, 0xff, 0xff);
        case ThemeColor::PanelRaisedBackground:
        case ThemeColor::InputBackground:
            return QColor(0xf5, 0xf5, 0xf5);
        case ThemeColor::InputFocusBackground:
            return QColor(0xff, 0xff, 0xff);
        case ThemeColor::PrimaryText:
            return QColor(0x11, 0x11, 0x11);
        case ThemeColor::SecondaryText:
            return QColor(0x55, 0x55, 0x55);
        case ThemeColor::TertiaryText:
        case ThemeColor::PlaceholderText:
            return QColor(0x88, 0x88, 0x88);
        case ThemeColor::Divider:
            return QColor(0xe9, 0xe9, 0xe9);
        case ThemeColor::ListHover:
            return QColor(0xf0, 0xf0, 0xf0);
        case ThemeColor::ListPinned:
            return QColor(0xec, 0xec, 0xec);
        case ThemeColor::ChatInfoPanelOverlay:
            return QColor(0, 0, 0, 88);
        case ThemeColor::ChatInfoPanelFallbackBackground:
            return QColor(0xef, 0xef, 0xef);
        case ThemeColor::ChatInfoPanelCardBackground:
            return QColor(0, 0, 0, 150);
        case ThemeColor::ChatInfoPanelCardHover:
            return QColor(0, 0, 0, 170);
        case ThemeColor::ChatInfoPanelCardPressed:
            return QColor(0, 0, 0, 190);
        case ThemeColor::SettingsOverlay:
            return QColor(0, 0, 0, 168);
        case ThemeColor::SettingsFallbackBackground:
            return QColor(0xb0, 0xb0, 0xb0);
        case ThemeColor::ChatInfoMemberListBackground:
            return QColor(0xff, 0xff, 0xff);
        case ThemeColor::ChatInfoMemberListHover:
            return QColor(0xe8, 0xe8, 0xe8);
        case ThemeColor::ChatInfoMemberListPrimaryText:
            return QColor(0x12, 0x12, 0x12);
        case ThemeColor::ChatInfoMemberListSecondaryText:
            return QColor(0x88, 0x88, 0x88);
        case ThemeColor::ChatInfoMemberListHeaderText:
            return QColor(0xff, 0xff, 0xff);
        case ThemeColor::DangerText:
            return QColor(0xd9, 0x36, 0x36);
        case ThemeColor::DangerControlHover:
            return QColor(0xff, 0xf4, 0xf4);
        case ThemeColor::DangerControlPressed:
            return QColor(0xff, 0xe8, 0xe8);
        case ThemeColor::AppBarItemBackground:
            return QColor(0xd8, 0xd8, 0xd8, 224);
        case ThemeColor::AppBarItemSelectedBackground:
            return QColor(0xd8, 0xd8, 0xd8);
        case ThemeColor::PostBarItemSelectedBackground:
            return QColor(0, 0, 0, 32);
        case ThemeColor::ImagePlaceholder:
            return QColor(0xf2, 0xf2, 0xf2);
        case ThemeColor::ControlHover:
            return QColor(0, 0, 0, 14);
        case ThemeColor::ControlPressed:
            return QColor(0, 0, 0, 28);
        case ThemeColor::MessageBubblePeer:
            return QColor(0xff, 0xff, 0xff);
        case ThemeColor::MessageBubblePeerSelected:
            return QColor(0xea, 0xea, 0xea);
        case ThemeColor::ContextMenuBackground:
            return QColor(0xf8, 0xf8, 0xf8);
        case ThemeColor::ContextMenuBorder:
            return QColor(0xda, 0xda, 0xda);
        case ThemeColor::ContextMenuHover:
            return QColor(0xea, 0xea, 0xea);
        case ThemeColor::ContextMenuSeparator:
            return QColor(0xe5, 0xe5, 0xe5);
        case ThemeColor::ContextMenuText:
            return QColor(0x22, 0x22, 0x22);
        case ThemeColor::ContextMenuDisabledText:
            return QColor(0xa0, 0xa0, 0xa0);
        case ThemeColor::ContextMenuShortcutText:
            return QColor(0x68, 0x68, 0x68);
        case ThemeColor::PopupShadow:
            return QColor(0, 0, 0, 50);
        case ThemeColor::FloatingPanelShadow:
            return QColor(150, 150, 150, 220);
        case ThemeColor::NewMessageNotifierBackground:
            return QColor(0xff, 0xff, 0xff);
        case ThemeColor::NewMessageNotifierHover:
            return QColor(0xf8, 0xf8, 0xf8);
        case ThemeColor::NewMessageNotifierPressed:
            return QColor(0xee, 0xee, 0xee);
        case ThemeColor::NewMessageNotifierText:
            return QColor(0x12, 0x12, 0x12);
        case ThemeColor::NewMessageNotifierShadow:
            return QColor(0, 0, 0, 54);
        case ThemeColor::BadgeUnreadBackground:
            return QColor(0xf7, 0x4c, 0x30);
        case ThemeColor::BadgeUnreadText:
            return QColor(0xff, 0xff, 0xff);
        case ThemeColor::BadgeMutedBackground:
            return QColor(0xcc, 0xcc, 0xcc);
        case ThemeColor::BadgeMutedText:
            return QColor(0xff, 0xfa, 0xfa);
        case ThemeColor::BadgeSelectedBackground:
            return QColor(0xff, 0xff, 0xff, 210);
        case ThemeColor::RoleOwnerBackground:
            return QColor(0xf5, 0xdd, 0xcb);
        case ThemeColor::RoleAdminBackground:
            return QColor(0xc2, 0xe1, 0xf5);
        case ThemeColor::RoleOwnerText:
            return QColor(0xff, 0x9c, 0x00);
        case ThemeColor::RoleAdminText:
            return QColor(0x00, 0x66, 0xcc);
        case ThemeColor::DestructiveActionText:
            return QColor(0xeb, 0x57, 0x57);
        case ThemeColor::DestructiveActionBackground:
            return QColor(0xff, 0xff, 0xff);
        case ThemeColor::NotificationFallbackBackground:
            return QColor(45, 45, 45, 235);
        case ThemeColor::NotificationTitleShadow:
            return QColor(0, 0, 0, 185);
        case ThemeColor::NotificationTitleText:
            return QColor(0xff, 0xff, 0x55);
        case ThemeColor::NotificationBodyText:
            return QColor(0xff, 0xff, 0xff);
        case ThemeColor::NotificationCloseBackground:
            return QColor(255, 255, 255, 220);
        case ThemeColor::NotificationCloseIcon:
            return QColor(0, 0, 0, 210);
        case ThemeColor::WindowBackdropTint:
            return QColor(248, 248, 252, 92);
        case ThemeColor::WindowCloseHover:
            return QColor(0xc4, 0x2b, 0x1c);
        case ThemeColor::OverlayStroke:
            return QColor(0, 0, 0, 150);
        case ThemeColor::TooltipBackground:
            return QColor(255, 255, 255, 120);
        case ThemeColor::TooltipText:
            return QColor(0x33, 0x33, 0x33);
        case ThemeColor::MediaOverlayStart:
            return QColor(48, 48, 48, 70);
        case ThemeColor::MediaOverlayEnd:
            return QColor(32, 32, 32, 132);
        case ThemeColor::PostOverlay:
            return QColor(0, 0, 0, 100);
        case ThemeColor::ScrollThumb:
            return QColor(128, 128, 128, 80);
        default:
            return QColor();
        }
    }

    switch (role) {
    case ThemeColor::WindowBackground:
        return QColor(0x18, 0x19, 0x1c);
    case ThemeColor::PageBackground:
        return QColor(0x1b, 0x1c, 0x20);
    case ThemeColor::PanelBackground:
        return QColor(0x20, 0x21, 0x26);
    case ThemeColor::PanelRaisedBackground:
        return QColor(0x2a, 0x2c, 0x32);
    case ThemeColor::InputBackground:
        return QColor(0x2a, 0x2c, 0x32);
    case ThemeColor::InputFocusBackground:
        return QColor(0x30, 0x33, 0x3a);
    case ThemeColor::PrimaryText:
        return QColor(0xf1, 0xf3, 0xf5);
    case ThemeColor::SecondaryText:
        return QColor(0xc3, 0xc7, 0xce);
    case ThemeColor::TertiaryText:
        return QColor(0x9a, 0xa1, 0xac);
    case ThemeColor::PlaceholderText:
        return QColor(0x82, 0x89, 0x94);
    case ThemeColor::Divider:
        return QColor(0x36, 0x39, 0x40);
    case ThemeColor::ListHover:
        return QColor(0x2c, 0x2f, 0x36);
    case ThemeColor::ListPinned:
        return QColor(0x30, 0x33, 0x3a);
    case ThemeColor::ChatInfoPanelOverlay:
        return QColor(0, 0, 0, 132);
    case ThemeColor::ChatInfoPanelFallbackBackground:
        return QColor(0x20, 0x21, 0x26);
    case ThemeColor::ChatInfoPanelCardBackground:
        return QColor(0, 0, 0, 150);
    case ThemeColor::ChatInfoPanelCardHover:
        return QColor(0, 0, 0, 170);
    case ThemeColor::ChatInfoPanelCardPressed:
        return QColor(0, 0, 0, 190);
    case ThemeColor::SettingsOverlay:
        return QColor(0, 0, 0, 168);
    case ThemeColor::SettingsFallbackBackground:
        return QColor(0x20, 0x21, 0x26);
    case ThemeColor::ChatInfoMemberListBackground:
        return QColor(0x20, 0x21, 0x26);
    case ThemeColor::ChatInfoMemberListHover:
        return QColor(0x2c, 0x2f, 0x36);
    case ThemeColor::ChatInfoMemberListPrimaryText:
        return QColor(0xf1, 0xf3, 0xf5);
    case ThemeColor::ChatInfoMemberListSecondaryText:
        return QColor(0xc3, 0xc7, 0xce);
    case ThemeColor::ChatInfoMemberListHeaderText:
        return QColor(0xf1, 0xf3, 0xf5);
    case ThemeColor::DangerText:
        return QColor(0xd9, 0x36, 0x36);
    case ThemeColor::DangerControlHover:
        return QColor(0x45, 0x2b, 0x2f);
    case ThemeColor::DangerControlPressed:
        return QColor(0x54, 0x30, 0x35);
    case ThemeColor::AppBarItemBackground:
        return QColor(0x3a, 0x3d, 0x44, 224);
    case ThemeColor::AppBarItemSelectedBackground:
        return QColor(0x3a, 0x3d, 0x44);
    case ThemeColor::PostBarItemSelectedBackground:
        return QColor(0x5a, 0x5d, 0x64, 200);
    case ThemeColor::ImagePlaceholder:
        return QColor(0x2b, 0x2d, 0x33);
    case ThemeColor::ControlHover:
        return QColor(255, 255, 255, 18);
    case ThemeColor::ControlPressed:
        return QColor(255, 255, 255, 34);
    case ThemeColor::MessageBubblePeer:
        return QColor(0x2d, 0x31, 0x38);
    case ThemeColor::MessageBubblePeerSelected:
        return QColor(0x25, 0x29, 0x30);
    case ThemeColor::ContextMenuBackground:
        return QColor(0x20, 0x21, 0x26);
    case ThemeColor::ContextMenuBorder:
        return QColor(0x36, 0x39, 0x40);
    case ThemeColor::ContextMenuHover:
        return QColor(0x2c, 0x2f, 0x36);
    case ThemeColor::ContextMenuSeparator:
        return QColor(0x36, 0x39, 0x40);
    case ThemeColor::ContextMenuText:
        return QColor(0xf1, 0xf3, 0xf5);
    case ThemeColor::ContextMenuDisabledText:
        return QColor(0x82, 0x89, 0x94);
    case ThemeColor::ContextMenuShortcutText:
        return QColor(0x9a, 0xa1, 0xac);
    case ThemeColor::PopupShadow:
        return QColor(0, 0, 0, 132);
    case ThemeColor::FloatingPanelShadow:
        return QColor(0, 0, 0, 132);
    case ThemeColor::NewMessageNotifierBackground:
        return QColor(0x0f, 0x10, 0x13);
    case ThemeColor::NewMessageNotifierHover:
        return QColor(0x1b, 0x1d, 0x22);
    case ThemeColor::NewMessageNotifierPressed:
        return QColor(0x22, 0x24, 0x29);
    case ThemeColor::NewMessageNotifierText:
        return QColor(0xff, 0xff, 0xff);
    case ThemeColor::NewMessageNotifierShadow:
        return QColor(0, 0, 0, 90);
    case ThemeColor::BadgeUnreadBackground:
        return QColor(0xf7, 0x4c, 0x30);
    case ThemeColor::BadgeUnreadText:
        return QColor(0xff, 0xff, 0xff);
    case ThemeColor::BadgeMutedBackground:
        return QColor(0x4e, 0x4e, 0x4e);
    case ThemeColor::BadgeMutedText:
        return QColor(0xff, 0xff, 0xff);
    case ThemeColor::BadgeSelectedBackground:
        return QColor(0xff, 0xff, 0xff, 210);
    case ThemeColor::RoleOwnerBackground:
        return QColor(0xf5, 0xdd, 0xcb);
    case ThemeColor::RoleAdminBackground:
        return QColor(0x2a, 0x3a, 0x46);
    case ThemeColor::RoleOwnerText:
        return QColor(0xff, 0xb3, 0x42);
    case ThemeColor::RoleAdminText:
        return QColor(0x6a, 0xb7, 0xff);
    case ThemeColor::DestructiveActionText:
        return QColor(0xeb, 0x57, 0x57);
    case ThemeColor::DestructiveActionBackground:
        return QColor(0x20, 0x21, 0x26);
    case ThemeColor::NotificationFallbackBackground:
        return QColor(45, 45, 45, 235);
    case ThemeColor::NotificationTitleShadow:
        return QColor(0, 0, 0, 185);
    case ThemeColor::NotificationTitleText:
        return QColor(0xff, 0xff, 0x55);
    case ThemeColor::NotificationBodyText:
        return QColor(0xff, 0xff, 0xff);
    case ThemeColor::NotificationCloseBackground:
        return QColor(255, 255, 255, 220);
    case ThemeColor::NotificationCloseIcon:
        return QColor(0, 0, 0, 210);
    case ThemeColor::WindowBackdropTint:
        return QColor(24, 25, 28, 120);
    case ThemeColor::WindowCloseHover:
        return QColor(0xc4, 0x2b, 0x1c);
    case ThemeColor::OverlayStroke:
        return QColor(0, 0, 0, 150);
    case ThemeColor::TooltipBackground:
        return QColor(0x20, 0x21, 0x26, 220);
    case ThemeColor::TooltipText:
        return QColor(0xf1, 0xf3, 0xf5);
    case ThemeColor::MediaOverlayStart:
        return QColor(48, 48, 48, 70);
    case ThemeColor::MediaOverlayEnd:
        return QColor(32, 32, 32, 132);
    case ThemeColor::PostOverlay:
        return QColor(0, 0, 0, 100);
    case ThemeColor::ScrollThumb:
        return QColor(128, 128, 128, 96);
    default:
        return QColor();
    }

    return QColor();
}

QPalette ThemeManager::applicationPalette() const
{
    QPalette palette = QApplication::style() ? QApplication::style()->standardPalette()
                                             : QPalette();
    palette.setColor(QPalette::Window, color(ThemeColor::WindowBackground));
    palette.setColor(QPalette::WindowText, color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::Base, color(ThemeColor::PanelBackground));
    palette.setColor(QPalette::AlternateBase, color(ThemeColor::PanelRaisedBackground));
    palette.setColor(QPalette::Text, color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::Button, color(ThemeColor::PanelRaisedBackground));
    palette.setColor(QPalette::ButtonText, color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::Highlight, color(ThemeColor::Accent));
    palette.setColor(QPalette::HighlightedText, color(ThemeColor::TextOnAccent));
    palette.setColor(QPalette::PlaceholderText, color(ThemeColor::PlaceholderText));
    palette.setColor(QPalette::ToolTipBase, color(ThemeColor::PanelRaisedBackground));
    palette.setColor(QPalette::ToolTipText, color(ThemeColor::PrimaryText));
    return palette;
}

void ThemeManager::applyToApplication(QApplication& application)
{
    m_application = &application;
    installSystemThemeListener(application);
    refreshApplicationTheme();
}

bool ThemeManager::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_application && configuredMode() == Mode::FollowSystem && !m_refreshing) {
        switch (event->type()) {
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
            refreshApplicationTheme();
            break;
        default:
            break;
        }
    }

    return QObject::eventFilter(watched, event);
}

void ThemeManager::installSystemThemeListener(QApplication& application)
{
    application.installEventFilter(this);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QStyleHints* hints = QGuiApplication::styleHints()) {
        connect(hints,
                &QStyleHints::colorSchemeChanged,
                this,
                &ThemeManager::handleSystemColorSchemeChanged,
                Qt::UniqueConnection);
    }
#endif
}

void ThemeManager::handleSystemColorSchemeChanged()
{
    if (configuredMode() == Mode::FollowSystem) {
        refreshApplicationTheme();
    }
}

void ThemeManager::refreshApplicationTheme()
{
    if (!m_application || m_refreshing) {
        return;
    }

    QScopedValueRollback<bool> guard(m_refreshing, true);

    m_application->setPalette(applicationPalette());

    emit themeChanged();

    const auto refreshWidget = [](QWidget* widget) {
        if (!widget) {
            return;
        }
        if (QStyle* style = widget->style()) {
            style->unpolish(widget);
            style->polish(widget);
        }
        if (auto* scrollArea = qobject_cast<QAbstractScrollArea*>(widget)) {
            if (QWidget* viewport = scrollArea->viewport()) {
                viewport->update();
            }
        }
        if (auto* itemView = qobject_cast<QAbstractItemView*>(widget)) {
            itemView->doItemsLayout();
        }
        widget->update();
    };

    const QWidgetList widgets = QApplication::allWidgets();
    for (QWidget* widget : widgets) {
        refreshWidget(widget);
    }
}

bool ThemeManager::systemIsDark() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (const QStyleHints* hints = QGuiApplication::styleHints()) {
        if (hints->colorScheme() == Qt::ColorScheme::Dark) {
            return true;
        }
        if (hints->colorScheme() == Qt::ColorScheme::Light) {
            return false;
        }
    }
#endif

    const QPalette palette = QApplication::style() ? QApplication::style()->standardPalette()
                                                   : QApplication::palette();
    return palette.color(QPalette::Window).lightness() < 128;
}
