#include "jd.h"
#include "extern.h"
#include <CoreText/CoreText.h>
#include <dispatch/dispatch.h>
#include <malloc/malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Window switcher panel (hold ⌥, press Tab). The windows, their contexts, the fonts,
// the icons and the entries exist only while the panel is open; the animation
// timer runs only while the highlight is moving.
//
// The highlight is its own small window that slides over the panel. Flushing the
// panel's context again makes SkyLight keep up to two more full-size backing
// surfaces (~4.6MB each) for about a second, so the panel is flushed once and the
// animation only moves the highlight window.

#define PANEL_W 640.0
#define RADIUS 26.0
#define HEADER_H 64.0
#define LABEL_H 26.0
#define ROW_H 42.0
#define ROW_GAP 2.0
#define ROW_INSET 8.0
#define ROW_PAD 14.0
#define ICON 22.0
#define FOOT_H 54.0
#define MAX_ROWS 9
#define SLIDE_MS 120.0

enum { F_TITLE, F_COUNT, F_LABEL, F_ROW, F_APP, F_BTN, F_KEY, NFONTS };
static const struct { CGFloat size, weight; } font_specs[NFONTS] = {
    {20, 0.23}, {14, 0}, {12, 0.23}, {14, 0.23}, {13, 0}, {13, 0.23}, {11, 0},
};

static int cid;
static uint32_t wid, hwid;
static CGContextRef ctx, hctx;
static CTFontRef fonts[NFONTS];
static dispatch_source_t timer;
static sw_entry_t *ents;
static int n, sel, top, rows;
static CGSize size;
static CGPoint origin;
static double pos, from_pos, t0;

static CTFontRef make_font(CGFloat sz, CGFloat weight) {
    CTFontRef base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, sz, NULL);
    if (!base || !weight) return base;
    CFNumberRef w = CFNumberCreate(NULL, kCFNumberCGFloatType, &weight);
    CFDictionaryRef traits = CFDictionaryCreate(NULL, (const void **)&kCTFontWeightTrait, (const void **)&w, 1,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, (const void **)&kCTFontTraitsAttribute, (const void **)&traits, 1,
                                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CTFontDescriptorRef d = CTFontDescriptorCreateWithAttributes(attrs);
    CTFontRef f = CTFontCreateCopyWithAttributes(base, sz, NULL, d);
    CFRelease(d);
    CFRelease(attrs);
    CFRelease(traits);
    CFRelease(w);
    CFRelease(base);
    return f ? f : CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, sz, NULL);
}

static void color(CGContextRef c, uint32_t rgb, CGFloat a, bool stroke) {
    CGFloat r = (rgb >> 16 & 255) / 255.0, g = (rgb >> 8 & 255) / 255.0, b = (rgb & 255) / 255.0;
    if (stroke) CGContextSetRGBStrokeColor(c, r, g, b, a);
    else CGContextSetRGBFillColor(c, r, g, b, a);
}

// One line of text, cut with an ellipsis to maxw (at the end, or at the start for paths).
static CTLineRef fit(CFStringRef s, int font, uint32_t rgb, CGFloat maxw, CTLineTruncationType how) {
    CGColorRef col = CGColorCreateSRGB((rgb >> 16 & 255) / 255.0, (rgb >> 8 & 255) / 255.0, (rgb & 255) / 255.0, 1);
    CFStringRef keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
    CFTypeRef vals[] = {fonts[font], col};
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
    CFRelease(as);
    CFRelease(at);
    CGColorRelease(col);
    return line;
}

static CGFloat width(CTLineRef l) {
    return CTLineGetTypographicBounds(l, NULL, NULL, NULL);
}

// Draws l with its x-height-ish middle on cy, then releases it. The context is y-down.
static void put(CGContextRef c, CTLineRef l, CGFloat x, CGFloat cy) {
    CGFloat asc, desc;
    CTLineGetTypographicBounds(l, &asc, &desc, NULL);
    CGContextSetTextPosition(c, x, cy + (asc - desc) / 2);
    CTLineDraw(l, c);
    CFRelease(l);
}

