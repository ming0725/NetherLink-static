#include "WindowEffect.h"

#include <QtCore/QDebug>

#ifdef Q_OS_MACOS
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

static NSString* const kVisualEffectViewIdentifier = @"ChatComponentVisualEffectView";

static NSView* nsViewFromHandle(NativeWindowHandle hWnd)
{
    NSView* view = reinterpret_cast<NSView*>(static_cast<quintptr>(hWnd));
    if (![view isKindOfClass:[NSView class]]) {
        return nil;
    }
    return view;
}

static NSWindow* nsWindowFromHandle(NativeWindowHandle hWnd)
{
    NSView* view = nsViewFromHandle(hWnd);
    return [view window];
}

static NSVisualEffectView* visualEffectViewForWindow(NSWindow* window)
{
    NSView* contentView = [window contentView];
    if (!contentView) {
        return nil;
    }

    NSMutableArray<NSView*>* pending = [NSMutableArray arrayWithObject:contentView];
    while ([pending count] > 0) {
        NSView* view = [pending lastObject];
        [pending removeLastObject];
        if ([view isKindOfClass:[NSVisualEffectView class]] &&
            [[view identifier] isEqualToString:kVisualEffectViewIdentifier]) {
            return static_cast<NSVisualEffectView*>(view);
        }
        for (NSView* subview in [view subviews]) {
            if ([subview isKindOfClass:[NSVisualEffectView class]] &&
                [[subview identifier] isEqualToString:kVisualEffectViewIdentifier]) {
                return static_cast<NSVisualEffectView*>(subview);
            }
            [pending addObject:subview];
        }
    }
    return nil;
}

@interface ChatComponentVisualEffectView : NSVisualEffectView
@end

@implementation ChatComponentVisualEffectView
- (NSView*)hitTest:(NSPoint)point
{
    Q_UNUSED(point);
    return nil;
}
@end
#endif

WindowEffect::WindowEffect()
#ifdef Q_OS_WIN
        : m_pSetWindowCompAttr(nullptr)
#endif
{
#ifdef Q_OS_WIN
    // Dynamically load the (undocumented) API
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        m_pSetWindowCompAttr = reinterpret_cast<PFN_SetWindowCompositionAttribute>(
                GetProcAddress(hUser, "SetWindowCompositionAttribute"));
    }

    if (!m_pSetWindowCompAttr) {
        qWarning() << "Failed to load SetWindowCompositionAttribute";
    }
#endif
}

