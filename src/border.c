#include "jd.h"
#include "extern.h"
#include <math.h>

// A white border drawn directly through SkyLight (no AppKit), kept just
// outside the focused window and ordered right above it. It is four thin
// windows (top, bottom, left, right) instead of one window the size of the
// target: a window's backing store is its full bounding rect, so one big
// window costs tens of MB even when it is mostly transparent.

#define BORDER_WIDTH 4.0
#define CORNER_RADIUS 10.0
#define EDGE_THICKNESS (BORDER_WIDTH + CORNER_RADIUS) // wide enough to hold a rounded corner

enum { TOP, BOTTOM, LEFT, RIGHT, NEDGES };

static struct {
    uint32_t wid;
    CGContextRef ctx;
} edges[NEDGES];

static int cid;
static bool shown;
static uint32_t last_target;
static CGRect last_outer;

static CGRect edge_rect(int e, CGRect o) {
    CGFloat t = EDGE_THICKNESS;
    switch (e) {
    case TOP: return CGRectMake(o.origin.x, o.origin.y, o.size.width, t);
    case BOTTOM: return CGRectMake(o.origin.x, o.origin.y + o.size.height - t, o.size.width, t);
    case LEFT: return CGRectMake(o.origin.x, o.origin.y + t, t, o.size.height - 2 * t);
    default: return CGRectMake(o.origin.x + o.size.width - t, o.origin.y + t, t, o.size.height - 2 * t);
    }
}

void border_init(void) {
    cid = SLSMainConnectionID();
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
    CGRect path_rect = CGRectInset((CGRect){{0, 0}, outer.size}, BORDER_WIDTH / 2, BORDER_WIDTH / 2);
    CGFloat radius = CORNER_RADIUS + BORDER_WIDTH / 2;
    CGFloat max_r = fmin(path_rect.size.width, path_rect.size.height) / 2;
    if (radius > max_r) radius = max_r;
    CGAffineTransform to_local = CGAffineTransformMakeTranslation(outer.origin.x - er.origin.x, outer.origin.y - er.origin.y);
    CGPathRef path = CGPathCreateWithRoundedRect(path_rect, radius, radius, &to_local);
    CGContextSetRGBStrokeColor(ctx, 1, 1, 1, 1);
    CGContextSetLineWidth(ctx, BORDER_WIDTH);
    CGContextAddPath(ctx, path);
    CGContextStrokePath(ctx);
    CGPathRelease(path);
    CGContextRestoreGState(ctx);
    CGContextFlush(ctx);
}

void border_show(CGRect frame, uint32_t target_wid) {
    if (!edges[NEDGES - 1].ctx) return;
    CGRect outer = CGRectInset(frame, -BORDER_WIDTH, -BORDER_WIDTH);
    CGDirectDisplayID d;
    uint32_t n = 0;
    if (CGGetDisplaysWithRect(frame, 1, &d, &n) == kCGErrorSuccess && n) outer = CGRectIntersection(outer, CGDisplayBounds(d));
    if (outer.size.width < 2 * EDGE_THICKNESS || outer.size.height < 2 * EDGE_THICKNESS) {
        border_hide();
        return;
    }

    SLSDisableUpdate(cid);
    if (!shown || target_wid != last_target || !CGRectEqualToRect(outer, last_outer))
        for (int e = 0; e < NEDGES; e++) draw_edge(e, outer);
    int level = 0;
    SLSGetWindowLevel(cid, target_wid, &level);
    for (int e = 0; e < NEDGES; e++) {
        SLSSetWindowLevel(cid, edges[e].wid, level);
        SLSOrderWindow(cid, edges[e].wid, 1, target_wid); // 1 = above
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
