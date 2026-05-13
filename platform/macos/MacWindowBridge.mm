#include "MacWindowBridge_p.h"

#ifdef Q_OS_MACOS

#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#import <QuartzCore/QuartzCore.h>
#import <dispatch/dispatch.h>

#include <QWidget>

#include "shared/theme/ThemeManager.h"

static void configureStandardButtons(NSWindow* window);
static void refreshConfiguredWindowChrome(NSWindow* window);

@interface NLTrafficLightsObserver : NSObject
@property(nonatomic, assign) NSWindow* window;
- (instancetype)initWithWindow:(NSWindow*)window;
@end

@implementation NLTrafficLightsObserver

- (instancetype)initWithWindow:(NSWindow*)window
{
    self = [super init];
    if (self) {
        self.window = window;
        NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
        [center addObserver:self
                   selector:@selector(handleWindowLayoutChange:)
                       name:NSWindowDidResizeNotification
                     object:window];
        [center addObserver:self
                   selector:@selector(handleWindowLayoutChange:)
                       name:NSWindowDidEndLiveResizeNotification
                     object:window];
        [center addObserver:self
                   selector:@selector(handleWindowDidFinishFullScreenChange:)
                       name:NSWindowDidEnterFullScreenNotification
                     object:window];
        [center addObserver:self
                   selector:@selector(handleWindowDidFinishFullScreenChange:)
                       name:NSWindowDidExitFullScreenNotification
                     object:window];
        [center addObserver:self
                   selector:@selector(handleWindowLayoutChange:)
                       name:NSWindowDidChangeBackingPropertiesNotification
                     object:window];
        [center addObserver:self
                   selector:@selector(handleWindowLayoutChange:)
                       name:NSWindowDidChangeScreenNotification
                     object:window];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [super dealloc];
}

- (void)handleWindowLayoutChange:(NSNotification*)notification
{
    Q_UNUSED(notification);
    if (self.window) {
        refreshConfiguredWindowChrome(self.window);
    }
}

- (void)handleWindowDidFinishFullScreenChange:(NSNotification*)notification
{
    Q_UNUSED(notification);
    NSWindow* window = self.window;
    if (!window) {
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        refreshConfiguredWindowChrome(window);
    });
}

@end