static CGPathRef rrect(CGFloat x, CGFloat y, CGFloat w, CGFloat h, CGFloat r) {
    return CGPathCreateWithRoundedRect((CGRect){{x, y}, {w, h}}, r, r, NULL);
}

static void fill_rrect(CGContextRef c, CGRect r, CGFloat radius, uint32_t rgb, CGFloat a) {
    CGPathRef p = rrect(r.origin.x, r.origin.y, r.size.width, r.size.height, radius);
    color(c, rgb, a, false);
    CGContextAddPath(c, p);
    CGContextFillPath(c);
    CGPathRelease(p);
}

// A 1pt border drawn inside r.
static void edge_rrect(CGContextRef c, CGRect r, CGFloat radius, uint32_t rgb, CGFloat a) {
    CGPathRef p = rrect(r.origin.x + 0.5, r.origin.y + 0.5, r.size.width - 1, r.size.height - 1, radius - 0.5);
    color(c, rgb, a, true);
    CGContextSetLineWidth(c, 1);
    CGContextAddPath(c, p);
    CGContextStrokePath(c);
    CGPathRelease(p);
}

static CGFloat keycap(CGContextRef c, CFStringRef s, CGFloat x, CGFloat cy, bool dry) {
    CTLineRef l = fit(s, F_KEY, 0xc7c7cc, 1e4, kCTLineTruncationEnd);
    CGFloat w = width(l) + 14;
    if (dry) {
        CFRelease(l);
        return w;
    }
    edge_rrect(c, (CGRect){{x, cy - 9}, {w, 18}}, 6, 0xffffff, 0.22);
    put(c, l, x + 7, cy);
    return w;
}

static bool is_path(CFStringRef s) {
    return CFStringHasPrefix(s, CFSTR("~/")) || CFStringHasPrefix(s, CFSTR("/"));
}

static void draw_header(CGContextRef c) {
    CGContextSetLineWidth(c, 1.6);
    color(c, 0xa1a1a6, 1, true);
    CGPathRef mon = rrect(22 + 2.5, 22 + 3.5, 15, 11, 2.5);
    CGContextAddPath(c, mon);
    CGContextMoveToPoint(c, 22 + 7, 22 + 17.5);
    CGContextAddLineToPoint(c, 22 + 13, 22 + 17.5);
    CGContextStrokePath(c);
    CGPathRelease(mon);

    char buf[32];
    snprintf(buf, sizeof buf, "Desktop %d", wm_current());
    CFStringRef s = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    put(c, fit(s, F_TITLE, 0xf5f5f7, 400, kCTLineTruncationEnd), 56, HEADER_H / 2);
    CFRelease(s);

    snprintf(buf, sizeof buf, "%d window%s", n, n == 1 ? "" : "s");
    s = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    CTLineRef l = fit(s, F_COUNT, 0x8e8e93, 200, kCTLineTruncationEnd);
    put(c, l, size.width - 20 - width(l), HEADER_H / 2);
    CFRelease(s);

    s = CFSTR("Windows");
    put(c, fit(s, F_LABEL, 0x8e8e93, 200, kCTLineTruncationEnd), ROW_INSET + ROW_PAD, HEADER_H + 13);
}

static void draw_row(CGContextRef c, sw_entry_t *e, CGFloat y) {
    CGFloat cy = y + ROW_H / 2, left = ROW_INSET + ROW_PAD + ICON + 12, right = size.width - ROW_INSET - ROW_PAD;
    if (e->icon) {
        CGContextSaveGState(c);
        CGContextTranslateCTM(c, ROW_INSET + ROW_PAD, cy + ICON / 2);
        CGContextScaleCTM(c, 1, -1);
        CGContextDrawImage(c, (CGRect){{0, 0}, {ICON, ICON}}, e->icon);
        CGContextRestoreGState(c);
    }
    CFStringRef main = e->title ? e->title : e->app;
    if (!main) return;
    put(c, fit(main, F_ROW, 0xf5f5f7, right - left, is_path(main) ? kCTLineTruncationStart : kCTLineTruncationEnd), left, cy);
}

