#include "MacFloatingInputBarBridge_p.h"

#ifdef Q_OS_MACOS

#import <AppKit/AppKit.h>
#if __has_include(<AppKit/NSGlassEffectView.h>)
#import <AppKit/NSGlassEffectView.h>
#define NETHERLINK_HAS_NS_GLASS_EFFECT_VIEW 1
#else
#define NETHERLINK_HAS_NS_GLASS_EFFECT_VIEW 0
#endif
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>

#include <QBuffer>
#include <QPainter>
#include <QPixmap>
#include <QWidget>
#include "shared/ui/FloatingInputBar.h"
#include "shared/theme/ThemeManager.h"

class FloatingInputBar;

@class NLFloatingInputShortcutTextView;

namespace {

// Floating input bar layout metrics. Tune these values first when adjusting the native glass UI.
static constexpr CGFloat kCornerRadius = 20.0;
static constexpr CGFloat kInputHorizontalMargin = 18.0;
static constexpr CGFloat kInputTopReserved = 46.0;
static constexpr CGFloat kInputBottomReserved = 14.0;
static constexpr CGFloat kInputRightInset = 50.0;
static constexpr CGFloat kToolbarTopInset = 8.0;
static constexpr CGFloat kToolbarBottomInset = 10.0;
static constexpr CGFloat kToolbarSideInset = 18.0;
static constexpr CGFloat kToolbarButtonSize = 32.0;
static constexpr CGFloat kToolbarButtonSpacing = 6.0;
static constexpr CGFloat kToolbarIconSize = 22.0;
static constexpr CGFloat kSendToolbarIconSize = 26.0;
static constexpr CGFloat kInactiveGlassBorderWidth = 1.0;
static constexpr CGFloat kInactiveGlassBorderAlpha = 0.28;
static constexpr CGFloat kInactiveGlassShadowOpacity = 0.16;
static constexpr CGFloat kInactiveGlassShadowRadius = 18.0;
static constexpr CGFloat kInactiveGlassShadowYOffset = 8.0;

NSColor* chatInputBorderColor(CGFloat alpha)
{
    QColor color = ThemeManager::instance().color(ThemeColor::Divider);
    color.setAlphaF(alpha);
    return [NSColor colorWithCalibratedRed:color.redF()
                                     green:color.greenF()
                                      blue:color.blueF()
                                     alpha:color.alphaF()];
}

QString qStringFromNSString(NSString* string)
{
    if (!string) {
        return {};
    }

    const char* utf8 = string.UTF8String;
    return utf8 ? QString::fromUtf8(utf8) : QString();
}

NSPanel* sharedTooltipPanel()
{
    static NSPanel* panel = nil;
    static NSTextField* label = nil;
    if (!panel) {
        panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 10, 10)
                                           styleMask:NSWindowStyleMaskBorderless
                                             backing:NSBackingStoreBuffered
                                               defer:NO];
        panel.opaque = NO;
        panel.backgroundColor = NSColor.clearColor;
        panel.hasShadow = NO;
        panel.hidesOnDeactivate = NO;
        panel.ignoresMouseEvents = YES;
        panel.level = NSPopUpMenuWindowLevel;
        panel.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
                | NSWindowCollectionBehaviorTransient;

        NSView* contentView = [[[NSView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)] autorelease];
        contentView.wantsLayer = YES;
        contentView.layer.cornerRadius = 7.0;
        contentView.layer.masksToBounds = YES;
        contentView.layer.backgroundColor = [NSColor colorWithWhite:0.97 alpha:0.96].CGColor;
        contentView.layer.borderWidth = 1.0;
        contentView.layer.borderColor = [NSColor colorWithWhite:0.80 alpha:0.85].CGColor;
        panel.contentView = contentView;

        label = [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
        label.editable = NO;
        label.bezeled = NO;
        label.drawsBackground = NO;
        label.selectable = NO;
        label.alignment = NSTextAlignmentCenter;
        label.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightRegular];
        label.textColor = [NSColor colorWithWhite:0.18 alpha:1.0];
        [contentView addSubview:label];
        objc_setAssociatedObject(panel, @selector(sharedTooltipPanel), label, OBJC_ASSOCIATION_ASSIGN);
    }

    return panel;
}

NSTextField* sharedTooltipLabel()
{
    NSPanel* panel = sharedTooltipPanel();
    return static_cast<NSTextField*>(objc_getAssociatedObject(panel, @selector(sharedTooltipPanel)));
}

void showNativeTooltipForButton(NSButton* button, NSString* text)
{
    Q_UNUSED(button);
    Q_UNUSED(text);
    return;

#if 0
    if (!button || text.length == 0 || !button.window) {
        return;
    }

    NSPanel* panel = sharedTooltipPanel();
    NSTextField* label = sharedTooltipLabel();
    if (!panel || !label) {
        return;
    }

    label.stringValue = text;
    [label sizeToFit];

    constexpr CGFloat horizontalPadding = 8.0;
    constexpr CGFloat verticalPadding = 5.0;
    constexpr CGFloat offsetAboveButton = 8.0;

    const NSSize labelSize = label.fittingSize;
    const CGFloat panelWidth = labelSize.width + horizontalPadding * 2.0;
    const CGFloat panelHeight = labelSize.height + verticalPadding * 2.0;
    panel.contentView.frame = NSMakeRect(0, 0, panelWidth, panelHeight);
    label.frame = NSMakeRect(horizontalPadding,
                             verticalPadding,
                             labelSize.width,
                             labelSize.height);
    [panel setContentSize:NSMakeSize(panelWidth, panelHeight)];

    NSRect buttonRectInWindow = [button convertRect:button.bounds toView:nil];
    NSRect buttonRectOnScreen = [button.window convertRectToScreen:buttonRectInWindow];
    const CGFloat panelX = NSMidX(buttonRectOnScreen) - panelWidth / 2.0;
    const CGFloat panelY = NSMaxY(buttonRectOnScreen) + offsetAboveButton;

    [panel setFrame:NSMakeRect(panelX, panelY, panelWidth, panelHeight) display:NO];
    [panel orderFront:nil];
#endif
}

void hideNativeTooltip()
{
    NSPanel* panel = sharedTooltipPanel();
    if (panel) {
        [panel orderOut:nil];
    }
}

NLFloatingInputShortcutTextView* inputTextViewForView(NSView* qtView);
void updateInactiveGlassChrome(NSView* hostView);

} // namespace

@interface NLFloatingInputBarTarget : NSObject
@property(nonatomic, assign) FloatingInputBar* owner;
@property(nonatomic, assign) NSView* hostView;
- (instancetype)initWithOwner:(FloatingInputBar*)owner hostView:(NSView*)hostView;
- (void)handleTextSubmit:(NSString*)text;
- (void)handleHelloShortcut;
- (void)handleImageShortcut;
- (void)handleImageButtonClick:(id)sender;
- (void)handleSendButtonClick:(id)sender;
@end

@interface NLFloatingInputShortcutTextView : NSTextView
@property(nonatomic, assign) NLFloatingInputBarTarget* shortcutTarget;
@end

@interface NLFloatingInputIconButton : NSButton
@property(nonatomic, retain) NSImage* normalImage;
@property(nonatomic, retain) NSImage* hoveredImage;
@property(nonatomic, copy) NSString* hoverTipText;
@property(nonatomic, assign) BOOL hovering;
@property(nonatomic, retain) NSTrackingArea* hoverTrackingArea;
- (void)refreshImage;
@end

@interface NLFloatingInputWindowObserver : NSObject
@property(nonatomic, assign) NSView* hostView;
@property(nonatomic, assign) NSWindow* window;
- (void)bindWindow:(NSWindow*)window hostView:(NSView*)hostView;
@end

@implementation NLFloatingInputBarTarget

