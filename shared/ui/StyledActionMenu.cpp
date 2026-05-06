#include "StyledActionMenu.h"

#include <QAction>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QVariant>

#ifdef Q_OS_MACOS
#include "platform/macos/MacStyledActionMenuBridge_p.h"
#endif

namespace {
constexpr auto ActionTextColorProperty = "_styledActionMenu_textColor";
constexpr auto ActionHoverTextColorProperty = "_styledActionMenu_hoverTextColor";
constexpr auto ActionHoverBackgroundColorProperty = "_styledActionMenu_hoverBackgroundColor";
constexpr auto MenuHoverColorProperty = "_styledActionMenu_hoverColor";
constexpr auto MenuSeparatorColorProperty = "_styledActionMenu_separatorColor";

constexpr int ShadowMargin = 4;
constexpr int MenuRadius = 7;
constexpr int MenuPadding = 4;
constexpr int ItemRadius = 4;
constexpr int ItemHeight = 28;
constexpr int ItemHorizontalPadding = 10;
constexpr int ItemIconGap = 6;
constexpr int ArrowWidth = 10;
constexpr int ItemFontSize = 13;
constexpr int SeparatorHeight = 3;
constexpr int MaxMouseReleaseWaitAttempts = 40;
constexpr int MouseReleasePollIntervalMs = 4;

QColor readColor(const QVariant& value, const QColor& fallback)
{
    return value.canConvert<QColor>() ? value.value<QColor>() : fallback;
}

QString menuText(const QString& text)
{
    QString label = text.split(QLatin1Char('\t')).constFirst();
    label.remove(QLatin1Char('&'));
    return label;
}

QString shortcutText(const QString& text)
{
    const QStringList parts = text.split(QLatin1Char('\t'));
    return parts.size() > 1 ? parts.constLast() : QString();
}

class StyledActionMenuSizeStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    QSize sizeFromContents(ContentsType type, const QStyleOption* option,
                           const QSize& size, const QWidget* widget) const override
    {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type != CT_MenuItem) {
            return result;
        }

        const auto* menuItem = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
        if (menuItem && menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
            result.setHeight(SeparatorHeight);
            return result;
        }

        result.setHeight(ItemHeight);
        result.rwidth() += 20;
        return result;
    }
};
}

StyledActionMenu::StyledActionMenu(QWidget* parent)
    : QMenu(parent)
{
    setProperty(MenuHoverColorProperty, QColor(234, 234, 234));
    setProperty(MenuSeparatorColorProperty, QColor(229, 229, 229));
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setContentsMargins(ShadowMargin + MenuPadding,
                       ShadowMargin + MenuPadding,
                       ShadowMargin + MenuPadding,
                       ShadowMargin + MenuPadding);
    if (!usesNativeMenu()) {
        auto* shadowEffect = new QGraphicsDropShadowEffect(this);
        shadowEffect->setBlurRadius(22);
        shadowEffect->setOffset(0, 1);
        shadowEffect->setColor(QColor(0, 0, 0, 50));
        setGraphicsEffect(shadowEffect);
    }

    auto* menuStyle = new StyledActionMenuSizeStyle;
    menuStyle->setParent(this);
    setStyle(menuStyle);
}

StyledActionMenu::~StyledActionMenu()
{
}

StyledActionMenu* StyledActionMenu::addStyledMenu(const QString& title)
{
    auto* menu = new StyledActionMenu(this);
    menu->setTitle(title);
    addMenu(menu);
    return menu;
}

void StyledActionMenu::popup(const QPoint& pos, QAction* atAction)
{
#ifdef Q_OS_MACOS
    if (usesNativeMenu()) {
        Q_UNUSED(atAction);
        bool hideEmitted = false;
        const auto emitNativeHide = [this, &hideEmitted]() {
            if (hideEmitted) {
                return;
            }

            hideEmitted = true;
            emit aboutToHide();
        };

        emit aboutToShow();
        MacStyledActionMenuBridge::popupMenu(this, this, pos, emitNativeHide);
        emitNativeHide();
        return;
    }
#endif

    QMenu::popup(pos, atAction);
}

