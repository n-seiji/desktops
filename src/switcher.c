#include "jd.h"
#include "extern.h"
#include <CoreText/CoreText.h>
#include <dispatch/dispatch.h>
#include <math.h>
#include <stdlib.h>

// Window switcher panel (hold ⌥, press Tab). The window, its context, the fonts,
// the icons and the entries exist only while the panel is open; the animation
// timer runs only while the highlight is moving.

#define PANEL_W 520.0
#define ROW_H 44.0
#define PAD 12.0
#define ICON 32.0
#define MAX_ROWS 12
#define SLIDE_MS 120.0

static int cid;
static uint32_t wid;
static CGContextRef ctx;
static CTFontRef font_b, font_p;
static dispatch_source_t timer;
static sw_entry_t *ents;
static int n, sel, top, rows;
static CGSize size;
static double pos, from_pos, t0;

static void draw_text(CGContextRef c, CFStringRef s, CTFontRef font, CGFloat alpha, CGFloat x, CGFloat y, CGFloat maxw,
                      CTLineTruncationType how) {
    CGColorRef col = CGColorCreateSRGB(1, 1, 1, alpha);
    CFStringRef keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
    CFTypeRef vals[] = {font, col};
    CFDictionaryRef at = CFDictionaryCreate(NULL, (const void **)keys, (const void **)vals, 2,
                                            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, s, at);
    CTLineRef line = CTLineCreateWithAttributedString(as);
    if (CTLineGetTypographicBounds(line, NULL, NULL, NULL) > maxw) {
        CFAttributedStringRef dots = CFAttributedStringCreate(NULL, CFSTR("\xE2\x80\xA6"), at);
        CTLineRef token = CTLineCreateWithAttributedString(dots);
        CTLineRef cut = CTLineCreateTruncatedLine(line, maxw, how, token);
        CFRelease(token);
        CFRelease(dots);
        if (cut) {
            CFRelease(line);
            line = cut;
        }
    }
    CGContextSetTextPosition(c, x, y);
    CTLineDraw(line, c);
    CFRelease(line);
    CFRelease(as);
    CFRelease(at);
    CGColorRelease(col);
}

static bool is_path(CFStringRef s) {
    return CFStringHasPrefix(s, CFSTR("~/")) || CFStringHasPrefix(s, CFSTR("/"));
}

// All of the panel's looks live here.
static void draw(CGContextRef c) {
    CGContextClearRect(c, (CGRect){{0, 0}, size});
    CGPathRef bg = CGPathCreateWithRoundedRect((CGRect){{0, 0}, size}, 14, 14, NULL);
    CGContextSetRGBFillColor(c, 0.1, 0.1, 0.1, 0.85);
    CGContextAddPath(c, bg);
    CGContextFillPath(c);
    CGPathRelease(bg);

    double hy = fmin(fmax(pos - top, 0), rows - 1);
    CGPathRef hp = CGPathCreateWithRoundedRect((CGRect){{PAD / 2, size.height - PAD - (hy + 1) * ROW_H},
                                                        {size.width - PAD, ROW_H}}, 8, 8, NULL);
    CGContextSetRGBFillColor(c, 1, 1, 1, 0.25);
    CGContextAddPath(c, hp);
    CGContextFillPath(c);
    CGPathRelease(hp);

    CGFloat tx = PAD + ICON + 10, tw = size.width - tx - PAD;
    for (int r = 0; r < rows; r++) {
        sw_entry_t *e = &ents[top + r];
        CGFloat y = size.height - PAD - (r + 1) * ROW_H;
        if (e->icon) CGContextDrawImage(c, (CGRect){{PAD, y + (ROW_H - ICON) / 2}, {ICON, ICON}}, e->icon);
        CFStringRef main = e->title ? e->title : e->app;
        if (main) draw_text(c, main, font_b, 1, tx, y + ROW_H / 2 + 2, tw, is_path(main) ? kCTLineTruncationStart : kCTLineTruncationEnd);
        if (e->title && e->app) draw_text(c, e->app, font_p, 0.6, tx, y + ROW_H / 2 - 14, tw, kCTLineTruncationEnd);
    }
}