- (instancetype)initWithOwner:(FloatingInputBar*)owner hostView:(NSView*)hostView
{
    self = [super init];
    if (self) {
        self.owner = owner;
        self.hostView = hostView;
    }
    return self;
}

- (void)handleTextSubmit:(NSString*)text
{
    if (!self.owner) {
        return;
    }

    NSString* value = text ?: @"";
    const char* utf8 = value.UTF8String;
    self.owner->submitNativeText(utf8 ? QString::fromUtf8(utf8) : QString());
}

- (void)handleHelloShortcut
{
    if (!self.owner) {
        return;
    }

    self.owner->triggerNativeHelloShortcut();
}

- (void)handleImageShortcut
{
    if (!self.owner) {
        return;
    }

    self.owner->triggerNativeImageShortcut();
}

- (void)handleImageButtonClick:(id)sender
{
    Q_UNUSED(sender);
    [self handleImageShortcut];
}

- (void)handleSendButtonClick:(id)sender
{
    Q_UNUSED(sender);
    if (!self.owner || !self.hostView) {
        return;
    }

    NLFloatingInputShortcutTextView* inputField = inputTextViewForView(self.hostView);
    if (!inputField) {
        return;
    }

    NSString* text = inputField.string ?: @"";
    if (self.owner->submitNativeText(qStringFromNSString(text))) {
        [inputField setString:@""];
    }
}

@end

@implementation NLFloatingInputShortcutTextView

- (BOOL)becomeFirstResponder
{
    return [super becomeFirstResponder];
}

- (BOOL)resignFirstResponder
{
    return [super resignFirstResponder];
}

- (void)keyDown:(NSEvent*)event
{
    const NSEventModifierFlags modifiers = event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
    NSString* characters = event.charactersIgnoringModifiers ?: @"";
    if (modifiers & NSEventModifierFlagCommand) {
        const QString chars = qStringFromNSString(characters).toLower();
        if (chars == QStringLiteral("a")) {
            [self selectAll:nil];
            return;
        }
        if (chars == QStringLiteral("c")) {
            [self copy:nil];
            return;
        }
        if (chars == QStringLiteral("v")) {
            [self paste:nil];
            return;
        }
        if (chars == QStringLiteral("x")) {
            [self cut:nil];
            return;
        }
    }
    if (modifiers == 0 && characters.length == 1) {
        unichar key = [characters characterAtIndex:0];
        if (key == ']') {
            [self.shortcutTarget handleHelloShortcut];
            return;
        }
        if (key == '\\') {
            [self.shortcutTarget handleImageShortcut];
            return;
        }
        if (key == '\r' || key == '\n') {
            NSString* text = self.string ?: @"";
            [self.shortcutTarget handleTextSubmit:text];
            [self setString:@""];
            return;
        }
    }

    [super keyDown:event];
}

- (BOOL)performKeyEquivalent:(NSEvent*)event
{
    const NSEventModifierFlags modifiers = event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
    if (modifiers & NSEventModifierFlagCommand) {
        NSString* characters = event.charactersIgnoringModifiers.lowercaseString ?: @"";
        if ([characters isEqualToString:@"a"]) {
            [self selectAll:nil];
            return YES;
        }
        if ([characters isEqualToString:@"c"]) {
            [self copy:nil];
            return YES;
        }
        if ([characters isEqualToString:@"v"]) {
            [self paste:nil];
            return YES;
        }
        if ([characters isEqualToString:@"x"]) {
            [self cut:nil];
            return YES;
        }
    }

    return [super performKeyEquivalent:event];
}

@end

@implementation NLFloatingInputIconButton

- (void)dealloc
{
    [_hoverTrackingArea release];
    [_normalImage release];
    [_hoveredImage release];
    [_hoverTipText release];
    [super dealloc];
}

- (void)refreshImage
{
    self.image = self.hovering && self.hoveredImage ? self.hoveredImage : self.normalImage;
}

- (void)updateTrackingAreas
{
    if (self.hoverTrackingArea) {
        [self removeTrackingArea:self.hoverTrackingArea];
        self.hoverTrackingArea = nil;
    }

    NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited
            | NSTrackingActiveInKeyWindow
            | NSTrackingInVisibleRect;
    NSTrackingArea* trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                                                options:options
                                                                  owner:self
                                                               userInfo:nil];
    self.hoverTrackingArea = trackingArea;
    [trackingArea release];
    [self addTrackingArea:self.hoverTrackingArea];
    [super updateTrackingAreas];
}

- (void)setFrame:(NSRect)frameRect
{
    [super setFrame:frameRect];
    [self updateTrackingAreas];
}

- (void)mouseEntered:(NSEvent*)event
{
    Q_UNUSED(event);
    self.hovering = YES;
    [self refreshImage];
    showNativeTooltipForButton(self, self.hoverTipText);
}

- (void)mouseExited:(NSEvent*)event
{
    Q_UNUSED(event);
    self.hovering = NO;
    [self refreshImage];
    hideNativeTooltip();
}

- (void)resetCursorRects
{
    [self addCursorRect:self.bounds cursor:[NSCursor pointingHandCursor]];
}

@end

@implementation NLFloatingInputWindowObserver

- (void)dealloc
{
    [self bindWindow:nil hostView:nil];
    [super dealloc];
}

- (void)bindWindow:(NSWindow*)window hostView:(NSView*)hostView
{
    if (self.window == window && self.hostView == hostView) {
        return;
    }

    if (self.window) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
                                                        name:NSWindowDidBecomeKeyNotification
                                                      object:self.window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
                                                        name:NSWindowDidResignKeyNotification
                                                      object:self.window];
    }

    self.window = window;
    self.hostView = hostView;

    if (self.window) {
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(handleWindowFocusChange:)
                                                     name:NSWindowDidBecomeKeyNotification
                                                   object:self.window];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(handleWindowFocusChange:)
                                                     name:NSWindowDidResignKeyNotification
                                                   object:self.window];
    }
}

- (void)handleWindowFocusChange:(NSNotification*)notification
{
    Q_UNUSED(notification);
    updateInactiveGlassChrome(self.hostView);
}

@end