namespace {

NSString* const kBackdropViewIdentifier = @"netherlink.window.backdrop";
constexpr CGFloat kWindowCornerRadius = 14.0;
constexpr CGFloat kFallbackTitlebarInset = 38.0;
constexpr CGFloat kFallbackLeadingInset = 52.0;
constexpr CGFloat kTrafficLightsHorizontalPadding = 5.5;
constexpr CGFloat kTrafficLightsButtonSize = 11.5;
constexpr CGFloat kTrafficLightsInterButtonGap = 5.5;
constexpr NSInteger kTrafficLightsRetryPasses = 6;
constexpr int64_t kTrafficLightsRetryDelayNanos = 16 * NSEC_PER_MSEC;

const void* const kTrafficLightsLayoutGenerationKey = &kTrafficLightsLayoutGenerationKey;
const void* const kWindowChromeRefreshGenerationKey = &kWindowChromeRefreshGenerationKey;
const void* const kTrafficLightsObserverKey = &kTrafficLightsObserverKey;
const void* const kConfiguredQtViewKey = &kConfiguredQtViewKey;
const void* const kConfiguredHostViewKey = &kConfiguredHostViewKey;
const void* const kConfiguredTintColorKey = &kConfiguredTintColorKey;
const void* const kConfiguredCompactTrafficLightsKey = &kConfiguredCompactTrafficLightsKey;

CGFloat trafficLightsClusterWidth()
{
    return kTrafficLightsHorizontalPadding * 2.0
           + kTrafficLightsButtonSize * 3.0
           + kTrafficLightsInterButtonGap * 2.0;
}

NSColor* toNSColor(const QColor& color, const QColor& fallback)
{
    const QColor resolved = color.isValid() ? color : fallback;
    return [NSColor colorWithCalibratedRed:resolved.redF()
                                     green:resolved.greenF()
                                      blue:resolved.blueF()
                                     alpha:resolved.alphaF()];
}

NSAppearance* appThemeAppearance()
{
    return [NSAppearance appearanceNamed:ThemeManager::instance().isDark()
                                             ? NSAppearanceNameDarkAqua
                                             : NSAppearanceNameAqua];
}

NSView* qtViewForWidget(QWidget* widget)
{
    if (!widget) {
        return nil;
    }

    widget->winId();
    return reinterpret_cast<NSView*>(widget->winId());
}

NSWindow* nativeWindowForWidget(QWidget* widget, NSView** qtViewOut, NSView** hostViewOut)
{
    NSView* qtView = qtViewForWidget(widget);
    if (qtViewOut) {
        *qtViewOut = qtView;
    }
    if (!qtView) {
        return nil;
    }

    NSWindow* window = qtView.window;
    NSView* hostView = qtView.superview;
    if (!hostView && window) {
        hostView = window.contentView;
    }

    if (hostViewOut) {
        *hostViewOut = hostView;
    }

    return window;
}

void prepareLayerBackedView(NSView* view)
{
    if (!view) {
        return;
    }

    [view setWantsLayer:YES];
    view.layer.backgroundColor = NSColor.clearColor.CGColor;
}

bool isWindowFullScreen(NSWindow* window)
{
    return window
           && ((window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen);
}

CGFloat windowCornerRadius(NSWindow* window)
{
    return isWindowFullScreen(window) ? 0.0 : kWindowCornerRadius;
}

void applyWindowCornerRadius(NSWindow* window, NSView* view)
{
    if (!view) {
        return;
    }

    prepareLayerBackedView(view);
    view.layer.cornerRadius = windowCornerRadius(window);
    view.layer.masksToBounds = YES;
}

NSWindowStyleMask configuredStyleMask()
{
    return NSWindowStyleMaskTitled
           | NSWindowStyleMaskClosable
           | NSWindowStyleMaskMiniaturizable
           | NSWindowStyleMaskResizable
           | NSWindowStyleMaskFullSizeContentView;
}

void applyWindowFrameChrome(NSWindow* window)
{
    if (!window) {
        return;
    }

    if (!isWindowFullScreen(window)) {
        const NSWindowStyleMask targetMask = configuredStyleMask();
        if (window.styleMask != targetMask) {
            [window setStyleMask:targetMask];
        }
    }
    [window setCollectionBehavior:([window collectionBehavior]
                                   | NSWindowCollectionBehaviorFullScreenPrimary)];
    [window setTitleVisibility:NSWindowTitleHidden];
    [window setTitlebarAppearsTransparent:YES];
    [window setShowsToolbarButton:NO];
    [window setOpaque:NO];
    [window setBackgroundColor:NSColor.clearColor];
    [window setHasShadow:YES];
    [window setMovableByWindowBackground:NO];
}

NSVisualEffectView* ensureBackdropView(NSView* hostView, NSView* qtView)
{
    if (!hostView) {
        return nil;
    }

    NSVisualEffectView* effectView = nil;
    for (NSView* subview in hostView.subviews) {
        if ([subview isKindOfClass:[NSVisualEffectView class]]
            && [subview.identifier isEqualToString:kBackdropViewIdentifier]) {
            effectView = static_cast<NSVisualEffectView*>(subview);
            break;
        }
    }

    if (!effectView) {
        effectView = [[NSVisualEffectView alloc] initWithFrame:hostView.bounds];
        effectView.identifier = kBackdropViewIdentifier;
        effectView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [hostView addSubview:effectView positioned:NSWindowBelow relativeTo:qtView];
        [effectView release];
    } else if (qtView && effectView.superview == hostView) {
        [hostView addSubview:effectView positioned:NSWindowBelow relativeTo:qtView];
    }

    effectView.frame = hostView.bounds;
    return effectView;
}

NSVisualEffectMaterial backdropMaterial()
{
    return NSVisualEffectMaterialSidebar;
}

void updateLayerContentsScale(NSView* view, CGFloat scale)
{
    if (!view) {
        return;
    }

    if (view.layer) {
        view.layer.contentsScale = scale;
        [view.layer setNeedsDisplay];
    }

    for (NSView* subview in view.subviews) {
        updateLayerContentsScale(subview, scale);
    }
}

NSView* configuredQtView(NSWindow* window)
{
    return window ? static_cast<NSView*>(objc_getAssociatedObject(window, kConfiguredQtViewKey)) : nil;
}

NSView* configuredHostView(NSWindow* window)
{
    return window ? static_cast<NSView*>(objc_getAssociatedObject(window, kConfiguredHostViewKey)) : nil;
}

NSColor* configuredTintColor(NSWindow* window)
{
    return window ? static_cast<NSColor*>(objc_getAssociatedObject(window, kConfiguredTintColorKey)) : nil;
}

bool configuredCompactTrafficLights(NSWindow* window)
{
    NSNumber* enabled = window ? static_cast<NSNumber*>(
                                         objc_getAssociatedObject(window, kConfiguredCompactTrafficLightsKey))
                               : nil;
    return enabled && enabled.boolValue;
}

CGFloat titlebarInset(NSWindow* window)
{
    if (!window || !window.contentView) {
        return kFallbackTitlebarInset;
    }

    CGFloat inset = 0.0;
    if (@available(macOS 10.10, *)) {
        const NSRect bounds = window.contentView.bounds;
        const NSRect layoutRect = window.contentLayoutRect;
        inset = NSMaxY(bounds) - NSMaxY(layoutRect);
    }

    if (inset <= 1.0) {
        NSButton* closeButton = [window standardWindowButton:NSWindowCloseButton];
        if (closeButton) {
            inset = NSHeight(closeButton.frame) + 18.0;
        }
    }

    return MAX(kFallbackTitlebarInset, ceil(inset));
}

CGFloat leadingInset(NSWindow* window, bool compactTrafficLights)
{
    if (compactTrafficLights) {
        return MAX(kFallbackLeadingInset, ceil(trafficLightsClusterWidth()));
    }

    if (!window) {
        return kFallbackLeadingInset;
    }

    CGFloat rightEdge = 0.0;
    const NSWindowButton buttonTypes[] = {
        NSWindowCloseButton,
        NSWindowMiniaturizeButton,
        NSWindowZoomButton
    };
    for (NSWindowButton buttonType : buttonTypes) {
        NSButton* button = [window standardWindowButton:buttonType];
        if (!button) {
            continue;
        }
        rightEdge = MAX(rightEdge, NSMaxX(button.frame));
    }

    return rightEdge > 0.0 ? static_cast<CGFloat>(ceil(rightEdge + 12.0))
                           : kFallbackLeadingInset;
}

void ensureTrafficLightsObserver(NSWindow* window)
{
    if (!window) {
        return;
    }

    if (objc_getAssociatedObject(window, kTrafficLightsObserverKey)) {
        return;
    }

    NLTrafficLightsObserver* observer = [[NLTrafficLightsObserver alloc] initWithWindow:window];
    objc_setAssociatedObject(window,
                             kTrafficLightsObserverKey,
                             observer,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [observer release];
}

NSInteger nextTrafficLightsLayoutGeneration(NSWindow* window)
{
    NSNumber* currentGeneration =
            (NSNumber*)objc_getAssociatedObject(window, kTrafficLightsLayoutGenerationKey);
    const NSInteger nextGeneration = currentGeneration ? currentGeneration.integerValue + 1 : 1;
    objc_setAssociatedObject(window,
                             kTrafficLightsLayoutGenerationKey,
                             @(nextGeneration),
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return nextGeneration;
}

bool isCurrentTrafficLightsLayoutGeneration(NSWindow* window, NSInteger generation)
{
    NSNumber* currentGeneration =
            (NSNumber*)objc_getAssociatedObject(window, kTrafficLightsLayoutGenerationKey);
    return currentGeneration && currentGeneration.integerValue == generation;
}

NSInteger nextWindowChromeRefreshGeneration(NSWindow* window)
{
    NSNumber* currentGeneration =
            (NSNumber*)objc_getAssociatedObject(window, kWindowChromeRefreshGenerationKey);
    const NSInteger nextGeneration = currentGeneration ? currentGeneration.integerValue + 1 : 1;
    objc_setAssociatedObject(window,
                             kWindowChromeRefreshGenerationKey,
                             @(nextGeneration),
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return nextGeneration;
}

bool isCurrentWindowChromeRefreshGeneration(NSWindow* window, NSInteger generation)
{
    NSNumber* currentGeneration =
            (NSNumber*)objc_getAssociatedObject(window, kWindowChromeRefreshGenerationKey);
    return currentGeneration && currentGeneration.integerValue == generation;
}

void applyTrafficLightsLayout(NSButton* closeButton,
                              NSButton* miniButton,
                              NSButton* zoomButton,
                              NSView* clusterView)
{
    if (!closeButton || !miniButton || !zoomButton) {
        return;
    }

    if (clusterView && [clusterView respondsToSelector:@selector(layoutSubtreeIfNeeded)]) {
        [clusterView layoutSubtreeIfNeeded];
    }

    const CGFloat backingScale = closeButton.window ? closeButton.window.backingScaleFactor
                                                    : NSScreen.mainScreen.backingScaleFactor;
    updateLayerContentsScale(clusterView, backingScale);

    NSButton* orderedButtons[] = { closeButton, miniButton, zoomButton };
    CGFloat nextX = kTrafficLightsHorizontalPadding;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    for (NSInteger index = 0; index < 3; ++index) {
        NSButton* button = orderedButtons[index];
        prepareLayerBackedView(button);
        button.layer.affineTransform = CGAffineTransformIdentity;
        button.layer.contentsScale = backingScale;
        button.layer.contents = nil;
        [button setNeedsDisplay:YES];
        [button displayIfNeeded];

        NSRect frame = button.frame;
        const CGFloat naturalWidth = MAX(NSWidth(frame), 1.0);
        const CGFloat naturalHeight = MAX(NSHeight(frame), 1.0);
        const CGFloat centerY = NSMidY(frame);
        const CGFloat scaleX = kTrafficLightsButtonSize / naturalWidth;
        const CGFloat scaleY = kTrafficLightsButtonSize / naturalHeight;

        button.frame = NSMakeRect(nextX,
                                  centerY - naturalHeight / 2.0,
                                  naturalWidth,
                                  naturalHeight);
        button.layer.anchorPoint = CGPointMake(0.0, 0.5);
        button.layer.position = CGPointMake(nextX, centerY);
        CGAffineTransform transform = {
            scaleX, 0.0,
            0.0, scaleY,
            0.0, 0.0
        };
        button.layer.affineTransform = transform;
        [button setNeedsDisplay:YES];
        [button.layer setNeedsDisplay];
        nextX += kTrafficLightsButtonSize + kTrafficLightsInterButtonGap;
    }

    [CATransaction commit];

    if (clusterView) {
        [clusterView setNeedsLayout:YES];
        [clusterView setNeedsDisplay:YES];
    }
}

void scheduleTrafficLightsLayout(NSWindow* window,
                                 NSButton* closeButton,
                                 NSButton* miniButton,
                                 NSButton* zoomButton,
                                 NSView* clusterView,
                                 NSInteger generation,
                                 NSInteger remainingPasses)
{
    if (!window || !isCurrentTrafficLightsLayoutGeneration(window, generation)) {
        return;
    }

    applyTrafficLightsLayout(closeButton, miniButton, zoomButton, clusterView);
    if (remainingPasses <= 0) {
        return;
    }

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kTrafficLightsRetryDelayNanos),
                   dispatch_get_main_queue(), ^{
        scheduleTrafficLightsLayout(window,
                                    closeButton,
                                    miniButton,
                                    zoomButton,
                                    clusterView,
                                    generation,
                                    remainingPasses - 1);
    });
}

void configureStandardButtonsImpl(NSWindow* window)
{
    const NSWindowButton buttonTypes[] = {
        NSWindowCloseButton,
        NSWindowMiniaturizeButton,
        NSWindowZoomButton
    };

    NSView* clusterView = nil;
    NSButton* closeButton = nil;
    NSButton* miniButton = nil;
    NSButton* zoomButton = nil;

    for (NSInteger index = 0; index < 3; ++index) {
        NSButton* button = [window standardWindowButton:buttonTypes[index]];
        if (!button) {
            continue;
        }

        if (index == 0) {
            closeButton = button;
        } else if (index == 1) {
            miniButton = button;
        } else if (index == 2) {
            zoomButton = button;
        }
        button.hidden = NO;
        if ([button.cell respondsToSelector:@selector(setControlSize:)]) {
            [button.cell setControlSize:NSControlSizeSmall];
        }
        if (!clusterView) {
            clusterView = button.superview;
        }
    }

    if (clusterView) {
        clusterView.hidden = YES;
    }
    const NSInteger generation = nextTrafficLightsLayoutGeneration(window);
    applyTrafficLightsLayout(closeButton, miniButton, zoomButton, clusterView);

    if (clusterView) {
        clusterView.hidden = NO;
        [clusterView setNeedsDisplay:YES];
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        scheduleTrafficLightsLayout(window,
                                    closeButton,
                                    miniButton,
                                    zoomButton,
                                    clusterView,
                                    generation,
                                    kTrafficLightsRetryPasses - 1);
    });
}

void applyConfiguredWindowChrome(NSWindow* window)
{
    if (!window) {
        return;
    }

    applyWindowFrameChrome(window);

    NSView* qtView = configuredQtView(window);
    NSView* hostView = configuredHostView(window);
    if (qtView && qtView.superview) {
        hostView = qtView.superview;
        objc_setAssociatedObject(window, kConfiguredHostViewKey, hostView, OBJC_ASSOCIATION_ASSIGN);
    } else if (!hostView) {
        hostView = window.contentView;
    }

    const CGFloat backingScale = window.backingScaleFactor;
    updateLayerContentsScale(window.contentView, backingScale);
    updateLayerContentsScale(hostView, backingScale);
    updateLayerContentsScale(qtView, backingScale);

    prepareLayerBackedView(window.contentView);
    applyWindowCornerRadius(window, window.contentView);
    applyWindowCornerRadius(window, hostView);
    applyWindowCornerRadius(window, qtView);

    NSVisualEffectView* effectView = ensureBackdropView(hostView, qtView);
    if (effectView) {
        prepareLayerBackedView(effectView);
        effectView.frame = hostView.bounds;
        effectView.appearance = appThemeAppearance();
        effectView.material = backdropMaterial();
        effectView.state = NSVisualEffectStateActive;
        effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        NSColor* tint = configuredTintColor(window);
        effectView.layer.backgroundColor =
                (tint ? tint : toNSColor(QColor(), ThemeManager::instance().color(ThemeColor::WindowBackdropTint))).CGColor;
        effectView.layer.cornerRadius = windowCornerRadius(window);
        effectView.layer.masksToBounds = YES;
    }

    if (configuredCompactTrafficLights(window) && !isWindowFullScreen(window)) {
        configureStandardButtons(window);
    }
}

void scheduleConfiguredWindowChromeRefresh(NSWindow* window, NSInteger generation, NSInteger remainingPasses)
{
    if (!window || !isCurrentWindowChromeRefreshGeneration(window, generation)) {
        return;
    }

    applyConfiguredWindowChrome(window);
    if (remainingPasses <= 0) {
        return;
    }

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kTrafficLightsRetryDelayNanos),
                   dispatch_get_main_queue(), ^{
        scheduleConfiguredWindowChromeRefresh(window, generation, remainingPasses - 1);
    });
}

} // namespace

