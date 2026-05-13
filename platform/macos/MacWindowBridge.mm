#include "MacWindowBridge_p.h"

#ifdef Q_OS_MACOS

#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#import <QuartzCore/QuartzCore.h>
#import <dispatch/dispatch.h>

#include <QWidget>

#include "shared/theme/ThemeManager.h"

static void configureStandardButtons(NSWindow* window);

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
        configureStandardButtons(self.window);
    }
}

@end

namespace {

NSString* const kBackdropViewIdentifier = @"netherlink.window.backdrop";
NSString* const kCompactToolbarIdentifier = @"netherlink.window.toolbar";
constexpr CGFloat kWindowCornerRadius = 14.0;
constexpr CGFloat kFallbackTitlebarInset = 38.0;
constexpr CGFloat kFallbackLeadingInset = 52.0;
constexpr CGFloat kTrafficLightsHorizontalPadding = 5.5;
constexpr CGFloat kTrafficLightsButtonSize = 11.5;
constexpr CGFloat kTrafficLightsInterButtonGap = 5.5;
constexpr NSInteger kTrafficLightsRetryPasses = 6;
constexpr int64_t kTrafficLightsRetryDelayNanos = 16 * NSEC_PER_MSEC;

const void* const kTrafficLightsLayoutGenerationKey = &kTrafficLightsLayoutGenerationKey;
const void* const kTrafficLightsObserverKey = &kTrafficLightsObserverKey;

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

void applyRoundedCorners(NSView* view)
{
    if (!view) {
        return;
    }

    prepareLayerBackedView(view);
    view.layer.cornerRadius = kWindowCornerRadius;
    view.layer.masksToBounds = YES;
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

NSToolbar* ensureCompactToolbar(NSWindow* window)
{
    if (!window) {
        return nil;
    }

    NSToolbar* toolbar = window.toolbar;
    if (!toolbar) {
        toolbar = [[NSToolbar alloc] initWithIdentifier:kCompactToolbarIdentifier];
        toolbar.displayMode = NSToolbarDisplayModeIconOnly;
        if ([toolbar respondsToSelector:@selector(setAllowsUserCustomization:)]) {
            toolbar.allowsUserCustomization = NO;
        }
        if ([toolbar respondsToSelector:@selector(setAutosavesConfiguration:)]) {
            toolbar.autosavesConfiguration = NO;
        }
        window.toolbar = toolbar;
        [toolbar release];
        toolbar = window.toolbar;
    }

    return toolbar;
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

    NSButton* orderedButtons[] = { closeButton, miniButton, zoomButton };
    CGFloat nextX = kTrafficLightsHorizontalPadding;

    for (NSInteger index = 0; index < 3; ++index) {
        NSButton* button = orderedButtons[index];
        NSRect frame = button.frame;
        const CGFloat naturalWidth = MAX(NSWidth(frame), 1.0);
        const CGFloat naturalHeight = MAX(NSHeight(frame), 1.0);
        const CGFloat centerY = NSMidY(frame);
        const CGFloat scaleX = kTrafficLightsButtonSize / naturalWidth;
        const CGFloat scaleY = kTrafficLightsButtonSize / naturalHeight;

        prepareLayerBackedView(button);
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
        nextX += kTrafficLightsButtonSize + kTrafficLightsInterButtonGap;
    }

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

} // namespace

static void configureStandardButtons(NSWindow* window)
{
    configureStandardButtonsImpl(window);
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

    const NSWindowStyleMask targetMask =
            NSWindowStyleMaskTitled
            | NSWindowStyleMaskClosable
            | NSWindowStyleMaskMiniaturizable
            | NSWindowStyleMaskResizable
            | NSWindowStyleMaskFullSizeContentView;

    [window setStyleMask:targetMask];
    [window setOpaque:NO];
    [window setBackgroundColor:NSColor.clearColor];
    [window setHasShadow:YES];
    [window setTitleVisibility:NSWindowTitleHidden];
    [window setTitlebarAppearsTransparent:YES];
    [window setMovableByWindowBackground:NO];
    [window setShowsToolbarButton:NO];

    if (compactTrafficLights) {
        ensureCompactToolbar(window);
        ensureTrafficLightsObserver(window);
        if ([window respondsToSelector:@selector(setToolbarStyle:)]) {
            window.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
        }
        configureStandardButtons(window);
    }
    prepareLayerBackedView(hostView);
    prepareLayerBackedView(qtView);
    applyRoundedCorners(hostView);
    applyRoundedCorners(qtView);

    NSVisualEffectView* effectView = ensureBackdropView(hostView, qtView);
    if (effectView) {
        prepareLayerBackedView(effectView);
        effectView.appearance = appThemeAppearance();
        effectView.material = backdropMaterial();
        effectView.state = NSVisualEffectStateActive;
        effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        effectView.layer.backgroundColor = toNSColor(tintColor, ThemeManager::instance().color(ThemeColor::WindowBackdropTint)).CGColor;
        effectView.layer.cornerRadius = kWindowCornerRadius;
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

} // namespace MacWindowBridge

#endif