namespace {

const void* const kContainerAssociationKey = &kContainerAssociationKey;
const void* const kContentAssociationKey = &kContentAssociationKey;
const void* const kShadowHostAssociationKey = &kShadowHostAssociationKey;
const void* const kTargetAssociationKey = &kTargetAssociationKey;
const void* const kInputScrollAssociationKey = &kInputScrollAssociationKey;
const void* const kInputTextViewAssociationKey = &kInputTextViewAssociationKey;
const void* const kEmojiButtonAssociationKey = &kEmojiButtonAssociationKey;
const void* const kImageButtonAssociationKey = &kImageButtonAssociationKey;
const void* const kScreenshotButtonAssociationKey = &kScreenshotButtonAssociationKey;
const void* const kHistoryButtonAssociationKey = &kHistoryButtonAssociationKey;
const void* const kSendButtonAssociationKey = &kSendButtonAssociationKey;
const void* const kInactiveBorderLayerAssociationKey = &kInactiveBorderLayerAssociationKey;
const void* const kInactiveShadowLayerAssociationKey = &kInactiveShadowLayerAssociationKey;
const void* const kWindowObserverAssociationKey = &kWindowObserverAssociationKey;
const void* const kVisualOpacityAssociationKey = &kVisualOpacityAssociationKey;

NSAppearance* appThemeAppearance()
{
    return [NSAppearance appearanceNamed:ThemeManager::instance().isDark()
                                             ? NSAppearanceNameDarkAqua
                                             : NSAppearanceNameAqua];
}

NSColor* nsColorForTheme(ThemeColor role, CGFloat alpha = -1.0)
{
    QColor color = ThemeManager::instance().color(role);
    if (alpha >= 0.0) {
        color.setAlphaF(alpha);
    }
    return [NSColor colorWithCalibratedRed:color.redF()
                                     green:color.greenF()
                                      blue:color.blueF()
                                     alpha:color.alphaF()];
}

void applyAppThemeAppearance(NSView* view)
{
    if (view) {
        view.appearance = appThemeAppearance();
    }
}

struct AppearanceStyle {
    CGFloat shadowOpacity;
    CGFloat shadowRadius;
    CGFloat shadowYOffset;
    CGFloat shadowPaddingX;
    CGFloat shadowPaddingTop;
    CGFloat shadowPaddingBottom;
    CGFloat backgroundAlpha;
    CGFloat borderAlpha;
};

constexpr CGFloat kNativeBlurShadowOpacity = 0.18;
constexpr CGFloat kNativeBlurShadowRadius = 20.0;
constexpr CGFloat kNativeBlurShadowYOffset = 0.0;
constexpr CGFloat kNativeBlurShadowPaddingX = 18.0;
constexpr CGFloat kNativeBlurShadowPaddingTop = 14.0;
constexpr CGFloat kNativeBlurShadowPaddingBottom = 18.0;
constexpr CGFloat kNativeBlurBackgroundAlpha = 0.34;
constexpr CGFloat kNativeBlurBorderAlpha = 0.32;

constexpr CGFloat kLiquidGlassShadowOpacity = 0.04;
constexpr CGFloat kLiquidGlassShadowRadius = 20.0;
constexpr CGFloat kLiquidGlassShadowYOffset = 10.0;
constexpr CGFloat kLiquidGlassShadowPaddingX = 14.0;
constexpr CGFloat kLiquidGlassShadowPaddingTop = 10.0;
constexpr CGFloat kLiquidGlassShadowPaddingBottom = 18.0;
constexpr CGFloat kLiquidGlassBackgroundAlpha = 0.0;
constexpr CGFloat kLiquidGlassBorderAlpha = 0.0;

constexpr AppearanceStyle kNativeBlurStyle {
    kNativeBlurShadowOpacity,
    kNativeBlurShadowRadius,
    kNativeBlurShadowYOffset,
    kNativeBlurShadowPaddingX,
    kNativeBlurShadowPaddingTop,
    kNativeBlurShadowPaddingBottom,
    kNativeBlurBackgroundAlpha,
    kNativeBlurBorderAlpha
};

constexpr AppearanceStyle kLiquidGlassStyle {
    kLiquidGlassShadowOpacity,
    kLiquidGlassShadowRadius,
    kLiquidGlassShadowYOffset,
    kLiquidGlassShadowPaddingX,
    kLiquidGlassShadowPaddingTop,
    kLiquidGlassShadowPaddingBottom,
    kLiquidGlassBackgroundAlpha,
    kLiquidGlassBorderAlpha
};

enum class Appearance {
    Unsupported,
    NativeBlur,
    LiquidGlass
};

AppearanceStyle styleForAppearance(Appearance appearance)
{
    switch (appearance) {
    case Appearance::NativeBlur:
        return kNativeBlurStyle;
    case Appearance::LiquidGlass:
        return kLiquidGlassStyle;
    case Appearance::Unsupported:
    default:
        return kLiquidGlassStyle;
    }
}

constexpr int kLiquidGlassMode = 1;
constexpr int kNativeBlurMode = 2;
constexpr int kQtFallbackMode = 3;

int defaultConfiguredMode()
{
    switch (MacFloatingInputBarBridge::kFloatingInputBarTestMode) {
    case kLiquidGlassMode:
    case kNativeBlurMode:
    case kQtFallbackMode:
        return MacFloatingInputBarBridge::kFloatingInputBarTestMode;
    default:
        return kNativeBlurMode;
    }
}

int& configuredModeStorage()
{
    static int mode = defaultConfiguredMode();
    return mode;
}

int normalizedTestMode()
{
    switch (configuredModeStorage()) {
    case kLiquidGlassMode:
    case kNativeBlurMode:
    case kQtFallbackMode:
        return configuredModeStorage();
    default:
        return kNativeBlurMode;
    }
}

bool supportsNativeBlur()
{
    return NSClassFromString(@"NSVisualEffectView") != nil
            && NSClassFromString(@"NSTextView") != nil;
}

bool supportsLiquidGlass()
{
    if (!supportsNativeBlur()) {
        return false;
    }

#if NETHERLINK_HAS_NS_GLASS_EFFECT_VIEW
    if (@available(macOS 26.0, *)) {
        return NSClassFromString(@"NSGlassEffectView") != nil;
    }
#endif
    return false;
}

Appearance resolvedAppearanceForMode(int mode)
{
    if (mode == kLiquidGlassMode && supportsLiquidGlass()) {
        return Appearance::LiquidGlass;
    }

    if (mode == kLiquidGlassMode || mode == kNativeBlurMode) {
        if (supportsNativeBlur()) {
            return Appearance::NativeBlur;
        }
    }

    switch (mode) {
    case kLiquidGlassMode:
    case kNativeBlurMode:
    case kQtFallbackMode:
    default:
        return Appearance::Unsupported;
    }
}

Appearance currentAppearance()
{
    return resolvedAppearanceForMode(normalizedTestMode());
}

MacFloatingInputBarBridge::Mode effectiveModeForMode(int mode)
{
    if (mode == kLiquidGlassMode && supportsLiquidGlass()) {
        return MacFloatingInputBarBridge::Mode::LiquidGlass;
    }

    if ((mode == kLiquidGlassMode || mode == kNativeBlurMode) && supportsNativeBlur()) {
        return MacFloatingInputBarBridge::Mode::NativeBlur;
    }

    return MacFloatingInputBarBridge::Mode::QtFallback;
}

NSView* topLevelQtViewForWidget(QWidget* widget, bool createIfNeeded)
{
    if (!widget) {
        return nil;
    }

    QWidget* topLevelWidget = widget->window();
    if (!topLevelWidget) {
        return nil;
    }

    if (!createIfNeeded && !topLevelWidget->testAttribute(Qt::WA_WState_Created)) {
        return nil;
    }

    const WId wid = createIfNeeded ? topLevelWidget->winId() : topLevelWidget->effectiveWinId();
    return wid ? reinterpret_cast<NSView*>(wid) : nil;
}

NSView* containerForView(NSView* qtView)
{
    return qtView
            ? static_cast<NSView*>(objc_getAssociatedObject(qtView, kContainerAssociationKey))
            : nil;
}

NSView* contentForView(NSView* qtView)
{
    return qtView
            ? static_cast<NSView*>(objc_getAssociatedObject(qtView, kContentAssociationKey))
            : nil;
}

NSView* shadowHostForView(NSView* qtView)
{
    return qtView
            ? static_cast<NSView*>(objc_getAssociatedObject(qtView, kShadowHostAssociationKey))
            : nil;
}

NLFloatingInputBarTarget* targetForView(NSView* qtView)
{
    return qtView
            ? static_cast<NLFloatingInputBarTarget*>(objc_getAssociatedObject(qtView, kTargetAssociationKey))
            : nil;
}

NLFloatingInputIconButton* imageButtonForView(NSView* qtView)
{
    return qtView
            ? static_cast<NLFloatingInputIconButton*>(objc_getAssociatedObject(qtView, kImageButtonAssociationKey))
            : nil;
}

NLFloatingInputIconButton* emojiButtonForView(NSView* qtView)
{
    return qtView
            ? static_cast<NLFloatingInputIconButton*>(objc_getAssociatedObject(qtView, kEmojiButtonAssociationKey))
            : nil;
}

NLFloatingInputIconButton* screenshotButtonForView(NSView* qtView)
{
    return qtView
            ? static_cast<NLFloatingInputIconButton*>(objc_getAssociatedObject(qtView, kScreenshotButtonAssociationKey))
            : nil;
}

NLFloatingInputIconButton* historyButtonForView(NSView* qtView)
{
    return qtView
            ? static_cast<NLFloatingInputIconButton*>(objc_getAssociatedObject(qtView, kHistoryButtonAssociationKey))
            : nil;
}

NLFloatingInputIconButton* sendButtonForView(NSView* qtView)
{
    return qtView
            ? static_cast<NLFloatingInputIconButton*>(objc_getAssociatedObject(qtView, kSendButtonAssociationKey))
            : nil;
}

CAShapeLayer* inactiveBorderLayerForView(NSView* qtView)
{
    return qtView
            ? static_cast<CAShapeLayer*>(objc_getAssociatedObject(qtView, kInactiveBorderLayerAssociationKey))
            : nil;
}

CAShapeLayer* inactiveShadowLayerForView(NSView* qtView)
{
    return qtView
            ? static_cast<CAShapeLayer*>(objc_getAssociatedObject(qtView, kInactiveShadowLayerAssociationKey))
            : nil;
}

NLFloatingInputWindowObserver* windowObserverForView(NSView* qtView)
{
    return qtView
            ? static_cast<NLFloatingInputWindowObserver*>(objc_getAssociatedObject(qtView, kWindowObserverAssociationKey))
            : nil;
}

CGFloat storedOpacityForView(NSView* qtView)
{
    NSNumber* value = qtView
            ? static_cast<NSNumber*>(objc_getAssociatedObject(qtView, kVisualOpacityAssociationKey))
            : nil;
    return value ? static_cast<CGFloat>(value.doubleValue) : 1.0;
}

NSScrollView* inputScrollForView(NSView* qtView)
{
    return qtView
            ? static_cast<NSScrollView*>(objc_getAssociatedObject(qtView, kInputScrollAssociationKey))
            : nil;
}

NLFloatingInputShortcutTextView* inputTextViewForView(NSView* qtView)
{
    return qtView
            ? static_cast<NLFloatingInputShortcutTextView*>(objc_getAssociatedObject(qtView, kInputTextViewAssociationKey))
            : nil;
}

void resignInputFirstResponder(NLFloatingInputShortcutTextView* inputField)
{
    if (!inputField || !inputField.window) {
        return;
    }

    NSResponder* firstResponder = inputField.window.firstResponder;
    if (firstResponder == inputField) {
        [inputField.window makeFirstResponder:nil];
    }
}

void prepareButtonForRemoval(NLFloatingInputIconButton* button)
{
    if (!button) {
        return;
    }

    button.target = nil;
    button.action = nil;
    button.hovering = NO;
    [button refreshImage];

    if (button.hoverTrackingArea) {
        if ([button.trackingAreas containsObject:button.hoverTrackingArea]) {
            [button removeTrackingArea:button.hoverTrackingArea];
        }
        button.hoverTrackingArea = nil;
    }
}

void configureHostView(NSView* qtView)
{
    if (!qtView) {
        return;
    }

    qtView.wantsLayer = YES;
    qtView.layer.backgroundColor = NSColor.clearColor.CGColor;
}

void storeOpacityForView(NSView* qtView, CGFloat opacity)
{
    if (!qtView) {
        return;
    }

    objc_setAssociatedObject(qtView,
                             kVisualOpacityAssociationKey,
                             [NSNumber numberWithDouble:opacity],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

NSImage* iconImageFromResource(const QString& resourcePath, CGFloat iconSize)
{
    QPixmap pixmap(resourcePath);
    if (pixmap.isNull()) {
        return nil;
    }

    const int targetSize = qMax(1, static_cast<int>(qRound(iconSize)));
    pixmap = pixmap.scaled(targetSize,
                           targetSize,
                           Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !pixmap.save(&buffer, "PNG")) {
        return nil;
    }

    NSData* data = [NSData dataWithBytes:bytes.constData()
                                  length:static_cast<NSUInteger>(bytes.size())];
    NSImage* image = [[NSImage alloc] initWithData:data];
    image.size = NSMakeSize(iconSize, iconSize);
    return [image autorelease];
}

NSRect hostFrameForWidget(QWidget* widget, NSView* hostView)
{
    if (!widget || !hostView) {
        return NSZeroRect;
    }

    QWidget* topLevelWidget = widget->window();
    if (!topLevelWidget) {
        return NSZeroRect;
    }

    const QPoint topLeft = widget->mapTo(topLevelWidget, QPoint(0, 0));
    const QRect widgetRect(topLeft, widget->size());
    if (widgetRect.isEmpty()) {
        return NSZeroRect;
    }

    const CGFloat x = static_cast<CGFloat>(widgetRect.x());
    const CGFloat width = static_cast<CGFloat>(widgetRect.width());
    const CGFloat height = static_cast<CGFloat>(widgetRect.height());
    const CGFloat y = [hostView isFlipped]
            ? static_cast<CGFloat>(widgetRect.y())
            : NSHeight(hostView.bounds) - static_cast<CGFloat>(widgetRect.y() + widgetRect.height());
    return NSMakeRect(x, y, width, height);
}

void applyCommonContainerShadow(NSView* container, const AppearanceStyle& style)
{
    if (!container) {
        return;
    }

    container.wantsLayer = YES;
    container.layer.shadowColor = NSColor.blackColor.CGColor;
    container.layer.shadowOpacity = style.shadowOpacity;
    container.layer.shadowRadius = style.shadowRadius;
    container.layer.shadowOffset = CGSizeMake(0.0, style.shadowYOffset);
}

CAShapeLayer* ensureInactiveBorderLayer(NSView* hostView, NSView* container)
{
    if (!hostView || !container || !container.layer) {
        return nil;
    }

    CAShapeLayer* layer = inactiveBorderLayerForView(hostView);
    if (!layer) {
        layer = [CAShapeLayer layer];
        objc_setAssociatedObject(hostView,
                                 kInactiveBorderLayerAssociationKey,
                                 layer,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }

    if (layer.superlayer != container.layer) {
        [layer removeFromSuperlayer];
        [container.layer addSublayer:layer];
    }

    layer.fillColor = NSColor.clearColor.CGColor;
    layer.lineWidth = kInactiveGlassBorderWidth;
    layer.strokeColor = chatInputBorderColor(kInactiveGlassBorderAlpha).CGColor;
    return layer;
}

CAShapeLayer* ensureInactiveShadowLayer(NSView* hostView, NSView* shadowHost)
{
    if (!hostView || !shadowHost || !shadowHost.layer) {
        return nil;
    }

    CAShapeLayer* layer = inactiveShadowLayerForView(hostView);
    if (!layer) {
        layer = [CAShapeLayer layer];
        objc_setAssociatedObject(hostView,
                                 kInactiveShadowLayerAssociationKey,
                                 layer,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }

    if (layer.superlayer != shadowHost.layer) {
        [layer removeFromSuperlayer];
        [shadowHost.layer addSublayer:layer];
    }

    layer.fillColor = NSColor.clearColor.CGColor;
    layer.strokeColor = nil;
    layer.shadowColor = NSColor.blackColor.CGColor;
    layer.shadowOpacity = kInactiveGlassShadowOpacity;
    layer.shadowRadius = kInactiveGlassShadowRadius;
    layer.shadowOffset = CGSizeMake(0.0, kInactiveGlassShadowYOffset);
    return layer;
}

void ensureWindowObserver(NSView* hostView)
{
    if (!hostView) {
        return;
    }

    NLFloatingInputWindowObserver* observer = windowObserverForView(hostView);
    if (!observer) {
        observer = [[NLFloatingInputWindowObserver alloc] init];
        objc_setAssociatedObject(hostView,
                                 kWindowObserverAssociationKey,
                                 observer,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [observer release];
        observer = windowObserverForView(hostView);
    }

    [observer bindWindow:hostView.window hostView:hostView];
}

void clearInactiveGlassChrome(NSView* hostView)
{
    if (!hostView) {
        return;
    }

    CAShapeLayer* borderLayer = inactiveBorderLayerForView(hostView);
    if (borderLayer) {
        [borderLayer removeFromSuperlayer];
    }

    CAShapeLayer* shadowLayer = inactiveShadowLayerForView(hostView);
    if (shadowLayer) {
        [shadowLayer removeFromSuperlayer];
    }

    NLFloatingInputWindowObserver* observer = windowObserverForView(hostView);
    if (observer) {
        [observer bindWindow:nil hostView:nil];
    }

    objc_setAssociatedObject(hostView, kInactiveBorderLayerAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(hostView, kInactiveShadowLayerAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(hostView, kWindowObserverAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
}

void updateInactiveGlassChrome(NSView* hostView)
{
    if (!hostView) {
        return;
    }

    NSView* container = containerForView(hostView);
    NSView* shadowHost = shadowHostForView(hostView);
    NSWindow* window = hostView.window;
    bool isLiquidGlass = false;
#if NETHERLINK_HAS_NS_GLASS_EFFECT_VIEW
    isLiquidGlass = container && [container isKindOfClass:[NSGlassEffectView class]];
#endif
    if (!isLiquidGlass || !shadowHost) {
        clearInactiveGlassChrome(hostView);
        return;
    }

    ensureWindowObserver(hostView);

    CAShapeLayer* borderLayer = ensureInactiveBorderLayer(hostView, container);
    CAShapeLayer* shadowLayer = ensureInactiveShadowLayer(hostView, shadowHost);
    if (!borderLayer || !shadowLayer) {
        return;
    }

    const BOOL hidden = container.hidden || shadowHost.hidden || !window;
    const CGFloat alpha = qBound(0.0,
                                 static_cast<double>(storedOpacityForView(hostView)),
                                 1.0);
    const BOOL showBorderChrome = !hidden && alpha > 0.001;
    const BOOL showInactiveShadow = showBorderChrome && !window.isKeyWindow;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    borderLayer.hidden = !showBorderChrome;
    shadowLayer.hidden = !showInactiveShadow;
    borderLayer.opacity = alpha;
    shadowLayer.opacity = alpha;

    if (showBorderChrome) {
        borderLayer.frame = container.bounds;
        const CGRect borderRect = CGRectInset(container.bounds,
                                              kInactiveGlassBorderWidth / 2.0,
                                              kInactiveGlassBorderWidth / 2.0);
        CGPathRef borderPath = CGPathCreateWithRoundedRect(borderRect,
                                                           kCornerRadius - kInactiveGlassBorderWidth / 2.0,
                                                           kCornerRadius - kInactiveGlassBorderWidth / 2.0,
                                                           nil);
        borderLayer.path = borderPath;
        CGPathRelease(borderPath);
    }

    if (showInactiveShadow) {
        shadowLayer.frame = shadowHost.bounds;
        const CGRect shadowRect = [shadowHost convertRect:container.frame fromView:hostView];
        CGPathRef shadowPath = CGPathCreateWithRoundedRect(shadowRect,
                                                           kCornerRadius,
                                                           kCornerRadius,
                                                           nil);
        shadowLayer.path = shadowPath;
        shadowLayer.shadowPath = shadowPath;
        CGPathRelease(shadowPath);
    }

    [CATransaction commit];
}

NSView* ensureShadowHost(NSView* hostView)
{
    if (!hostView) {
        return nil;
    }

    NSView* shadowHost = shadowHostForView(hostView);
    if (!shadowHost) {
        shadowHost = [[NSView alloc] initWithFrame:NSZeroRect];
        shadowHost.wantsLayer = YES;
        shadowHost.layer.backgroundColor = NSColor.clearColor.CGColor;
        shadowHost.layer.masksToBounds = NO;
        objc_setAssociatedObject(hostView,
                                 kShadowHostAssociationKey,
                                 shadowHost,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [hostView addSubview:shadowHost positioned:NSWindowAbove relativeTo:nil];
        [shadowHost release];
        shadowHost = shadowHostForView(hostView);
    } else if (shadowHost.superview != hostView) {
        [shadowHost removeFromSuperview];
        [hostView addSubview:shadowHost positioned:NSWindowAbove relativeTo:nil];
    }

    shadowHost.wantsLayer = YES;
    shadowHost.layer.backgroundColor = NSColor.clearColor.CGColor;
    shadowHost.layer.masksToBounds = NO;
    return shadowHost;
}

NSView* ensureGlassContentView(NSView* hostView, NSView* container)
{
    if (!hostView || !container) {
        return nil;
    }

    NSView* contentView = contentForView(hostView);
    NSGlassEffectView* glassContainer = [container isKindOfClass:[NSGlassEffectView class]]
            ? static_cast<NSGlassEffectView*>(container)
            : nil;
    if (!glassContainer) {
        return nil;
    }

    if (!contentView || contentView == container || glassContainer.contentView != contentView) {
        if (contentView && contentView != container && contentView != glassContainer.contentView) {
            [contentView removeFromSuperview];
        }

        contentView = [[NSView alloc] initWithFrame:container.bounds];
        contentView.wantsLayer = YES;
        contentView.layer.backgroundColor = NSColor.clearColor.CGColor;
        applyAppThemeAppearance(contentView);
        glassContainer.contentView = contentView;
        objc_setAssociatedObject(hostView,
                                 kContentAssociationKey,
                                 contentView,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [contentView release];
        contentView = contentForView(hostView);
    } else {
        contentView.frame = container.bounds;
        applyAppThemeAppearance(contentView);
    }

    return contentView;
}

NSView* ensureVisualEffectContainer(NSView* hostView, const AppearanceStyle& style)
{
    if (!hostView) {
        return nil;
    }

    NSView* shadowHost = ensureShadowHost(hostView);
    if (!shadowHost) {
        return nil;
    }

    NSVisualEffectView* container = static_cast<NSVisualEffectView*>(containerForView(hostView));
    if (!container || ![container isKindOfClass:[NSVisualEffectView class]]) {
        if (container) {
            [container removeFromSuperview];
        }

        container = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
        objc_setAssociatedObject(hostView,
                                 kContainerAssociationKey,
                                 container,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [hostView addSubview:container positioned:NSWindowAbove relativeTo:shadowHost];
        objc_setAssociatedObject(hostView,
                                 kContentAssociationKey,
                                 container,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [container release];
        container = static_cast<NSVisualEffectView*>(containerForView(hostView));
    } else if (container.superview != hostView) {
        [container removeFromSuperview];
        [hostView addSubview:container positioned:NSWindowAbove relativeTo:shadowHost];
    }

    container.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    applyAppThemeAppearance(container);
    container.material = NSVisualEffectMaterialPopover;
    container.state = NSVisualEffectStateActive;
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    container.layer.backgroundColor = nsColorForTheme(ThemeColor::PanelBackground,
                                                      style.backgroundAlpha).CGColor;
    container.layer.borderWidth = 1.0;
    container.layer.borderColor = chatInputBorderColor(style.borderAlpha).CGColor;
    applyCommonContainerShadow(shadowHost, style);
    return container;
}

#if NETHERLINK_HAS_NS_GLASS_EFFECT_VIEW
NSView* ensureGlassContainer(NSView* hostView, const AppearanceStyle& style)
{
    if (!hostView) {
        return nil;
    }

    if (@available(macOS 26.0, *)) {
        NSView* shadowHost = ensureShadowHost(hostView);
        if (!shadowHost) {
            return nil;
        }

        NSGlassEffectView* container = static_cast<NSGlassEffectView*>(containerForView(hostView));
        if (!container || ![container isKindOfClass:[NSGlassEffectView class]]) {
            if (container) {
                [container removeFromSuperview];
            }

            container = [[NSGlassEffectView alloc] initWithFrame:NSZeroRect];
            objc_setAssociatedObject(hostView,
                                     kContainerAssociationKey,
                                     container,
                                     OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            [hostView addSubview:container positioned:NSWindowAbove relativeTo:shadowHost];
            [container release];
            container = static_cast<NSGlassEffectView*>(containerForView(hostView));
        } else if (container.superview != hostView) {
            [container removeFromSuperview];
            [hostView addSubview:container positioned:NSWindowAbove relativeTo:shadowHost];
        }

        applyAppThemeAppearance(container);
        container.style = NSGlassEffectViewStyleRegular;
        container.cornerRadius = kCornerRadius;
        container.tintColor = nil;
        applyCommonContainerShadow(shadowHost, style);

        NSView* contentView = ensureGlassContentView(hostView, container);
        objc_setAssociatedObject(hostView,
                                 kContentAssociationKey,
                                 contentView,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return container;
    }

    return nil;
}
#endif

NSView* ensureContainer(NSView* hostView, Appearance appearance)
{
    const AppearanceStyle style = styleForAppearance(appearance);
    switch (appearance) {
    case Appearance::LiquidGlass:
#if NETHERLINK_HAS_NS_GLASS_EFFECT_VIEW
        return ensureGlassContainer(hostView, style);
#else
        return nil;
#endif
    case Appearance::NativeBlur:
        return ensureVisualEffectContainer(hostView, style);
    case Appearance::Unsupported:
    default:
        return nil;
    }
}

NLFloatingInputBarTarget* ensureTarget(NSView* qtView, FloatingInputBar* widget)
{
    if (!qtView) {
        return nil;
    }

    NLFloatingInputBarTarget* target = targetForView(qtView);
    if (!target) {
        target = [[NLFloatingInputBarTarget alloc] initWithOwner:widget hostView:qtView];
        objc_setAssociatedObject(qtView,
                                 kTargetAssociationKey,
                                 target,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [target release];
        target = targetForView(qtView);
    } else {
        target.owner = widget;
        target.hostView = qtView;
    }

    return target;
}

void clearButtons(NSView* qtView)
{
    if (!qtView) {
        return;
    }

    hideNativeTooltip();

    NLFloatingInputIconButton* emojiButton = emojiButtonForView(qtView);
    if (emojiButton) {
        prepareButtonForRemoval(emojiButton);
        [emojiButton removeFromSuperview];
    }

    NLFloatingInputIconButton* imageButton = imageButtonForView(qtView);
    if (imageButton) {
        prepareButtonForRemoval(imageButton);
        [imageButton removeFromSuperview];
    }

    NLFloatingInputIconButton* screenshotButton = screenshotButtonForView(qtView);
    if (screenshotButton) {
        prepareButtonForRemoval(screenshotButton);
        [screenshotButton removeFromSuperview];
    }

    NLFloatingInputIconButton* historyButton = historyButtonForView(qtView);
    if (historyButton) {
        prepareButtonForRemoval(historyButton);
        [historyButton removeFromSuperview];
    }

    NLFloatingInputIconButton* sendButton = sendButtonForView(qtView);
    if (sendButton) {
        prepareButtonForRemoval(sendButton);
        [sendButton removeFromSuperview];
    }

    objc_setAssociatedObject(qtView, kEmojiButtonAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(qtView, kImageButtonAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(qtView, kScreenshotButtonAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(qtView, kHistoryButtonAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(qtView, kSendButtonAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
}

void clearInputField(NSView* qtView)
{
    if (!qtView) {
        return;
    }

    NLFloatingInputShortcutTextView* inputField = inputTextViewForView(qtView);
    resignInputFirstResponder(inputField);
    if (inputField) {
        inputField.shortcutTarget = nil;
    }

    NSScrollView* inputScroll = inputScrollForView(qtView);
    if (inputScroll) {
        inputScroll.documentView = nil;
        [inputScroll removeFromSuperview];
    }

    objc_setAssociatedObject(qtView, kInputScrollAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(qtView, kInputTextViewAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
}

void configureIconButton(NLFloatingInputIconButton* button,
                         NSImage* normalImage,
                         NSImage* hoveredImage,
                         NSString* toolTip,
                         id target,
                         SEL action)
{
    if (!button) {
        return;
    }

    button.bordered = NO;
    button.buttonType = NSButtonTypeMomentaryChange;
    button.bezelStyle = NSBezelStyleRegularSquare;
    applyAppThemeAppearance(button);
    button.focusRingType = NSFocusRingTypeNone;
    button.imagePosition = NSImageOnly;
    button.imageScaling = NSImageScaleNone;
    button.translatesAutoresizingMaskIntoConstraints = YES;
    button.toolTip = nil;
    button.hoverTipText = toolTip;
    button.target = target;
    button.action = action;
    button.normalImage = normalImage;
    button.hoveredImage = hoveredImage;
    button.hovering = NO;
    [button refreshImage];
}

void ensureButtons(NSView* qtView, NSView* contentView, FloatingInputBar* widget)
{
    if (!qtView || !contentView || !widget) {
        return;
    }

    NLFloatingInputBarTarget* target = ensureTarget(qtView, widget);
    if (!target) {
        return;
    }

    NLFloatingInputIconButton* emojiButton = emojiButtonForView(qtView);
    if (!emojiButton) {
        emojiButton = [[NLFloatingInputIconButton alloc] initWithFrame:NSZeroRect];
        configureIconButton(emojiButton,
                            iconImageFromResource(QStringLiteral(":/resources/icon/skull.png"), kToolbarIconSize),
                            iconImageFromResource(QStringLiteral(":/resources/icon/hovered_skull.png"), kToolbarIconSize),
                            @"表情",
                            nil,
                            nil);
        objc_setAssociatedObject(qtView,
                                 kEmojiButtonAssociationKey,
                                 emojiButton,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [contentView addSubview:emojiButton];
        [emojiButton release];
    } else if (emojiButton.superview != contentView) {
        [emojiButton removeFromSuperview];
        [contentView addSubview:emojiButton];
    }

    NLFloatingInputIconButton* imageButton = imageButtonForView(qtView);
    if (!imageButton) {
        imageButton = [[NLFloatingInputIconButton alloc] initWithFrame:NSZeroRect];
        configureIconButton(imageButton,
                            iconImageFromResource(QStringLiteral(":/resources/icon/painting.png"), kToolbarIconSize),
                            iconImageFromResource(QStringLiteral(":/resources/icon/hovered_painting.png"), kToolbarIconSize),
                            @"图片",
                            target,
                            @selector(handleImageButtonClick:));
        objc_setAssociatedObject(qtView,
                                 kImageButtonAssociationKey,
                                 imageButton,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [contentView addSubview:imageButton];
        [imageButton release];
    } else if (imageButton.superview != contentView) {
        [imageButton removeFromSuperview];
        [contentView addSubview:imageButton];
    }

    NLFloatingInputIconButton* screenshotButton = screenshotButtonForView(qtView);
    if (!screenshotButton) {
        screenshotButton = [[NLFloatingInputIconButton alloc] initWithFrame:NSZeroRect];
        configureIconButton(screenshotButton,
                            iconImageFromResource(QStringLiteral(":/resources/icon/shears.png"), kToolbarIconSize),
                            iconImageFromResource(QStringLiteral(":/resources/icon/hovered_shears.png"), kToolbarIconSize),
                            @"截图",
                            nil,
                            nil);
        objc_setAssociatedObject(qtView,
                                 kScreenshotButtonAssociationKey,
                                 screenshotButton,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [contentView addSubview:screenshotButton];
        [screenshotButton release];
    } else if (screenshotButton.superview != contentView) {
        [screenshotButton removeFromSuperview];
        [contentView addSubview:screenshotButton];
    }

    NLFloatingInputIconButton* historyButton = historyButtonForView(qtView);
    if (!historyButton) {
        historyButton = [[NLFloatingInputIconButton alloc] initWithFrame:NSZeroRect];
        configureIconButton(historyButton,
                            iconImageFromResource(QStringLiteral(":/resources/icon/clock.png"), kToolbarIconSize),
                            iconImageFromResource(QStringLiteral(":/resources/icon/hovered_clock.png"), kToolbarIconSize),
                            @"历史",
                            nil,
                            nil);
        objc_setAssociatedObject(qtView,
                                 kHistoryButtonAssociationKey,
                                 historyButton,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [contentView addSubview:historyButton];
        [historyButton release];
    } else if (historyButton.superview != contentView) {
        [historyButton removeFromSuperview];
        [contentView addSubview:historyButton];
    }

    NLFloatingInputIconButton* sendButton = sendButtonForView(qtView);
    if (!sendButton) {
        sendButton = [[NLFloatingInputIconButton alloc] initWithFrame:NSZeroRect];
        configureIconButton(sendButton,
                            iconImageFromResource(QStringLiteral(":/resources/icon/book_and_pen.png"), kSendToolbarIconSize),
                            iconImageFromResource(QStringLiteral(":/resources/icon/hovered_book_and_pen.png"), kSendToolbarIconSize),
                            @"发送",
                            target,
                            @selector(handleSendButtonClick:));
        objc_setAssociatedObject(qtView,
                                 kSendButtonAssociationKey,
                                 sendButton,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [contentView addSubview:sendButton];
        [sendButton release];
    } else if (sendButton.superview != contentView) {
        [sendButton removeFromSuperview];
        [contentView addSubview:sendButton];
    }
}

NLFloatingInputShortcutTextView* ensureInputField(NSView* qtView,
                                                  NSView* contentView,
                                                  FloatingInputBar* widget,
                                                  const QString& placeholderText)
{
    Q_UNUSED(placeholderText);
    if (!qtView || !contentView) {
        return nil;
    }

    NSScrollView* inputScroll = inputScrollForView(qtView);
    NLFloatingInputShortcutTextView* inputField = inputTextViewForView(qtView);
    if (!inputScroll || !inputField) {
        inputField = [[NLFloatingInputShortcutTextView alloc] initWithFrame:NSZeroRect];
        inputField.richText = NO;
        inputField.importsGraphics = NO;
        inputField.allowsUndo = NO;
        inputField.automaticQuoteSubstitutionEnabled = NO;
        inputField.automaticDashSubstitutionEnabled = NO;
        inputField.automaticTextReplacementEnabled = NO;
        inputField.automaticSpellingCorrectionEnabled = NO;
        inputField.continuousSpellCheckingEnabled = NO;
        inputField.drawsBackground = NO;
        inputField.editable = YES;
        inputField.selectable = YES;
        inputField.font = [NSFont systemFontOfSize:14.0 weight:NSFontWeightRegular];
        inputField.horizontallyResizable = NO;
        inputField.verticallyResizable = YES;
        inputField.textContainer.widthTracksTextView = YES;
        inputField.textContainer.containerSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
        inputField.textContainerInset = NSMakeSize(0.0, 0.0);

        inputScroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
        inputScroll.borderType = NSNoBorder;
        inputScroll.drawsBackground = NO;
        inputScroll.hasVerticalScroller = YES;
        inputScroll.hasHorizontalScroller = NO;
        inputScroll.autohidesScrollers = YES;
        inputScroll.documentView = inputField;

        objc_setAssociatedObject(qtView,
                                 kInputScrollAssociationKey,
                                 inputScroll,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        objc_setAssociatedObject(qtView,
                                 kInputTextViewAssociationKey,
                                 inputField,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [contentView addSubview:inputScroll];
        [inputScroll release];
        [inputField release];
        inputScroll = inputScrollForView(qtView);
        inputField = inputTextViewForView(qtView);
    } else if (inputScroll.superview != contentView) {
        [inputScroll removeFromSuperview];
        [contentView addSubview:inputScroll];
    }

    NLFloatingInputBarTarget* target = ensureTarget(qtView, widget);
    applyAppThemeAppearance(inputScroll);
    applyAppThemeAppearance(inputField);
    inputField.textColor = nsColorForTheme(ThemeColor::PrimaryText, 0.95);
    inputField.insertionPointColor = nsColorForTheme(ThemeColor::Accent);
    inputField.shortcutTarget = target;
    return inputField;
}

void layoutInputField(NSScrollView* inputScroll,
                      NLFloatingInputShortcutTextView* inputField,
                      NSView* contentView)
{
    if (!inputScroll || !inputField || !contentView) {
        return;
    }

    const NSRect bounds = contentView.bounds;
    const CGFloat width = MAX(0.0,
                              NSWidth(bounds) - kInputHorizontalMargin - kInputRightInset);
    const CGFloat height = MAX(0.0, NSHeight(bounds) - kInputTopReserved - kInputBottomReserved);
    inputScroll.frame = NSMakeRect(kInputHorizontalMargin,
                                   kInputBottomReserved,
                                   width,
                                   height);
    inputField.frame = NSMakeRect(0.0, 0.0, width, height);
}

void layoutButtons(NSView* contentView,
                   NLFloatingInputIconButton* emojiButton,
                   NLFloatingInputIconButton* imageButton,
                   NLFloatingInputIconButton* screenshotButton,
                   NLFloatingInputIconButton* historyButton,
                   NLFloatingInputIconButton* sendButton)
{
    if (!contentView) {
        return;
    }

    const NSRect bounds = contentView.bounds;
    const CGFloat topButtonY = MAX(kToolbarTopInset,
                                   NSHeight(bounds) - kToolbarTopInset - kToolbarButtonSize);
    const CGFloat leftX = kToolbarSideInset;
    const CGFloat middleX = leftX + kToolbarButtonSize + kToolbarButtonSpacing;
    const CGFloat rightOfMiddleX = middleX + kToolbarButtonSize + kToolbarButtonSpacing;
    const CGFloat topRightX = MAX(kToolbarSideInset,
                                  NSWidth(bounds) - kToolbarSideInset - kToolbarButtonSize);
    const CGFloat bottomRightX = topRightX;
    const CGFloat bottomRightY = kToolbarBottomInset;

    if (emojiButton) {
        emojiButton.frame = NSMakeRect(leftX,
                                       topButtonY,
                                       kToolbarButtonSize,
                                       kToolbarButtonSize);
    }

    if (imageButton) {
        imageButton.frame = NSMakeRect(middleX,
                                       topButtonY,
                                       kToolbarButtonSize,
                                       kToolbarButtonSize);
    }

    if (screenshotButton) {
        screenshotButton.frame = NSMakeRect(rightOfMiddleX,
                                            topButtonY,
                                            kToolbarButtonSize,
                                            kToolbarButtonSize);
    }

    if (historyButton) {
        historyButton.frame = NSMakeRect(topRightX,
                                         topButtonY,
                                         kToolbarButtonSize,
                                         kToolbarButtonSize);
    }

    if (sendButton) {
        sendButton.frame = NSMakeRect(bottomRightX,
                                      bottomRightY,
                                      kToolbarButtonSize,
                                      kToolbarButtonSize);
    }
}

} // namespace

namespace MacFloatingInputBarBridge {

Mode mode()
{
    return effectiveModeForMode(normalizedTestMode());
}

void setMode(Mode mode)
{
    switch (mode) {
    case Mode::LiquidGlass:
    case Mode::NativeBlur:
    case Mode::QtFallback:
        configuredModeStorage() = static_cast<int>(mode);
        break;
    }
}

QVector<Mode> supportedModes()
{
    QVector<Mode> modes;
    if (supportsLiquidGlass()) {
        modes.push_back(Mode::LiquidGlass);
    }
    if (supportsNativeBlur()) {
        modes.push_back(Mode::NativeBlur);
    }
    modes.push_back(Mode::QtFallback);
    return modes;
}

Appearance appearance()
{
    switch (currentAppearance()) {
    case ::Appearance::NativeBlur:
        return Appearance::NativeBlur;
    case ::Appearance::LiquidGlass:
        return Appearance::LiquidGlass;
    case ::Appearance::Unsupported:
    default:
        return Appearance::Unsupported;
    }
}

bool isSupported()
{
    return appearance() != Appearance::Unsupported;
}

void syncGlass(QWidget* widget, double opacity)
{
    const auto current = currentAppearance();
    NSLog(@"[NetherLink][FloatingInputBarBridge] syncGlass widget=%p appearance=%d visible=%d opacity=%.3f",
          widget,
          static_cast<int>(current),
          widget ? widget->isVisible() : false,
          opacity);
    if (!widget || current == ::Appearance::Unsupported || !widget->isVisible()) {
        clearInputBar(widget);
        return;
    }

    NSView* hostView = topLevelQtViewForWidget(widget, true);
    if (!hostView) {
        return;
    }

    const AppearanceStyle style = styleForAppearance(current);
    configureHostView(hostView);
    NSView* container = ensureContainer(hostView, current);
    NSView* contentView = contentForView(hostView);
    NSView* shadowHost = shadowHostForView(hostView);
    if (!container || !contentView || !shadowHost) {
        return;
    }

    clearInputField(hostView);
    clearButtons(hostView);

    const NSRect frame = hostFrameForWidget(widget, hostView);
    const NSRect shadowFrame = NSMakeRect(frame.origin.x - style.shadowPaddingX,
                                          frame.origin.y - style.shadowPaddingTop,
                                          frame.size.width + style.shadowPaddingX * 2.0,
                                          frame.size.height + style.shadowPaddingTop + style.shadowPaddingBottom);
    shadowHost.frame = shadowFrame;
    shadowHost.hidden = NO;
    const NSRect containerFrame = NSMakeRect(style.shadowPaddingX,
                                             style.shadowPaddingTop,
                                             frame.size.width,
                                             frame.size.height);
    CGPathRef shadowPath = CGPathCreateWithRoundedRect(containerFrame,
                                                       kCornerRadius,
                                                       kCornerRadius,
                                                       nil);
    shadowHost.layer.shadowPath = shadowPath;
    CGPathRelease(shadowPath);
    container.frame = frame;
    const BOOL hidden = widget->isHidden() || widget->width() <= 0 || widget->height() <= 0;
    const CGFloat alpha = static_cast<CGFloat>(qBound(0.0, opacity, 1.0));
    storeOpacityForView(hostView, alpha);
    shadowHost.alphaValue = alpha;
    container.alphaValue = alpha;
    shadowHost.hidden = hidden;
    container.hidden = hidden;
    if (contentView != container) {
        contentView.frame = container.bounds;
    }
    updateInactiveGlassChrome(hostView);
}

void syncInputBar(QWidget* widget,
                  const QString& placeholderText,
                  double opacity)
{
    const auto current = currentAppearance();
    NSLog(@"[NetherLink][FloatingInputBarBridge] syncInputBar widget=%p appearance=%d visible=%d opacity=%.3f",
          widget,
          static_cast<int>(current),
          widget ? widget->isVisible() : false,
          opacity);
    if (!widget || current == ::Appearance::Unsupported || !widget->isVisible()) {
        clearInputBar(widget);
        return;
    }

    NSView* hostView = topLevelQtViewForWidget(widget, true);
    if (!hostView) {
        return;
    }

    const AppearanceStyle style = styleForAppearance(current);
    configureHostView(hostView);
    NSView* container = ensureContainer(hostView, current);
    NSView* contentView = contentForView(hostView);
    NSView* shadowHost = shadowHostForView(hostView);
    if (!container || !contentView || !shadowHost) {
        return;
    }

    auto* owner = static_cast<FloatingInputBar*>(widget);
    NLFloatingInputShortcutTextView* inputField = ensureInputField(hostView, contentView, owner, placeholderText);
    NSScrollView* inputScroll = inputScrollForView(hostView);
    ensureButtons(hostView, contentView, owner);
    NLFloatingInputIconButton* emojiButton = emojiButtonForView(hostView);
    NLFloatingInputIconButton* imageButton = imageButtonForView(hostView);
    NLFloatingInputIconButton* screenshotButton = screenshotButtonForView(hostView);
    NLFloatingInputIconButton* historyButton = historyButtonForView(hostView);
    NLFloatingInputIconButton* sendButton = sendButtonForView(hostView);
    if (!inputField || !inputScroll) {
        return;
    }

    const NSRect frame = hostFrameForWidget(widget, hostView);
    const NSRect shadowFrame = NSMakeRect(frame.origin.x - style.shadowPaddingX,
                                          frame.origin.y - style.shadowPaddingTop,
                                          frame.size.width + style.shadowPaddingX * 2.0,
                                          frame.size.height + style.shadowPaddingTop + style.shadowPaddingBottom);
    shadowHost.frame = shadowFrame;
    shadowHost.hidden = NO;
    const NSRect containerFrame = NSMakeRect(style.shadowPaddingX,
                                             style.shadowPaddingTop,
                                             frame.size.width,
                                             frame.size.height);
    CGPathRef shadowPath = CGPathCreateWithRoundedRect(containerFrame,
                                                       kCornerRadius,
                                                       kCornerRadius,
                                                       nil);
    shadowHost.layer.shadowPath = shadowPath;
    CGPathRelease(shadowPath);
    container.frame = frame;
    const BOOL hidden = widget->isHidden() || widget->width() <= 0 || widget->height() <= 0;
    const CGFloat alpha = static_cast<CGFloat>(qBound(0.0, opacity, 1.0));
    storeOpacityForView(hostView, alpha);
    shadowHost.alphaValue = alpha;
    container.alphaValue = alpha;
    shadowHost.hidden = hidden;
    container.hidden = hidden;
    if (contentView != container) {
        contentView.frame = container.bounds;
    }
    layoutButtons(contentView,
                  emojiButton,
                  imageButton,
                  screenshotButton,
                  historyButton,
                  sendButton);
    layoutInputField(inputScroll, inputField, contentView);
    updateInactiveGlassChrome(hostView);
}

void focusInputBar(QWidget* widget)
{
    NSView* hostView = topLevelQtViewForWidget(widget, false);
    NLFloatingInputShortcutTextView* inputField = inputTextViewForView(hostView);
    if (inputField && inputField.window) {
        [inputField.window makeFirstResponder:inputField];
    }
}

void clearInputBar(QWidget* widget)
{
    NSLog(@"[NetherLink][FloatingInputBarBridge] clearInputBar widget=%p", widget);
    NSView* hostView = topLevelQtViewForWidget(widget, false);
    if (hostView) {
        hideNativeTooltip();
        clearInputField(hostView);
        clearButtons(hostView);
        clearInactiveGlassChrome(hostView);

        NLFloatingInputBarTarget* target = targetForView(hostView);
        if (target) {
            target.owner = nullptr;
            target.hostView = nil;
        }

        NSView* container = containerForView(hostView);
        NSView* shadowHost = shadowHostForView(hostView);
        if (shadowHost) {
            [shadowHost removeFromSuperview];
        }

        NSView* contentView = contentForView(hostView);
        if (contentView && contentView != container) {
            [contentView removeFromSuperview];
        }

        if (container) {
            [container removeFromSuperview];
        }

        objc_setAssociatedObject(hostView, kContentAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(hostView, kShadowHostAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(hostView, kTargetAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(hostView, kContainerAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(hostView, kVisualOpacityAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
    }
}

} // namespace MacFloatingInputBarBridge

#endif
