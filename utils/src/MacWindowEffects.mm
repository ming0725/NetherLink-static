#include "MacWindowEffects_p.h"

#ifdef Q_OS_MACOS
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include <QWidget>

namespace {

NSString* const kBackdropViewIdentifier = @"netherlink.backdrop";

NSColor* toNSColor(const QColor& color, const QColor& fallback)
{
    const QColor resolved = color.isValid() ? color : fallback;
    return [NSColor colorWithCalibratedRed:resolved.redF()
                                     green:resolved.greenF()
                                      blue:resolved.blueF()
                                     alpha:resolved.alphaF()];
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

    NSWindow* window = [qtView window];
    NSView* hostView = [qtView superview];
    if (!hostView && window) {
        hostView = [window contentView];
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

NSVisualEffectView* ensureBackdropView(NSView* hostView, NSView* qtView)
{
    if (!hostView) {
        return nil;
    }

    NSVisualEffectView* effectView = nil;
    for (NSView* subview in [hostView subviews]) {
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
    if (@available(macOS 10.14, *)) {
        return NSVisualEffectMaterialWindowBackground;
    }
    return NSVisualEffectMaterialSidebar;
}

} // namespace

namespace MacWindowEffects {

void configureFramelessWindow(QWidget* widget)
{
    NSView* qtView = nil;
    NSView* hostView = nil;
    NSWindow* window = nativeWindowForWidget(widget, &qtView, &hostView);
    if (!window || !qtView) {
        return;
    }

    [window setOpaque:NO];
    [window setBackgroundColor:NSColor.clearColor];
    [window setHasShadow:YES];
    [window setTitleVisibility:NSWindowTitleHidden];
    [window setTitlebarAppearsTransparent:YES];
    [window setMovableByWindowBackground:NO];
    [window setStyleMask:([window styleMask]
                          | NSWindowStyleMaskFullSizeContentView
                          | NSWindowStyleMaskClosable
                          | NSWindowStyleMaskMiniaturizable
                          | NSWindowStyleMaskResizable)];

    prepareLayerBackedView(hostView);
    prepareLayerBackedView(qtView);
}

void applyBackdrop(QWidget* widget, const QColor& tintColor)
{
    configureFramelessWindow(widget);

    NSView* qtView = nil;
    NSView* hostView = nil;
    NSWindow* window = nativeWindowForWidget(widget, &qtView, &hostView);
    if (!window || !hostView) {
        return;
    }

    prepareLayerBackedView(hostView);
    prepareLayerBackedView(qtView);

    NSVisualEffectView* effectView = ensureBackdropView(hostView, qtView);
    if (!effectView) {
        return;
    }

    prepareLayerBackedView(effectView);
    effectView.material = backdropMaterial();
    effectView.state = NSVisualEffectStateActive;
    effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    effectView.layer.backgroundColor = toNSColor(tintColor, QColor(248, 248, 252, 92)).CGColor;
}

} // namespace MacWindowEffects

#endif