static void draw_footer(CGContextRef c) {
    CGFloat cy = size.height - 10 - 17;
    fill_rrect(c, (CGRect){{10, cy - 17}, {34, 34}}, 17, 0xffffff, 0.04);
    edge_rrect(c, (CGRect){{10, cy - 17}, {34, 34}}, 17, 0xffffff, 0.14);
    color(c, 0xa1a1a6, 1, true);
    CGContextSetLineWidth(c, 1.5);
    CGContextSetLineCap(c, kCGLineCapRound);
    CGContextMoveToPoint(c, 27 - 5, cy - 2);
    CGContextAddLineToPoint(c, 27 + 5, cy - 2);
    CGContextMoveToPoint(c, 27 - 5, cy + 2);
    CGContextAddLineToPoint(c, 27 + 1, cy + 2);
    CGContextStrokePath(c);
    CGContextSetLineCap(c, kCGLineCapButt);

    CFStringRef k1 = CFSTR("\u2325 \u3092\u96e2\u3059"), k2 = CFSTR("esc");
    CTLineRef a = fit(CFSTR("\u5207\u308a\u66ff\u3048"), F_BTN, 0xf5f5f7, 200, kCTLineTruncationEnd);
    CTLineRef b = fit(CFSTR("\u30ad\u30e3\u30f3\u30bb\u30eb"), F_APP, 0xa1a1a6, 200, kCTLineTruncationEnd);
    CGFloat w1 = keycap(c, k1, 0, 0, true), w2 = keycap(c, k2, 0, 0, true);
    CGFloat pw = 14 + width(a) + 8 + w1 + 22 + width(b) + 8 + w2 + 6;
    CGFloat x = size.width - 10 - pw;
    fill_rrect(c, (CGRect){{x, cy - 17}, {pw, 34}}, 17, 0xffffff, 0.05);
    edge_rrect(c, (CGRect){{x, cy - 17}, {pw, 34}}, 17, 0xffffff, 0.12);
    x += 14;
    CGFloat wa = width(a);
    put(c, a, x, cy);
    x += wa + 8;
    x += keycap(c, k1, x, cy, false) + 22;
    CGFloat wb = width(b);
    put(c, b, x, cy);
    x += wb + 8;
    keycap(c, k2, x, cy, false);
}

// All of the panel's looks live here. Below the first line the context is y-down.
static void draw(CGContextRef c) {
    CGRect all = {{0, 0}, size};
    CGContextClearRect(c, all);
    CGContextSaveGState(c);
    CGContextTranslateCTM(c, 0, size.height);
    CGContextScaleCTM(c, 1, -1);
    CGContextSetTextMatrix(c, CGAffineTransformMakeScale(1, -1));

    CGPathRef panel = rrect(0, 0, size.width, size.height, RADIUS);
    CGContextSaveGState(c);
    CGContextAddPath(c, panel);
    CGContextClip(c);
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGFloat comps[8] = {38 / 255.0, 38 / 255.0, 40 / 255.0, 0.96, 22 / 255.0, 22 / 255.0, 24 / 255.0, 0.97};
    CGGradientRef g = CGGradientCreateWithColorComponents(cs, comps, NULL, 2);
    CGContextDrawLinearGradient(c, g, CGPointZero, CGPointMake(0, size.height), 0);
    CGGradientRelease(g);
    CGColorSpaceRelease(cs);
    CGMutablePathRef hl = CGPathCreateMutable();
    CGPathRef in1 = rrect(1, 1, size.width - 2, size.height - 2, RADIUS - 1), in2 = rrect(1, 2, size.width - 2, size.height - 2, RADIUS - 1);
    CGPathAddPath(hl, NULL, in1);
    CGPathAddPath(hl, NULL, in2);
    color(c, 0xffffff, 0.08, false);
    CGContextAddPath(c, hl);
    CGContextEOFillPath(c);
    CGPathRelease(in1);
    CGPathRelease(in2);
    CGPathRelease(hl);
    CGContextRestoreGState(c);
    CGPathRelease(panel);
    edge_rrect(c, all, RADIUS, 0xffffff, 0.16);

    draw_header(c);
    for (int r = 0; r < rows; r++) draw_row(c, &ents[top + r], HEADER_H + LABEL_H + r * (ROW_H + ROW_GAP));
    draw_footer(c);
    CGContextRestoreGState(c);
}