static void stop_timer(void) {
    if (!timer) return;
    dispatch_source_cancel(timer);
    dispatch_release(timer);
    timer = NULL;
}

static void close_panel(void) {
    stop_timer();
    if (wid) {
        SLSOrderWindow(cid, wid, 0, 0);
        if (ctx) CGContextRelease(ctx);
        SLSReleaseWindow(cid, wid);
    }
    if (font_b) CFRelease(font_b);
    if (font_p) CFRelease(font_p);
    for (int i = 0; i < n; i++) {
        if (ents[i].icon) CGImageRelease(ents[i].icon);
        if (ents[i].app) CFRelease(ents[i].app);
        if (ents[i].title) CFRelease(ents[i].title);
    }
    free(ents);
    ents = NULL;
    ctx = NULL;
    font_b = font_p = NULL;
    wid = 0;
    n = 0;
}

static void tick(void) {
    double k = (CFAbsoluteTimeGetCurrent() - t0) * 1000 / SLIDE_MS;
    if (k >= 1) {
        pos = sel;
        stop_timer();
    } else {
        k = 1 - k;
        pos = sel + (from_pos - sel) * k * k * k; // ease-out cubic
    }
    draw(ctx);
    CGContextFlush(ctx);
}

static void open_panel(int dir) {
    n = wm_windows(dir, &ents, &sel);
    if (!n) {
        close_panel();
        return;
    }
    cid = SLSMainConnectionID();
    rows = n < MAX_ROWS ? n : MAX_ROWS;
    top = sel < rows ? 0 : sel - rows + 1;
    pos = from_pos = sel;
    size = CGSizeMake(PANEL_W, 2 * PAD + rows * ROW_H);
    CGRect d = CGDisplayBounds(CGMainDisplayID());
    CGRect r = {{0, 0}, size};
    CFTypeRef region = NULL;
    CGSNewRegionWithRect(&r, &region);
    if (!region) {
        close_panel();
        return;
    }
    SLSNewWindow(cid, 2, d.origin.x + (d.size.width - size.width) / 2, d.origin.y + (d.size.height - size.height) / 2,
                 region, &wid);
    CFRelease(region);
    if (!wid) {
        close_panel();
        return;
    }
    uint64_t tags[2] = {(1ULL << 1) | (1ULL << 9), 0}; // floating, ignores mouse events
    SLSSetWindowTags(cid, wid, tags, 64);
    SLSSetWindowResolution(cid, wid, 2.0);
    SLSSetWindowOpacity(cid, wid, false);
    SLSSetWindowLevel(cid, wid, CGWindowLevelForKey(kCGPopUpMenuWindowLevelKey));
    ctx = SLWindowContextCreate(cid, wid, NULL);
    font_b = CTFontCreateUIFontForLanguage(kCTFontUIFontEmphasizedSystem, 15, NULL);
    font_p = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 12, NULL);
    if (!ctx || !font_b || !font_p) {
        close_panel();
        return;
    }
    draw(ctx);
    CGContextFlush(ctx);
    SLSOrderWindow(cid, wid, 1, 0);
}

void switcher_step(int dir) {
    if (!wid) {
        open_panel(dir);
        return;
    }
    from_pos = pos;
    sel = ((sel + dir) % n + n) % n;
    if (sel < top) top = sel;
    else if (sel >= top + rows) top = sel - rows + 1;
    t0 = CFAbsoluteTimeGetCurrent();
    if (timer) return;
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, NSEC_PER_SEC / 60, NSEC_PER_MSEC);
    dispatch_source_set_event_handler(timer, ^{ tick(); });
    dispatch_resume(timer);
}

void switcher_commit(void) {
    if (!wid) return;
    uint32_t target = ents[sel].wid;
    close_panel();
    wm_focus_wid(target);
}

void switcher_cancel(void) {
    if (wid) close_panel();
}
