#include "jd.h"
#include "extern.h"
#include <math.h>

// A thin white ring drawn directly through SkyLight (no AppKit), floating
// off the focused window with a small gap and ordered right above it. It is
// four thin windows (top, bottom, left, right) instead of one window the size
// of the target: a window's backing store is its full bounding rect, so one
// big window costs tens of MB even when it is mostly transparent.

#define RING_WIDTH 1.5
#define RING_GAP 3.0
#define WINDOW_RADIUS 16.0 // typical macOS window corner radius (VS Code, Chrome); apps vary
#define RING_OUTSET (RING_GAP + RING_WIDTH) // window frame to the ring's outer edge
#define EDGE_THICKNESS (WINDOW_RADIUS + RING_OUTSET) // top/bottom: tall enough to hold the corner arcs
#define SIDE_THICKNESS ceil(RING_WIDTH) // left/right only hold the straight part of the ring

#define RIM_FADE 0.18 // fraction of the ring's height the easing takes

// The ring is white in Dark mode and black in Light mode; its alpha is strongest
// at the very top (rim) and eases to ring over RIM_FADE.
typedef struct {
    CGFloat gray, rim, ring;
} palette_t;
static const palette_t DARK = {1, 0.9, 0.6}, LIGHT = {0, 0.75, 0.55};

enum { TOP, BOTTOM, LEFT, RIGHT, NEDGES };

// WindowServer event numbers (the same ones JankyBorders and yabai register for).
enum { EVENT_WINDOW_MOVE = 806, EVENT_WINDOW_RESIZE = 807 };

static struct {
    uint32_t wid;
    CGContextRef ctx;
} edges[NEDGES];

static int cid;
static bool shown;
static uint32_t last_target;
static CGRect last_outer;
static const palette_t *pal = &DARK;

// Window rects are snapped out to whole points (the ring itself sits at
// fractional offsets inside them); a window at a half-point origin leaves an
// unpainted row along one edge.
static CGRect edge_rect(int e, CGRect o) {
    CGFloat t = EDGE_THICKNESS, s = SIDE_THICKNESS;
    CGRect r;
    switch (e) {
    case TOP: r = CGRectMake(o.origin.x, o.origin.y, o.size.width, t); break;
    case BOTTOM: r = CGRectMake(o.origin.x, o.origin.y + o.size.height - t, o.size.width, t); break;
    case LEFT: r = CGRectMake(o.origin.x, o.origin.y + t, s, o.size.height - 2 * t); break;
    default: r = CGRectMake(o.origin.x + o.size.width - s, o.origin.y + t, s, o.size.height - 2 * t);
    }
    return CGRectIntegral(r);
}

static void appearance_changed(CFNotificationCenterRef, void *, CFNotificationName, const void *, CFDictionaryRef);
static void window_changed(uint32_t event, uint32_t *wid, size_t len, void *ctx);
static void drain_events(CFMachPortRef port, void *msg, CFIndex size, void *info);

void border_init(void) {
    cid = SLSMainConnectionID();
    pal = appearance_dark() ? &DARK : &LIGHT;
    CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(), NULL, appearance_changed,
                                    CFSTR("AppleInterfaceThemeChangedNotification"), NULL,
                                    CFNotificationSuspensionBehaviorDeliverImmediately);
    SLSRegisterNotifyProc(window_changed, EVENT_WINDOW_MOVE, NULL);
    SLSRegisterNotifyProc(window_changed, EVENT_WINDOW_RESIZE, NULL);
    // The notify procs run while the connection's event queue is drained.
    mach_port_t port;
    if (SLSGetEventPort(cid, &port) == kCGErrorSuccess) {
        CFMachPortRef cf_port = CFMachPortCreateWithPort(NULL, port, drain_events, NULL, false);
        _CFMachPortSetOptions(cf_port, 0x40);
        CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(NULL, cf_port, 0);
        CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopDefaultMode);
        CFRelease(src);
        CFRelease(cf_port);
    }
    for (int e = 0; e < NEDGES; e++) {
        CGRect r = {{0, 0}, {1, 1}};
        CFTypeRef region = NULL;
        CGSNewRegionWithRect(&r, &region);
        if (!region) return;
        SLSNewWindow(cid, 2, 0, 0, region, &edges[e].wid);
        CFRelease(region);
        uint32_t wid = edges[e].wid;
        if (!wid) return;

        uint64_t tags[2] = {(1ULL << 1) | (1ULL << 9), 0}; // floating, ignores mouse events
        SLSSetWindowTags(cid, wid, tags, 64);
        SLSSetWindowResolution(cid, wid, 2.0);
        SLSSetWindowOpacity(cid, wid, false);
        edges[e].ctx = SLWindowContextCreate(cid, wid, NULL);
        if (edges[e].ctx) CGContextSetInterpolationQuality(edges[e].ctx, kCGInterpolationNone);
    }
}

