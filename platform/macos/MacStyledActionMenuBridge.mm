#include "MacStyledActionMenuBridge_p.h"

#ifdef Q_OS_MACOS

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include <QAction>
#include <QColor>
#include <QMenu>
#include <QWidget>

@interface NLStyledActionMenuTarget : NSObject
@property(nonatomic, assign) QAction* action;
- (instancetype)initWithAction:(QAction*)action;
- (void)handleAction:(id)sender;
@end

@implementation NLStyledActionMenuTarget

- (instancetype)initWithAction:(QAction*)action
{
    self = [super init];
    if (self) {
        self.action = action;
    }
    return self;
}

- (void)handleAction:(id)sender
{
    Q_UNUSED(sender);
    if (self.action && self.action->isEnabled()) {
        self.action->trigger();
    }
}

@end

@interface NLStyledActionMenuDelegate : NSObject <NSMenuDelegate> {
@public
    std::function<void()>* closeHandler;
}
@end

@implementation NLStyledActionMenuDelegate

- (void)menuDidClose:(NSMenu*)menu
{
    Q_UNUSED(menu);
    if (closeHandler && *closeHandler) {
        (*closeHandler)();
    }
}

@end

namespace {

constexpr auto ActionTextColorProperty = "_styledActionMenu_textColor";
const void* const kNativeTargetsAssociationKey = &kNativeTargetsAssociationKey;
const void* const kNativeDelegateAssociationKey = &kNativeDelegateAssociationKey;

constexpr int kNativeMenuMode = 1;
constexpr int kNativeMenuAliasMode = 2;
constexpr int kQtFallbackMode = 3;

int defaultConfiguredMode()
{
    switch (MacStyledActionMenuBridge::kStyledActionMenuTestMode) {
    case kNativeMenuMode:
    case kNativeMenuAliasMode:
    case kQtFallbackMode:
        return MacStyledActionMenuBridge::kStyledActionMenuTestMode;
    default:
        return kNativeMenuMode;
    }
}

int& configuredModeStorage()
{
    static int mode = defaultConfiguredMode();
    return mode;
}

NSString* nsStringFromQString(const QString& value)
{
    return [NSString stringWithUTF8String:value.toUtf8().constData()];
}

QString menuText(const QString& text)
{
    QString label = text.split(QLatin1Char('\t')).constFirst();
    label.remove(QLatin1Char('&'));
    return label;
}

NSColor* nsColorFromQColor(const QColor& color)
{
    if (!color.isValid()) {
        return nil;
    }

    return [NSColor colorWithSRGBRed:color.redF()
                               green:color.greenF()
                                blue:color.blueF()
                               alpha:color.alphaF()];
}

void applyNativeMenuAppearance(NSMenu* menu, MacStyledActionMenuBridge::Appearance appearance)
{
    if (!menu) {
        return;
    }

    if (@available(macOS 14.0, *)) {
        menu.presentationStyle = NSMenuPresentationStyleRegular;
    }

    switch (appearance) {
    case MacStyledActionMenuBridge::Appearance::NativeMenu:
        // AppKit owns the material: macOS 26+ uses Liquid Glass, older macOS uses native blur.
        menu.appearance = nil;
        break;
    case MacStyledActionMenuBridge::Appearance::Unsupported:
    default:
        break;
    }
}

int normalizedTestMode()
{
    switch (configuredModeStorage()) {
    case kNativeMenuMode:
    case kNativeMenuAliasMode:
    case kQtFallbackMode:
        return configuredModeStorage();
    default:
        return kNativeMenuMode;
    }
}

bool supportsNativeMenu()
{
    return NSClassFromString(@"NSMenu") != nil;
}

MacStyledActionMenuBridge::Appearance resolvedAppearanceForMode(int mode)
{
    if ((mode == kNativeMenuMode || mode == kNativeMenuAliasMode) && supportsNativeMenu()) {
        return MacStyledActionMenuBridge::Appearance::NativeMenu;
    }

    return MacStyledActionMenuBridge::Appearance::Unsupported;
}

MacStyledActionMenuBridge::Appearance currentAppearance()
{
    return resolvedAppearanceForMode(normalizedTestMode());
}

MacStyledActionMenuBridge::Mode effectiveModeForMode(int mode)
{
    if ((mode == kNativeMenuMode || mode == kNativeMenuAliasMode) && supportsNativeMenu()) {
        return MacStyledActionMenuBridge::Mode::NativeMenu;
    }

    return MacStyledActionMenuBridge::Mode::QtFallback;
}

QWidget* anchorForWidget(QWidget* widget)
{
    if (!widget) {
        return nullptr;
    }

    if (QWidget* parent = widget->parentWidget()) {
        return parent;
    }

    return widget;
}

NSView* topLevelQtViewForWidget(QWidget* widget)
{
    QWidget* anchor = anchorForWidget(widget);
    if (!anchor) {
        return nil;
    }

    QWidget* topLevelWidget = anchor->window();
    if (!topLevelWidget) {
        return nil;
    }

    const WId wid = topLevelWidget->winId();
    return wid ? reinterpret_cast<NSView*>(wid) : nil;
}

NSPoint menuPointInView(QWidget* widget, NSView* hostView, const QPoint& globalPos)
{
    QWidget* anchor = anchorForWidget(widget);
    QWidget* topLevelWidget = anchor ? anchor->window() : nullptr;
    if (!topLevelWidget || !hostView) {
        return NSMakePoint(globalPos.x(), globalPos.y());
    }

    const QPoint localPoint = topLevelWidget->mapFromGlobal(globalPos);
    const CGFloat x = static_cast<CGFloat>(localPoint.x());
    const CGFloat y = [hostView isFlipped]
            ? static_cast<CGFloat>(localPoint.y())
            : NSHeight(hostView.bounds) - static_cast<CGFloat>(localPoint.y());
    return NSMakePoint(x, y);
}

void applyActionTextColor(NSMenuItem* item, QAction* action)
{
    const QColor color = action
            ? action->property(ActionTextColorProperty).value<QColor>()
            : QColor();
    NSColor* textColor = nsColorFromQColor(color);
    if (!item || !textColor) {
        return;
    }

    NSDictionary* attributes = @{NSForegroundColorAttributeName: textColor};
    item.attributedTitle = [[[NSAttributedString alloc] initWithString:item.title
                                                            attributes:attributes] autorelease];
}

NSMenu* buildNativeMenu(QMenu* sourceMenu,
                        NSMutableArray* retainedTargets,
                        MacStyledActionMenuBridge::Appearance appearance)
{
    NSMenu* nativeMenu = [[[NSMenu alloc]
        initWithTitle:nsStringFromQString(sourceMenu ? sourceMenu->title() : QString()) ?: @""]
        autorelease];
    nativeMenu.autoenablesItems = NO;
    if (sourceMenu && sourceMenu->minimumWidth() > 0) {
        nativeMenu.minimumWidth = static_cast<CGFloat>(sourceMenu->minimumWidth());
    }
    applyNativeMenuAppearance(nativeMenu, appearance);

    if (!sourceMenu) {
        return nativeMenu;
    }

    for (QAction* action : sourceMenu->actions()) {
        if (!action || !action->isVisible()) {
            continue;
        }

        if (action->isSeparator()) {
            [nativeMenu addItem:[NSMenuItem separatorItem]];
            continue;
        }

        NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:nsStringFromQString(menuText(action->text()))
                                                       action:nil
                                                keyEquivalent:@""]
                autorelease];
        item.enabled = action->isEnabled();
        item.state = action->isCheckable() && action->isChecked() ? NSControlStateValueOn : NSControlStateValueOff;