static void configureStandardButtons(NSWindow* window)
{
    configureStandardButtonsImpl(window);
}

static void refreshConfiguredWindowChrome(NSWindow* window)
{
    if (!window) {
        return;
    }

    const NSInteger generation = nextWindowChromeRefreshGeneration(window);
    scheduleConfiguredWindowChromeRefresh(window, generation, kTrafficLightsRetryPasses);
}

namespace MacWindowBridge {

WindowMetrics configureWindow(QWidget* widget, const QColor& tintColor, bool compactTrafficLights)
{
    NSView* qtView = nil;
    NSView* hostView = nil;
    NSWindow* window = nativeWindowForWidget(widget, &qtView, &hostView);
    if (!window || !qtView) {
        return {
            static_cast<int>(kFallbackTitlebarInset),
            static_cast<int>(kFallbackLeadingInset)
        };
    }

    objc_setAssociatedObject(window, kConfiguredQtViewKey, qtView, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(window, kConfiguredHostViewKey, hostView, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(window,
                             kConfiguredTintColorKey,
                             toNSColor(tintColor, ThemeManager::instance().color(ThemeColor::WindowBackdropTint)),
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(window,
                             kConfiguredCompactTrafficLightsKey,
                             @(compactTrafficLights),
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);

    applyWindowFrameChrome(window);

    const CGFloat backingScale = window.backingScaleFactor;
    updateLayerContentsScale(window.contentView, backingScale);
    updateLayerContentsScale(hostView, backingScale);
    updateLayerContentsScale(qtView, backingScale);

    if (compactTrafficLights) {
        ensureTrafficLightsObserver(window);
        if (!isWindowFullScreen(window)) {
            configureStandardButtons(window);
        }
    }
    prepareLayerBackedView(hostView);
    prepareLayerBackedView(qtView);
    applyWindowCornerRadius(window, hostView);
    applyWindowCornerRadius(window, qtView);

    NSVisualEffectView* effectView = ensureBackdropView(hostView, qtView);
    if (effectView) {
        prepareLayerBackedView(effectView);
        effectView.appearance = appThemeAppearance();
        effectView.material = backdropMaterial();
        effectView.state = NSVisualEffectStateActive;
        effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        effectView.layer.backgroundColor = toNSColor(tintColor, ThemeManager::instance().color(ThemeColor::WindowBackdropTint)).CGColor;
        effectView.layer.cornerRadius = windowCornerRadius(window);
        effectView.layer.masksToBounds = YES;
    }

    return {
        static_cast<int>(titlebarInset(window)),
        static_cast<int>(leadingInset(window, compactTrafficLights))
    };
}

bool startWindowDrag(QWidget* widget)
{
    NSView* qtView = nil;
    NSView* hostView = nil;
    NSWindow* window = nativeWindowForWidget(widget, &qtView, &hostView);
    Q_UNUSED(hostView);
    if (!window) {
        return false;
    }

    NSEvent* currentEvent = NSApp.currentEvent;
    if (!currentEvent || currentEvent.type != NSEventTypeLeftMouseDown) {
        return false;
    }

    [window performWindowDragWithEvent:currentEvent];
    return true;
}

bool performWindowZoom(QWidget* widget)
{
    NSView* qtView = nil;
    NSView* hostView = nil;
    NSWindow* window = nativeWindowForWidget(widget, &qtView, &hostView);
    Q_UNUSED(qtView);
    Q_UNUSED(hostView);
    if (!window) {
        return false;
    }

    if ([window respondsToSelector:@selector(performZoom:)]) {
        [window performZoom:nil];
    } else {
        [window zoom:nil];
    }
    return true;
}

} // namespace MacWindowBridge

#endif