static void draw_edge(int e, CGRect outer) {
    CGRect er = edge_rect(e, outer);
    CGRect local = {{0, 0}, er.size};
    CFTypeRef region = NULL;
    CGSNewRegionWithRect(&local, &region);
    if (!region) return;
    SLSSetWindowShape(cid, edges[e].wid, 0, 0, region);
    CFRelease(region);
    SLSMoveWindow(cid, edges[e].wid, &er.origin);

    CGContextRef ctx = edges[e].ctx;
    CGContextClearRect(ctx, local);
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, er.size.height); // SkyLight contexts are y-up, frames are y-down
    CGContextScaleCTM(ctx, 1, -1);
    CGRect path_rect = CGRectInset((CGRect){{0, 0}, outer.size}, RING_WIDTH / 2, RING_WIDTH / 2);
    CGFloat radius = WINDOW_RADIUS + RING_GAP + RING_WIDTH / 2;
    CGFloat max_r = fmin(path_rect.size.width, path_rect.size.height) / 2;
    if (radius > max_r) radius = max_r;
    CGFloat ox = outer.origin.x - er.origin.x, oy = outer.origin.y - er.origin.y;
    CGAffineTransform to_local = CGAffineTransformMakeTranslation(ox, oy);
    CGPathRef path = CGPathCreateWithRoundedRect(path_rect, radius, radius, &to_local);
    CGContextAddPath(ctx, path);
    CGContextSetLineWidth(ctx, RING_WIDTH);
    CGContextReplacePathWithStrokedPath(ctx);
    CGContextClip(ctx);
    CGPathRelease(path);

    // One vertical gradient in the ring's own coordinates, so every edge window
    // draws its slice of the same colours and there is no seam between them.
    enum { STOPS = 5 };
    CGFloat comps[STOPS * 4], locs[STOPS];
    for (int i = 0; i < STOPS; i++) {
        CGFloat u = (CGFloat)i / (STOPS - 1);
        CGFloat ease = u * u * (3 - 2 * u); // smoothstep
        locs[i] = u;
        comps[i * 4] = comps[i * 4 + 1] = comps[i * 4 + 2] = pal->gray;
        comps[i * 4 + 3] = pal->rim + (pal->ring - pal->rim) * ease;
    }
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGGradientRef grad = CGGradientCreateWithColorComponents(space, comps, locs, STOPS);
    CGContextDrawLinearGradient(ctx, grad, (CGPoint){0, oy}, (CGPoint){0, oy + RIM_FADE * outer.size.height},
                                kCGGradientDrawsAfterEndLocation);
    CGGradientRelease(grad);
    CGColorSpaceRelease(space);
    CGContextRestoreGState(ctx);
    CGContextFlush(ctx);
}

// Redraws the shown ring in the new appearance.
static void appearance_changed(CFNotificationCenterRef center __unused, void *observer __unused,
                               CFNotificationName name __unused, const void *object __unused,
                               CFDictionaryRef info __unused) {
    pal = appearance_dark() ? &DARK : &LIGHT;
    if (!shown) return;
    SLSDisableUpdate(cid);
    for (int e = 0; e < NEDGES; e++) draw_edge(e, last_outer);
    SLSReenableUpdate(cid);
}

void border_show(CGRect frame, uint32_t target_wid) {
    if (!edges[NEDGES - 1].ctx) return;
    CGRect outer = CGRectInset(frame, -RING_OUTSET, -RING_OUTSET);
    CGDirectDisplayID d;
    uint32_t n = 0;
    if (CGGetDisplaysWithRect(frame, 1, &d, &n) == kCGErrorSuccess && n) outer = CGRectIntersection(outer, CGDisplayBounds(d));
    if (outer.size.width < 2 * EDGE_THICKNESS || outer.size.height < 2 * EDGE_THICKNESS) {
        border_hide();
        return;
    }

    bool same = shown && target_wid == last_target;
    if (same && CGRectEqualToRect(outer, last_outer)) return;

    SLSDisableUpdate(cid);
    if (same && CGSizeEqualToSize(outer.size, last_outer.size)) { // dragged: only the position changed
        for (int e = 0; e < NEDGES; e++) {
            CGPoint o = edge_rect(e, outer).origin;
            SLSMoveWindow(cid, edges[e].wid, &o);
        }
    } else {
        for (int e = 0; e < NEDGES; e++) draw_edge(e, outer);
    }
    if (!same) {
        SLSRequestNotificationsForWindows(cid, &target_wid, 1); // move / resize events for this window only
        int level = 0;
        SLSGetWindowLevel(cid, target_wid, &level);
        for (int e = 0; e < NEDGES; e++) {
            SLSSetWindowLevel(cid, edges[e].wid, level);
            SLSOrderWindow(cid, edges[e].wid, 1, target_wid); // 1 = above
        }
    }
    SLSReenableUpdate(cid);
    shown = true;
    last_target = target_wid;
    last_outer = outer;
}

void border_hide(void) {
    if (!shown) return;
    for (int e = 0; e < NEDGES; e++) SLSOrderWindow(cid, edges[e].wid, 0, 0); // 0 = out
    shown = false;
}

static void drain_events(CFMachPortRef port __unused, void *msg __unused, CFIndex size __unused, void *info __unused) {
    CGEventRef ev;
    while ((ev = SLEventCreateNextEvent(cid))) CFRelease(ev);
}

// WindowServer reports the followed window's moves and resizes as they happen, ahead of
// the Accessibility notifications, which arrive late and coalesced.
static void window_changed(uint32_t event __unused, uint32_t *wid, size_t len, void *ctx __unused) {
    if (!shown || len < sizeof *wid || *wid != last_target) return;
    CGRect frame;
    if (SLSGetWindowBounds(cid, *wid, &frame) != kCGErrorSuccess) return;
    border_show(frame, *wid);
}
