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
#include <QtGlobal>

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

QColor ThemeManager::color(ThemeColor role) const
{
    const QColor accent(0x00, 0x99, 0xff);
    if (!isDark()) {
        switch (role) {
        case ThemeColor::Accent:
        case ThemeColor::ListSelected:
            return accent;
        case ThemeColor::AccentHover:
            return QColor(0x00, 0x88, 0xee);
        case ThemeColor::AccentPressed:
            return QColor(0x00, 0x77, 0xdd);
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
        case ThemeColor::SettingsOverlay:
            return QColor(0, 0, 0, 168);
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
        }
    }

    const QColor darkAccent(0x00, 0x78, 0xd4);

    switch (role) {
    case ThemeColor::Accent:
    case ThemeColor::ListSelected:
        return darkAccent;
    case ThemeColor::AccentHover:
        return QColor(0x00, 0x86, 0xe4);
    case ThemeColor::AccentPressed:
        return QColor(0x00, 0x66, 0xb8);
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
    case ThemeColor::SettingsOverlay:
        return QColor(0, 0, 0, 168);
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
    palette.setColor(QPalette::HighlightedText, Qt::white);
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