void WindowEffect::setAcrylicEffect(NativeWindowHandle hWnd,
                                    const QColor& color,
                                    bool enableShadow,
                                    quint32 animId)
{
#ifdef Q_OS_WIN
    if (!m_pSetWindowCompAttr)
        return;

    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    // Set shadow flags if requested (0x20|0x40|0x80|0x100) matches Python code
    policy.AccentFlags = enableShadow ? (0x20|0x40|0x80|0x100) : 0;
    // Use Qt’s ARGB directly (0xAARRGGBB)
    policy.GradientColor = static_cast<DWORD>(color.rgba());
    policy.AnimationId   = animId;

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute  = WCA_ACCENT_POLICY;
    data.Data       = &policy;
    data.SizeOfData = sizeof(policy);

    m_pSetWindowCompAttr(hWnd, &data);
#elif defined(Q_OS_MACOS)
    NSView* qtView = nsViewFromHandle(hWnd);
    NSWindow* window = nsWindowFromHandle(hWnd);
    if (!qtView || !window) {
        return;
    }

    [window setOpaque:NO];
    [window setBackgroundColor:[NSColor clearColor]];
    [window setHasShadow:enableShadow];

    NSView* parentView = [qtView superview];
    if (!parentView) {
        parentView = [window contentView];
    }
    if (!parentView) {
        return;
    }

    NSVisualEffectView* effectView = visualEffectViewForWindow(window);
    if (!effectView) {
        effectView = [[ChatComponentVisualEffectView alloc] initWithFrame:[qtView frame]];
        [effectView setIdentifier:kVisualEffectViewIdentifier];
        [effectView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [parentView addSubview:effectView positioned:NSWindowBelow relativeTo:qtView];
        [effectView release];
    } else if ([effectView superview] != parentView) {
        [effectView removeFromSuperview];
        [parentView addSubview:effectView positioned:NSWindowBelow relativeTo:qtView];
    }

    [effectView setFrame:[parentView bounds]];
    [effectView setState:NSVisualEffectStateActive];
    [effectView setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
    [effectView setMaterial:NSVisualEffectMaterialPopover];
    [effectView setWantsLayer:YES];
    const bool isFullScreen = ([window styleMask] & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen;
    [[effectView layer] setCornerRadius:isFullScreen ? 0.0 : 10.0];
    [[effectView layer] setMasksToBounds:YES];

    if (color.alpha() > 0) {
        NSColor* tint = [NSColor colorWithCalibratedRed:color.redF()
                                                  green:color.greenF()
                                                   blue:color.blueF()
                                                  alpha:color.alphaF()];
        [[effectView layer] setBackgroundColor:[tint CGColor]];
    } else {
        [[effectView layer] setBackgroundColor:nil];
    }
#else
    Q_UNUSED(hWnd);
    Q_UNUSED(color);
    Q_UNUSED(enableShadow);
    Q_UNUSED(animId);
#endif
}

void WindowEffect::setAeroEffect(NativeWindowHandle hWnd)
{
#ifdef Q_OS_WIN
    if (!m_pSetWindowCompAttr)
        return;

    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_BLURBEHIND;
    // other fields zero

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute  = WCA_ACCENT_POLICY;
    data.Data       = &policy;
    data.SizeOfData = sizeof(policy);

    m_pSetWindowCompAttr(hWnd, &data);
#elif defined(Q_OS_MACOS)
    setAcrylicEffect(hWnd);
#else
    Q_UNUSED(hWnd);
#endif
}

void WindowEffect::setMicaEffect(NativeWindowHandle hWnd, bool isDarkMode, bool isAlt)
{
    Q_UNUSED(isDarkMode);
    Q_UNUSED(isAlt);
    setAcrylicEffect(hWnd);
}

void WindowEffect::setTransparentEffect(NativeWindowHandle hWnd)
{
#ifdef Q_OS_WIN
    if (!m_pSetWindowCompAttr)
        return;

    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_TRANSPARENTGRADIENT;

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute = WCA_ACCENT_POLICY;
    data.Data = &policy;
    data.SizeOfData = sizeof(policy);

    m_pSetWindowCompAttr(hWnd, &data);
#elif defined(Q_OS_MACOS)
    NSWindow* window = nsWindowFromHandle(hWnd);
    if (!window) {
        return;
    }
    [window setOpaque:NO];
    [window setBackgroundColor:[NSColor clearColor]];
    removeBackgroundEffect(hWnd);
#else
    Q_UNUSED(hWnd);
#endif
}

void WindowEffect::removeBackgroundEffect(NativeWindowHandle hWnd)
{
#ifdef Q_OS_WIN
    if (!m_pSetWindowCompAttr)
        return;

    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_DISABLED;

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attribute = WCA_ACCENT_POLICY;
    data.Data = &policy;
    data.SizeOfData = sizeof(policy);

    m_pSetWindowCompAttr(hWnd, &data);
#elif defined(Q_OS_MACOS)
    NSWindow* window = nsWindowFromHandle(hWnd);
    if (!window) {
        return;
    }

    NSVisualEffectView* effectView = visualEffectViewForWindow(window);
    if (effectView) {
        [effectView removeFromSuperview];
    }
#else
    Q_UNUSED(hWnd);
#endif
}

void WindowEffect::addShadowEffect(NativeWindowHandle hWnd)
{
#ifdef Q_OS_WIN
    Q_UNUSED(hWnd);
#elif defined(Q_OS_MACOS)
    NSWindow* window = nsWindowFromHandle(hWnd);
    if (window) {
        [window setHasShadow:YES];
    }
#else
    Q_UNUSED(hWnd);
#endif
}

void WindowEffect::removeShadowEffect(NativeWindowHandle hWnd)
{
#ifdef Q_OS_WIN
    Q_UNUSED(hWnd);
#elif defined(Q_OS_MACOS)
    NSWindow* window = nsWindowFromHandle(hWnd);
    if (window) {
        [window setHasShadow:NO];
    }
#else
    Q_UNUSED(hWnd);
#endif
}

void WindowEffect::performWindowZoom(NativeWindowHandle hWnd)
{
#ifdef Q_OS_MACOS
    NSWindow* window = nsWindowFromHandle(hWnd);
    if (!window) {
        return;
    }

    if ([window respondsToSelector:@selector(performZoom:)]) {
        [window performZoom:nil];
    } else {
        [window zoom:nil];
    }
#else
    Q_UNUSED(hWnd);
#endif
}

bool WindowEffect::isWindowZoomed(NativeWindowHandle hWnd) const
{
#ifdef Q_OS_MACOS
    NSWindow* window = nsWindowFromHandle(hWnd);
    return window && [window isZoomed];
#else
    Q_UNUSED(hWnd);
    return false;
#endif
}

void WindowEffect::toggleWindowFullScreen(NativeWindowHandle hWnd)
{
#ifdef Q_OS_MACOS
    NSWindow* window = nsWindowFromHandle(hWnd);
    if (!window) {
        return;
    }

    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary)];
    [window toggleFullScreen:nil];
#else
    Q_UNUSED(hWnd);
#endif
}

bool WindowEffect::isWindowFullScreen(NativeWindowHandle hWnd) const
{
#ifdef Q_OS_MACOS
    NSWindow* window = nsWindowFromHandle(hWnd);
    return window && (([window styleMask] & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen);
#else
    Q_UNUSED(hWnd);
    return false;
#endif
}

void WindowEffect::moveWindow(NativeWindowHandle hWnd)
{
#ifdef Q_OS_WIN
    // Release mouse capture & send WM_SYSCOMMAND to drag the title bar
    ReleaseCapture();
    SendMessageW(hWnd, WM_SYSCOMMAND,
                 SC_MOVE | HTCAPTION,
                 0);
#else
    Q_UNUSED(hWnd);
#endif
}