static CGSize hl_size(void) {
    return CGSizeMake(size.width - 2 * ROW_INSET, ROW_H);
}

static void draw_highlight(CGContextRef c) {
    CGContextClearRect(c, (CGRect){{0, 0}, hl_size()});
    fill_rrect(c, (CGRect){{0, 0}, hl_size()}, 12, 0xffffff, 0.09);
}

static CGPoint hl_origin(void) {
    double hy = fmin(fmax(pos - top, 0), rows - 1);
    return CGPointMake(origin.x + ROW_INSET, origin.y + HEADER_H + LABEL_H + hy * (ROW_H + ROW_GAP));
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
    if (hwid) {
        SLSOrderWindow(cid, hwid, 0, 0);
        if (hctx) CGContextRelease(hctx);
        SLSReleaseWindow(cid, hwid);
    }
    for (int i = 0; i < NFONTS; i++) {
        if (fonts[i]) CFRelease(fonts[i]);
        fonts[i] = NULL;
    }
    for (int i = 0; i < n; i++) {
        if (ents[i].icon) CGImageRelease(ents[i].icon);
        if (ents[i].app) CFRelease(ents[i].app);
        if (ents[i].title) CFRelease(ents[i].title);
    }
    free(ents);
    ents = NULL;
    ctx = hctx = NULL;
    wid = hwid = 0;
    n = 0;
    malloc_zone_pressure_relief(NULL, 0);
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
    CGPoint o = hl_origin();
    SLSMoveWindow(cid, hwid, &o);
}

// A translucent, mouse-transparent window at (x, y) of the given size, drawn at 2x.
static uint32_t make_window(CGPoint at, CGSize sz, CGContextRef *out) {
    CGRect r = {{0, 0}, sz};
    CFTypeRef region = NULL;
    CGSNewRegionWithRect(&r, &region);
    if (!region) return 0;
    uint32_t w = 0;
    SLSNewWindow(cid, 2, at.x, at.y, region, &w);
    CFRelease(region);
    if (!w) return 0;
    uint64_t tags[2] = {(1ULL << 1) | (1ULL << 9), 0}; // floating, ignores mouse events
    SLSSetWindowTags(cid, w, tags, 64);
    SLSSetWindowResolution(cid, w, 2.0);
    SLSSetWindowOpacity(cid, w, false);
    SLSSetWindowLevel(cid, w, CGWindowLevelForKey(kCGPopUpMenuWindowLevelKey));
    *out = SLWindowContextCreate(cid, w, NULL);
    return w;
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
    size = CGSizeMake(PANEL_W, HEADER_H + LABEL_H + rows * (ROW_H + ROW_GAP) + FOOT_H);
    CGRect d = CGDisplayBounds(CGMainDisplayID());
    origin = CGPointMake(d.origin.x + (d.size.width - size.width) / 2, d.origin.y + (d.size.height - size.height) / 2);
    wid = make_window(origin, size, &ctx);
    CGPoint o = hl_origin();
    hwid = wid ? make_window(o, hl_size(), &hctx) : 0;
    if (!hwid) {
        close_panel();
        return;
    }
    bool ok = ctx && hctx;
    for (int i = 0; i < NFONTS; i++) ok = (fonts[i] = make_font(font_specs[i].size, font_specs[i].weight)) && ok;
    if (!ok) {
        close_panel();
        return;
    }
    draw(ctx);
    CGContextFlush(ctx);
    draw_highlight(hctx);
    CGContextFlush(hctx);
    SLSOrderWindow(cid, wid, 1, 0);
    SLSOrderWindow(cid, hwid, 1, wid);
}

void switcher_step(int dir) {
    if (!wid) {
        open_panel(dir);
        return;
    }
    from_pos = pos;
    sel = ((sel + dir) % n + n) % n;
    int old = top;
    if (sel < top) top = sel;
    else if (sel >= top + rows) top = sel - rows + 1;
    if (top != old) {
        draw(ctx);
        CGContextFlush(ctx);
    }
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