void StyledActionMenu::popupWhenMouseReleased(const QPoint& pos, QAction* atAction)
{
    popupWhenMouseReleased(pos, atAction, 0);
}

void StyledActionMenu::popupWhenMouseReleased(const QPoint& pos, QAction* atAction, int attempt)
{
    const Qt::MouseButtons buttons = QGuiApplication::mouseButtons();
    const bool hasPressedButton = buttons.testFlag(Qt::LeftButton) ||
            buttons.testFlag(Qt::RightButton) ||
            buttons.testFlag(Qt::MiddleButton);

    if (!hasPressedButton || attempt >= MaxMouseReleaseWaitAttempts) {
        popup(pos, atAction);
        return;
    }

    QTimer::singleShot(MouseReleasePollIntervalMs, this, [this, pos, atAction, attempt]() {
        popupWhenMouseReleased(pos, atAction, attempt + 1);
    });
}

bool StyledActionMenu::isUsingNativeMenu() const
{
    return usesNativeMenu();
}

void StyledActionMenu::setItemHoverColor(const QColor& color)
{
    setProperty(MenuHoverColorProperty, color);
    update();
}

void StyledActionMenu::setSeparatorColor(const QColor& color)
{
    setProperty(MenuSeparatorColorProperty, color);
    update();
}

void StyledActionMenu::setActionTextColor(QAction* action, const QColor& color)
{
    if (!action) {
        return;
    }

    action->setProperty(ActionTextColorProperty, color);
}

void StyledActionMenu::setActionHoverTextColor(QAction* action, const QColor& color)
{
    if (!action) {
        return;
    }

    action->setProperty(ActionHoverTextColorProperty, color);
}

void StyledActionMenu::setActionHoverBackgroundColor(QAction* action, const QColor& color)
{
    if (!action) {
        return;
    }

    action->setProperty(ActionHoverBackgroundColorProperty, color);
}

void StyledActionMenu::setActionColors(QAction* action, const QColor& textColor,
                                       const QColor& hoverTextColor)
{
    setActionTextColor(action, textColor);
    setActionHoverTextColor(action, hoverTextColor);
}

void StyledActionMenu::setActionColors(QAction* action, const QColor& textColor,
                                       const QColor& hoverTextColor,
                                       const QColor& hoverBackgroundColor)
{
    setActionColors(action, textColor, hoverTextColor);
    setActionHoverBackgroundColor(action, hoverBackgroundColor);
}

