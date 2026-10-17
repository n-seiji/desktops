#include "jd.h"
#include "extern.h"
#include <math.h>

// One white border window, drawn directly through SkyLight (no AppKit),
// kept just outside the focused window and ordered right above it.

#define BORDER_WIDTH 4.0
#define CORNER_RADIUS 10.0

static int cid;
static uint32_t bwid;
static CGContextRef ctx;

void border_init(void) {
    cid = SLSMainConnectionID();
    CGRect r = {{0, 0}, {1, 1}};
    CFTypeRef region = NULL;
    CGSNewRegionWithRect(&r, &region);
    if (!region) return;
    SLSNewWindow(cid, 2, 0, 0, region, &bwid);
    CFRelease(region);
    if (!bwid) return;

    uint64_t tags[2] = {(1ULL << 1) | (1ULL << 9), 0}; // floating, ignores mouse events
    SLSSetWindowTags(cid, bwid, tags, 64);
    SLSSetWindowResolution(cid, bwid, 2.0);
    SLSSetWindowOpacity(cid, bwid, false);
    ctx = SLWindowContextCreate(cid, bwid, NULL);
    if (ctx) CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
}

void border_show(CGRect frame, uint32_t target_wid) {
    if (!ctx) return;
    CGRect outer = CGRectInset(frame, -BORDER_WIDTH, -BORDER_WIDTH);
    CGRect local = {{0, 0}, outer.size};
    CFTypeRef region = NULL;
    CGSNewRegionWithRect(&local, &region);
    if (!region) return;

    SLSDisableUpdate(cid);
    SLSSetWindowShape(cid, bwid, 0, 0, region);
    SLSMoveWindow(cid, bwid, &outer.origin);

    CGContextClearRect(ctx, local);
    CGRect path_rect = CGRectInset(local, BORDER_WIDTH / 2, BORDER_WIDTH / 2);
    CGFloat radius = CORNER_RADIUS + BORDER_WIDTH / 2;
    CGFloat max_r = fmin(path_rect.size.width, path_rect.size.height) / 2;
    if (radius > max_r) radius = max_r;
    CGPathRef path = CGPathCreateWithRoundedRect(path_rect, radius, radius, NULL);
    CGContextSetRGBStrokeColor(ctx, 1, 1, 1, 1);
    CGContextSetLineWidth(ctx, BORDER_WIDTH);
    CGContextAddPath(ctx, path);
    CGContextStrokePath(ctx);
    CGPathRelease(path);
    CGContextFlush(ctx);

    int level = 0;
    SLSGetWindowLevel(cid, target_wid, &level);
    SLSSetWindowLevel(cid, bwid, level);
    SLSOrderWindow(cid, bwid, 1, target_wid); // 1 = above
    SLSReenableUpdate(cid);
    CFRelease(region);
}

void border_hide(void) {
    if (bwid) SLSOrderWindow(cid, bwid, 0, 0); // 0 = out
}
