#include "jd.h"
#include "extern.h"
#include <CoreText/CoreText.h>
#include <dispatch/dispatch.h>
#include <math.h>

// Workspace switch HUD: a row of numbered cells with a white highlight that
// slides to the target, holds, then fades out. The window, its context and
// the font exist only while the HUD is on screen, so nothing is held idle.

#define CELL 44.0
#define GAP 8.0
#define PAD 12.0
#define SLIDE_MS 150.0
#define HOLD_MS 400.0
#define FADE_MS 200.0

static int cid;
static uint32_t wid;
static CGContextRef ctx;
static CTFontRef font;
static dispatch_source_t timer;
static CGSize size;
static double pos, from_pos, t0;
static int target;
static bool settled;

static void draw(void) {
    int n = config_workspaces();
    CGContextClearRect(ctx, (CGRect){{0, 0}, size});
    CGPathRef bg = CGPathCreateWithRoundedRect((CGRect){{0, 0}, size}, 14, 14, NULL);
    CGContextSetRGBFillColor(ctx, 0.1, 0.1, 0.1, 0.85);
    CGContextAddPath(ctx, bg);
    CGContextFillPath(ctx);
    CGPathRelease(bg);

    CGContextSetRGBStrokeColor(ctx, 1, 1, 1, 0.35);
    CGContextSetLineWidth(ctx, 1.5);
    for (int i = 0; i < n; i++) {
        CGPathRef p = CGPathCreateWithRoundedRect((CGRect){{PAD + i * (CELL + GAP), PAD}, {CELL, CELL}}, 8, 8, NULL);
        CGContextAddPath(ctx, p);
        CGContextStrokePath(ctx);
        CGPathRelease(p);
    }
    CGPathRef hp = CGPathCreateWithRoundedRect((CGRect){{PAD + pos * (CELL + GAP), PAD}, {CELL, CELL}}, 8, 8, NULL);
    CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
    CGContextAddPath(ctx, hp);
    CGContextFillPath(ctx);
    CGPathRelease(hp);

    for (int i = 0; i < n; i++) {
        char s[2] = {i == 9 ? '0' : '1' + i, 0};
        CFStringRef str = CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8);
        CGFloat c = fabs(pos - i) < 0.5 ? 0 : 1;
        CGColorRef col = CGColorCreateSRGB(c, c, c, 1);
        CFStringRef keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
        CFTypeRef vals[] = {font, col};
        CFDictionaryRef at = CFDictionaryCreate(NULL, (const void **)keys, (const void **)vals, 2,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, str, at);
        CTLineRef line = CTLineCreateWithAttributedString(as);
        CGRect lb = CTLineGetImageBounds(line, ctx);
        CGContextSetTextPosition(ctx, PAD + i * (CELL + GAP) + (CELL - lb.size.width) / 2 - lb.origin.x,
                                 PAD + (CELL - lb.size.height) / 2 - lb.origin.y);
        CTLineDraw(line, ctx);
        CFRelease(line);
        CFRelease(as);
        CFRelease(at);
        CGColorRelease(col);
        CFRelease(str);
    }
    CGContextFlush(ctx);
}

static void teardown(void) {
    dispatch_source_cancel(timer);
    dispatch_release(timer);
    timer = NULL;
    SLSOrderWindow(cid, wid, 0, 0);
    CGContextRelease(ctx);
    SLSReleaseWindow(cid, wid);
    CFRelease(font);
    ctx = NULL;
    font = NULL;
    wid = 0;
}

static void tick(void) {
    double ms = (CFAbsoluteTimeGetCurrent() - t0) * 1000;
    if (ms < SLIDE_MS) {
        double k = 1 - ms / SLIDE_MS;
        pos = target + (from_pos - target) * k * k * k; // ease-out cubic
        draw();
    } else if (ms < SLIDE_MS + HOLD_MS) {
        if (!settled) {
            pos = target;
            draw();
            settled = true;
        }
    } else if (ms < SLIDE_MS + HOLD_MS + FADE_MS) {
        SLSSetWindowAlpha(cid, wid, 1 - (ms - SLIDE_MS - HOLD_MS) / FADE_MS);
    } else {
        teardown();
    }
}

void hud_show(int from, int to) {
    if (!config_hud()) return;
    if (wid) { // retarget: keep the window, restart from where the highlight is now
        from_pos = pos;
    } else {
        cid = SLSMainConnectionID();
        int n = config_workspaces();
        size = CGSizeMake(PAD * 2 + n * CELL + (n - 1) * GAP, PAD * 2 + CELL);
        CGRect d = CGDisplayBounds(CGMainDisplayID());
        CGRect r = {{0, 0}, size};
        CFTypeRef region = NULL;
        CGSNewRegionWithRect(&r, &region);
        if (!region) return;
        SLSNewWindow(cid, 2, d.origin.x + (d.size.width - size.width) / 2, d.origin.y + (d.size.height - size.height) / 2,
                     region, &wid);
        CFRelease(region);
        if (!wid) return;
        uint64_t tags[2] = {(1ULL << 1) | (1ULL << 9), 0}; // floating, ignores mouse events
        SLSSetWindowTags(cid, wid, tags, 64);
        SLSSetWindowResolution(cid, wid, 2.0);
        SLSSetWindowOpacity(cid, wid, false);
        SLSSetWindowLevel(cid, wid, CGWindowLevelForKey(kCGPopUpMenuWindowLevelKey));
        ctx = SLWindowContextCreate(cid, wid, NULL);
        font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 20, NULL);
        pos = from_pos = from - 1;
    }
    target = to - 1;
    t0 = CFAbsoluteTimeGetCurrent();
    settled = false;
    SLSSetWindowAlpha(cid, wid, 1);
    if (timer) return;
    draw();
    SLSOrderWindow(cid, wid, 1, 0);
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, NSEC_PER_SEC / 60, NSEC_PER_MSEC);
    dispatch_source_set_event_handler(timer, ^{ tick(); });
    dispatch_resume(timer);
}