void StyledActionMenu::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF panelRect = QRectF(rect()).adjusted(ShadowMargin, ShadowMargin,
                                                    -ShadowMargin, -ShadowMargin);
    painter.setPen(QPen(QColor(218, 218, 218), 1));
    painter.setBrush(QColor(248, 248, 248));
    painter.drawRoundedRect(panelRect.adjusted(0, 0, 0, 0), MenuRadius, MenuRadius);

    QFont itemFont = painter.font();
    itemFont.setPixelSize(ItemFontSize);
    painter.setFont(itemFont);

    const QColor separatorColor = readColor(property(MenuSeparatorColorProperty), QColor(229, 229, 229));

    for (QAction* action : actions()) {
        if (!action->isVisible()) {
            continue;
        }

        const QRect actionRect = actionGeometry(action);
        if (!actionRect.isValid()) {
            continue;
        }

        if (action->isSeparator()) {
            const int y = actionRect.center().y();
            painter.setPen(QPen(separatorColor, 1));
            painter.drawLine(actionRect.left() + ItemHorizontalPadding, y,
                             actionRect.right() - ItemHorizontalPadding, y);
            continue;
        }

        const bool enabled = action->isEnabled();
        const bool selected = enabled && activeAction() == action;
        const QRect itemRect = actionRect.adjusted(MenuPadding, 2, -MenuPadding, -2);
        const QColor defaultHoverColor = readColor(property(MenuHoverColorProperty),
                                                  QColor(234, 234, 234));
        const QColor hoverColor = readColor(action->property(ActionHoverBackgroundColorProperty),
                                            defaultHoverColor);

        if (selected) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(hoverColor);
            painter.drawRoundedRect(itemRect, ItemRadius, ItemRadius);
        }

        const QColor defaultTextColor = enabled ? QColor(34, 34, 34) : QColor(160, 160, 160);
        const QColor normalTextColor = readColor(action->property(ActionTextColorProperty),
                                                 defaultTextColor);
        const QColor hoverTextColor = readColor(action->property(ActionHoverTextColorProperty),
                                                normalTextColor);
        painter.setPen(selected ? hoverTextColor : normalTextColor);

        QRect textRect = itemRect.adjusted(ItemHorizontalPadding, 0,
                                           -ItemHorizontalPadding - ArrowWidth, 0);
        if (action->isCheckable()) {
            const int checkSize = 12;
            const QRect checkRect(itemRect.left() + ItemHorizontalPadding,
                                  itemRect.top() + (itemRect.height() - checkSize) / 2,
                                  checkSize,
                                  checkSize);
            if (action->isChecked()) {
                QPen checkPen(selected ? hoverTextColor : normalTextColor,
                              1.8,
                              Qt::SolidLine,
                              Qt::RoundCap,
                              Qt::RoundJoin);
                painter.setPen(checkPen);
                painter.drawLine(QPointF(checkRect.left() + 2.0, checkRect.center().y() + 1.0),
                                 QPointF(checkRect.left() + 5.0, checkRect.bottom() - 2.0));
                painter.drawLine(QPointF(checkRect.left() + 5.0, checkRect.bottom() - 2.0),
                                 QPointF(checkRect.right() - 1.0, checkRect.top() + 2.0));
                painter.setPen(selected ? hoverTextColor : normalTextColor);
            }
            textRect.setLeft(checkRect.right() + ItemIconGap);
        }
        const QIcon actionIcon = action->icon();
        if (!actionIcon.isNull()) {
            const int iconSize = qMin(18, itemRect.height() - 8);
            const QRect iconRect(itemRect.left() + ItemHorizontalPadding,
                                 itemRect.top() + (itemRect.height() - iconSize) / 2,
                                 iconSize, iconSize);
            actionIcon.paint(&painter, iconRect, Qt::AlignCenter,
                             enabled ? QIcon::Normal : QIcon::Disabled);
            textRect.setLeft(iconRect.right() + 1 + ItemIconGap);
        }

        const QString shortcut = shortcutText(action->text());
        QRect shortcutRect;
        if (!shortcut.isEmpty()) {
            const int shortcutWidth = painter.fontMetrics().horizontalAdvance(shortcut);
            shortcutRect = QRect(textRect.right() - shortcutWidth, textRect.top(),
                                 shortcutWidth, textRect.height());
            textRect.setRight(shortcutRect.left() - 12);
        }

        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                         painter.fontMetrics().elidedText(menuText(action->text()),
                                                          Qt::ElideRight, textRect.width()));

        if (!shortcut.isEmpty()) {
            painter.drawText(shortcutRect, Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine,
                             shortcut);
        }

        if (action->menu()) {
            const int cx = itemRect.right() - ItemHorizontalPadding - 3;
            const int cy = itemRect.center().y();
            QPainterPath arrow;
            arrow.moveTo(cx - 3, cy - 5);
            arrow.lineTo(cx + 3, cy);
            arrow.lineTo(cx - 3, cy + 5);
            painter.setPen(QPen(selected ? hoverTextColor : QColor(104, 104, 104), 1.6,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(arrow);
        }
    }
}

bool StyledActionMenu::usesNativeMenu() const
{
#ifdef Q_OS_MACOS
    return MacStyledActionMenuBridge::isSupported();
#else
    return false;
#endif
}