        if (QMenu* submenu = action->menu()) {
            NSMenu* nativeSubmenu = buildNativeMenu(submenu, retainedTargets, appearance);
            item.submenu = nativeSubmenu;
        } else {
            NLStyledActionMenuTarget* target = [[NLStyledActionMenuTarget alloc] initWithAction:action];
            item.target = target;
            item.action = @selector(handleAction:);
            [retainedTargets addObject:target];
            [target release];
        }

        applyActionTextColor(item, action);
        [nativeMenu addItem:item];
    }

    objc_setAssociatedObject(nativeMenu,
                             kNativeTargetsAssociationKey,
                             retainedTargets,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return nativeMenu;
}

} // namespace

namespace MacStyledActionMenuBridge {

Mode mode()
{
    return effectiveModeForMode(normalizedTestMode());
}

void setMode(Mode mode)
{
    switch (mode) {
    case Mode::NativeMenu:
    case Mode::QtFallback:
        configuredModeStorage() = static_cast<int>(mode);
        break;
    }
}

QVector<Mode> supportedModes()
{
    QVector<Mode> modes;
    if (supportsNativeMenu()) {
        modes.push_back(Mode::NativeMenu);
    }
    modes.push_back(Mode::QtFallback);
    return modes;
}

Appearance appearance()
{
    return currentAppearance();
}

bool isSupported()
{
    return appearance() != Appearance::Unsupported;
}

bool popupMenu(QWidget* anchorWidget,
               QMenu* menu,
               const QPoint& globalPos,
               const std::function<void()>& closeHandler)
{
    const Appearance currentAppearance = appearance();
    if (!anchorWidget || !menu || currentAppearance == Appearance::Unsupported) {
        return false;
    }

    NSView* hostView = topLevelQtViewForWidget(anchorWidget);
    if (!hostView) {
        return false;
    }

    NSMutableArray* targets = [NSMutableArray array];
    NSMenu* nativeMenu = buildNativeMenu(menu, targets, currentAppearance);
    NLStyledActionMenuDelegate* delegate = [[NLStyledActionMenuDelegate alloc] init];
    std::function<void()> mutableCloseHandler = closeHandler;
    delegate->closeHandler = &mutableCloseHandler;
    nativeMenu.delegate = delegate;
    objc_setAssociatedObject(nativeMenu,
                             kNativeDelegateAssociationKey,
                             delegate,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [delegate release];

    const NSPoint point = menuPointInView(anchorWidget, hostView, globalPos);
    const BOOL result = [nativeMenu popUpMenuPositioningItem:nil atLocation:point inView:hostView];
    delegate->closeHandler = nullptr;
    return result;
}

} // namespace MacStyledActionMenuBridge

#endif
