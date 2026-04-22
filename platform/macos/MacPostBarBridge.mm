#include "MacPostBarBridge_p.h"

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

#include <QMetaObject>
#include <QOperatingSystemVersion>
#include <QWidget>

@interface NLPostBarTarget : NSObject
@property(nonatomic, assign) QWidget* owner;
- (instancetype)initWithOwner:(QWidget*)owner;
- (void)handleButtonTap:(NSButton*)sender;
@end

@interface NLPostBarButton : NSButton
@end

@implementation NLPostBarTarget

- (instancetype)initWithOwner:(QWidget*)owner
{
    self = [super init];
    if (self) {
        self.owner = owner;
    }
    return self;
}

- (void)handleButtonTap:(NSButton*)sender
{
    if (!self.owner) {
        return;
    }

    const NSInteger selectedSegment = sender.tag;
    QMetaObject::invokeMethod(self.owner,
                              "onNativeSelectionChanged",
                              Qt::QueuedConnection,
                              Q_ARG(int, static_cast<int>(selectedSegment)));
}

@end

@implementation NLPostBarButton

- (void)resetCursorRects
{
    [super resetCursorRects];
    [self addCursorRect:self.bounds cursor:NSCursor.pointingHandCursor];
}

@end

namespace {

const void* const kContainerAssociationKey = &kContainerAssociationKey;
const void* const kContentAssociationKey = &kContentAssociationKey;
const void* const kShadowHostAssociationKey = &kShadowHostAssociationKey;
const void* const kSelectionAssociationKey = &kSelectionAssociationKey;
const void* const kButtonsAssociationKey = &kButtonsAssociationKey;
const void* const kTargetAssociationKey = &kTargetAssociationKey;

constexpr CGFloat kCornerRadius = 15.0;
constexpr CGFloat kHorizontalInset = 10.0;
constexpr CGFloat kVerticalInset = 5.0;
constexpr CGFloat kSelectionInsetX = 4.0;
constexpr CGFloat kSelectionInsetY = 2.0;
constexpr CGFloat kSelectionCornerRadius = 9.0;
constexpr CGFloat kLabelFontSize = 13.0;
constexpr NSTimeInterval kSelectionAnimationDuration = 0.28;

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

constexpr CGFloat kNativeBlurShadowOpacity = 0.26;
constexpr CGFloat kNativeBlurShadowRadius = 26.0;
constexpr CGFloat kNativeBlurShadowYOffset = 0.0;
constexpr CGFloat kNativeBlurShadowPaddingX = 22.0;
constexpr CGFloat kNativeBlurShadowPaddingTop = 18.0;
constexpr CGFloat kNativeBlurShadowPaddingBottom = 22.0;
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

AppearanceStyle styleForAppearance(MacPostBarBridge::Appearance appearance)
{
    switch (appearance) {
    case MacPostBarBridge::Appearance::NativeBlur:
        return kNativeBlurStyle;
    case MacPostBarBridge::Appearance::LiquidGlass:
        return kLiquidGlassStyle;
    case MacPostBarBridge::Appearance::Unsupported:
    default:
        return kLiquidGlassStyle;
    }
}

MacPostBarBridge::Appearance testAppearanceOverride()
{
    switch (MacPostBarBridge::kPostBarTestMode) {
    case 1:
        return MacPostBarBridge::Appearance::LiquidGlass;
    case 2:
        return MacPostBarBridge::Appearance::NativeBlur;
    case 3:
        return MacPostBarBridge::Appearance::Unsupported;
    default:
        return MacPostBarBridge::Appearance::LiquidGlass;
    }
}

MacPostBarBridge::Appearance currentAppearance()
{
    const MacPostBarBridge::Appearance forced = testAppearanceOverride();
    if (forced == MacPostBarBridge::Appearance::Unsupported) {
        return forced;
    }

    if (NSClassFromString(@"NSVisualEffectView") == nil || NSClassFromString(@"NSButton") == nil) {
        return MacPostBarBridge::Appearance::Unsupported;
    }

    if (forced == MacPostBarBridge::Appearance::NativeBlur) {
        return MacPostBarBridge::Appearance::NativeBlur;
    }

#if NETHERLINK_HAS_NS_GLASS_EFFECT_VIEW
    if (@available(macOS 26.0, *)) {
        if (NSClassFromString(@"NSGlassEffectView") != nil) {
            return MacPostBarBridge::Appearance::LiquidGlass;
        }
    }
#endif

    const QOperatingSystemVersion current = QOperatingSystemVersion::current();
    if (current.type() == QOperatingSystemVersion::MacOS && current.majorVersion() >= 26) {
        return MacPostBarBridge::Appearance::NativeBlur;
    }

    return MacPostBarBridge::Appearance::NativeBlur;
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

NLPostBarTarget* targetForView(NSView* qtView)
{
    return qtView
            ? static_cast<NLPostBarTarget*>(objc_getAssociatedObject(qtView, kTargetAssociationKey))
            : nil;
}

NSView* selectionViewFor(NSView* qtView)
{
    return qtView
            ? static_cast<NSView*>(objc_getAssociatedObject(qtView, kSelectionAssociationKey))
            : nil;
}

NSMutableArray<NSButton*>* buttonsFor(NSView* qtView)
{
    return qtView
            ? static_cast<NSMutableArray<NSButton*>*>(objc_getAssociatedObject(qtView, kButtonsAssociationKey))
            : nil;
}

void configureHostView(NSView* qtView)
{
    if (!qtView) {
        return;
    }

    qtView.wantsLayer = YES;
    qtView.layer.backgroundColor = NSColor.clearColor.CGColor;
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
        glassContainer.contentView = contentView;
        objc_setAssociatedObject(hostView,
                                 kContentAssociationKey,
                                 contentView,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [contentView release];
        contentView = contentForView(hostView);
    } else {
        contentView.frame = container.bounds;
    }

    return contentView;
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

    // Native blur should sample the Qt content behind the bar inside the same window.
    container.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    container.material = NSVisualEffectMaterialPopover;
    container.state = NSVisualEffectStateActive;
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    container.layer.backgroundColor = [[NSColor colorWithWhite:1.0 alpha:style.backgroundAlpha] CGColor];
    container.layer.borderWidth = 1.0;
    container.layer.borderColor = [[NSColor colorWithWhite:1.0 alpha:style.borderAlpha] CGColor];
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

NSView* ensureContainer(NSView* hostView, MacPostBarBridge::Appearance appearance)
{
    const AppearanceStyle style = styleForAppearance(appearance);
    switch (appearance) {
    case MacPostBarBridge::Appearance::LiquidGlass:
#if NETHERLINK_HAS_NS_GLASS_EFFECT_VIEW
        return ensureGlassContainer(hostView, style);
#else
        return ensureVisualEffectContainer(hostView, style);
#endif
    case MacPostBarBridge::Appearance::NativeBlur:
        return ensureVisualEffectContainer(hostView, style);
    case MacPostBarBridge::Appearance::Unsupported:
    default:
        return nil;
    }
}

NSView* ensureSelectionView(NSView* qtView, NSView* contentView)
{
    if (!qtView || !contentView) {
        return nil;
    }

    NSView* selectionView = selectionViewFor(qtView);
    if (!selectionView) {
        selectionView = [[NSView alloc] initWithFrame:NSZeroRect];
        selectionView.hidden = YES;
        selectionView.wantsLayer = YES;
        selectionView.layer.backgroundColor = [[NSColor colorWithWhite:0.55 alpha:0.26] CGColor];
        selectionView.layer.cornerRadius = kSelectionCornerRadius;
        selectionView.layer.masksToBounds = YES;
        objc_setAssociatedObject(qtView,
                                 kSelectionAssociationKey,
                                 selectionView,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [contentView addSubview:selectionView];
        [selectionView release];
        selectionView = selectionViewFor(qtView);
    } else if (selectionView.superview != contentView) {
        [selectionView removeFromSuperview];
        [contentView addSubview:selectionView];
    }

    return selectionView;
}

NLPostBarTarget* ensureTarget(NSView* qtView, QWidget* widget)
{
    if (!qtView) {
        return nil;
    }

    NLPostBarTarget* target = targetForView(qtView);
    if (!target) {
        target = [[NLPostBarTarget alloc] initWithOwner:widget];
        objc_setAssociatedObject(qtView,
                                 kTargetAssociationKey,
                                 target,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [target release];
        target = targetForView(qtView);
    } else {
        target.owner = widget;
    }

    return target;
}

NSFont* labelFont()
{
    return [NSFont systemFontOfSize:kLabelFontSize weight:NSFontWeightSemibold];
}

NSDictionary<NSAttributedStringKey, id>* labelAttributes(bool selected)
{
    return @{
        NSFontAttributeName: labelFont(),
        NSForegroundColorAttributeName: selected
                ? [NSColor colorWithWhite:0.15 alpha:0.95]
                : [NSColor colorWithWhite:0.05 alpha:0.88]
    };
}

NSAttributedString* attributedLabel(const QString& label, bool selected)
{
    const QByteArray utf8 = label.toUtf8();
    NSString* nativeLabel = [NSString stringWithUTF8String:utf8.constData()];
    return [[[NSAttributedString alloc] initWithString:(nativeLabel ?: @"")
                                            attributes:labelAttributes(selected)] autorelease];
}

CGFloat buttonWidthForLabel(const QString& label)
{
    NSAttributedString* title = attributedLabel(label, false);
    return ceil(title.size.width);
}

void applyButtonStyle(NSButton* button, const QString& label, bool selected)
{
    if (!button) {
        return;
    }

    button.attributedTitle = attributedLabel(label, selected);
}

void animateSelectionFrame(NSView* selectionView, const NSRect& targetFrame)
{
    if (!selectionView) {
        return;
    }

    [selectionView.layer removeAllAnimations];
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
        context.duration = kSelectionAnimationDuration;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        selectionView.animator.frame = targetFrame;
    } completionHandler:nil];
}

NSMutableArray<NSButton*>* ensureButtons(NSView* qtView,
                                         NSView* contentView,
                                         QWidget* widget,
                                         const QStringList& labels,
                                         int selectedIndex)
{
    if (!qtView || !contentView) {
        return nil;
    }

    NSMutableArray<NSButton*>* buttons = buttonsFor(qtView);
    if (!buttons || buttons.count != labels.size()) {
        for (NSButton* button in buttons) {
            [button removeFromSuperview];
        }

        buttons = [NSMutableArray arrayWithCapacity:labels.size()];
        objc_setAssociatedObject(qtView,
                                 kButtonsAssociationKey,
                                 buttons,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }

    NLPostBarTarget* target = ensureTarget(qtView, widget);

    for (NSInteger index = 0; index < labels.size(); ++index) {
        NSButton* button = index < buttons.count ? buttons[index] : nil;
        if (!button) {
            button = [[NLPostBarButton alloc] initWithFrame:NSZeroRect];
            button.bordered = NO;
            button.buttonType = NSButtonTypeMomentaryChange;
            button.focusRingType = NSFocusRingTypeNone;
            button.bezelStyle = NSBezelStyleRegularSquare;
            [contentView addSubview:button];
            [buttons addObject:button];
            [button release];
            button = buttons[index];
        } else if (button.superview != contentView) {
            [button removeFromSuperview];
            [contentView addSubview:button];
        }

        button.tag = index;
        button.target = target;
        button.action = @selector(handleButtonTap:);
        applyButtonStyle(button, labels.at(index), index == selectedIndex);
    }

    return buttons;
}

void layoutButtons(NSMutableArray<NSButton*>* buttons,
                   NSView* selectionView,
                   NSView* contentView,
                   const QStringList& labels,
                   int selectedIndex,
                   bool animateSelection)
{
    if (!buttons || !selectionView || !contentView) {
        return;
    }

    const NSRect bounds = contentView.bounds;
    const CGFloat buttonHeight = NSHeight(bounds) - kVerticalInset * 2.0;
    const CGFloat availableWidth = NSWidth(bounds) - kHorizontalInset * 2.0;
    const NSInteger count = buttons.count;
    if (count <= 0 || availableWidth <= 0.0) {
        selectionView.hidden = YES;
        return;
    }

    const CGFloat slotWidth = availableWidth / static_cast<CGFloat>(count);

    for (NSInteger index = 0; index < count; ++index) {
        NSButton* button = buttons[index];
        const CGFloat minLabelWidth = buttonWidthForLabel(labels.at(index));
        const CGFloat buttonWidth = MAX(slotWidth, minLabelWidth);
        button.frame = NSMakeRect(kHorizontalInset + slotWidth * index,
                                  kVerticalInset,
                                  buttonWidth,
                                  buttonHeight);
        if (button.window) {
            [button.window invalidateCursorRectsForView:button];
        }
    }

    if (selectedIndex >= 0 && selectedIndex < count) {
        const NSRect selectedFrame = NSMakeRect(kHorizontalInset + slotWidth * selectedIndex,
                                                kVerticalInset,
                                                slotWidth,
                                                buttonHeight);
        const NSRect targetFrame = NSInsetRect(selectedFrame, kSelectionInsetX, kSelectionInsetY);
        const bool hadSelection = !selectionView.hidden;
        selectionView.hidden = NO;
        if (animateSelection && hadSelection) {
            animateSelectionFrame(selectionView, targetFrame);
        } else {
            [selectionView.layer removeAllAnimations];
            selectionView.frame = targetFrame;
        }
    } else {
        selectionView.hidden = YES;
    }
}

} // namespace

namespace MacPostBarBridge {

Appearance appearance()
{
    return currentAppearance();
}

bool isSupported()
{
    return appearance() != Appearance::Unsupported;
}

void syncBar(QWidget* widget,
             const QStringList& labels,
             int selectedIndex,
             bool animateSelection,
             double opacity)
{
    const Appearance current = appearance();
    if (!widget || current == Appearance::Unsupported || !widget->isVisible()) {
        clearBar(widget);
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
    NSView* selectionView = ensureSelectionView(hostView, contentView);
    NSMutableArray<NSButton*>* buttons = ensureButtons(hostView, contentView, widget, labels, selectedIndex);
    NSView* shadowHost = shadowHostForView(hostView);
    if (!container || !contentView || !selectionView || !buttons || !shadowHost) {
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
    shadowHost.alphaValue = alpha;
    container.alphaValue = alpha;
    shadowHost.hidden = hidden;
    container.hidden = hidden;
    if (contentView != container) {
        contentView.frame = container.bounds;
    }
    layoutButtons(buttons, selectionView, contentView, labels, selectedIndex, animateSelection);
}

void clearBar(QWidget* widget)
{
    NSView* hostView = topLevelQtViewForWidget(widget, false);
    NSView* container = containerForView(hostView);
    if (container) {
        [container removeFromSuperview];
    }

    if (hostView) {
        NSView* shadowHost = shadowHostForView(hostView);
        if (shadowHost) {
            [shadowHost removeFromSuperview];
        }

        NSView* contentView = contentForView(hostView);
        if (contentView && contentView != container) {
            [contentView removeFromSuperview];
        }

        objc_setAssociatedObject(hostView, kContentAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(hostView, kShadowHostAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(hostView, kButtonsAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(hostView, kSelectionAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(hostView, kTargetAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(hostView, kContainerAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
    }
}

} // namespace MacPostBarBridge

#endif
