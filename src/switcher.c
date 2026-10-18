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

#define MIN_W 420.0
#define MAX_W 750.0
#define RADIUS 26.0
#define HEADER_H 34.0
#define ROW_H 46.0
#define ROW_INSET 8.0
#define ROW_PAD 8.0
#define ROW_RADIUS 12.0
#define ICON 28.0
#define ICON_GAP 14.0
#define BLUR 30
#define MAX_ROWS 9
#define SLIDE_MS 120.0

// The panel's colours; the dark one is chosen unless the system is in Light mode.
typedef struct {
    uint32_t tint;
    CGFloat tint_a;
    CGFloat edge_gray, edge_top, edge_bottom; // the border's colour and its alpha at the top and the flat rim
    uint32_t header, title, highlight;
    CGFloat highlight_a;
} palette_t;
static const palette_t DARK = {0x141416, 0.85, 1, 0.35, 0.18, 0x9a9a9e, 0xffffff, 0xffffff, 0.10};
static const palette_t LIGHT = {0xf6f6f8, 0.80, 0, 0.10, 0.16, 0x6e6e73, 0x1d1d1f, 0x000000, 0.08};

enum { F_HEADER, F_ROW, NFONTS };
static const struct { CGFloat size, weight; } font_specs[NFONTS] = {
    {13, 0.23}, {16, 0.23},
};

static int cid;
static uint32_t wid, hwid;
static CGContextRef ctx, hctx;
static CTFontRef fonts[NFONTS];
static dispatch_source_t timer;
static sw_entry_t *ents;
static int n, sel, top, rows;
static bool hl_last;
static const palette_t *pal;
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

// A 1pt border drawn inside r, lit along the top and fading to a flat rim.
static void edge_rrect(CGContextRef c, CGRect r) {
    CGPathRef p = rrect(r.origin.x + 0.5, r.origin.y + 0.5, r.size.width - 1, r.size.height - 1, RADIUS - 0.5);
    CGContextSaveGState(c);
    CGContextSetLineWidth(c, 1);
    CGContextAddPath(c, p);
    CGContextReplacePathWithStrokedPath(c);
    CGContextClip(c);
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGFloat g0 = pal->edge_gray;
    CGFloat comps[8] = {g0, g0, g0, pal->edge_top, g0, g0, g0, pal->edge_bottom};
    CGGradientRef g = CGGradientCreateWithColorComponents(cs, comps, NULL, 2);
    CGContextDrawLinearGradient(c, g, CGPointMake(0, r.origin.y), CGPointMake(0, r.origin.y + RADIUS * 1.5),
                                kCGGradientDrawsAfterEndLocation);
    CGGradientRelease(g);
    CGColorSpaceRelease(cs);
    CGContextRestoreGState(c);
    CGPathRelease(p);
}

static bool is_path(CFStringRef s) {
    return CFStringHasPrefix(s, CFSTR("~/")) || CFStringHasPrefix(s, CFSTR("/"));
}

static CFStringRef main_text(const sw_entry_t *e) {
    return e->title ? e->title : e->app;
}

// The panel's width: the widest title plus the row's padding, within MIN_W..MAX_W.
static CGFloat fit_width(void) {
    CGFloat w = 0;
    for (int i = 0; i < n; i++) {
        CFStringRef s = main_text(&ents[i]);
        if (!s) continue;
        CTLineRef l = fit(s, F_ROW, 0, 1e9, kCTLineTruncationEnd);
        w = fmax(w, CTLineGetTypographicBounds(l, NULL, NULL, NULL));
        CFRelease(l);
    }
    w = ceil(2 * (ROW_INSET + ROW_PAD) + ICON + ICON_GAP + w);
    return fmin(fmax(w, MIN_W), MAX_W);
}

static void draw_header(CGContextRef c) {
    char buf[32];
    snprintf(buf, sizeof buf, "Desktop %d", wm_current());
    CFStringRef s = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    put(c, fit(s, F_HEADER, pal->header, size.width - 2 * (ROW_INSET + ROW_PAD), kCTLineTruncationEnd), ROW_INSET + ROW_PAD, 19);
    CFRelease(s);
}

static void draw_row(CGContextRef c, sw_entry_t *e, CGFloat y) {
    CGFloat cy = y + ROW_H / 2, left = ROW_INSET + ROW_PAD + ICON + ICON_GAP, right = size.width - ROW_INSET - ROW_PAD;
    if (e->icon) {
        CGContextSaveGState(c);
        CGContextTranslateCTM(c, ROW_INSET + ROW_PAD, cy + ICON / 2);
        CGContextScaleCTM(c, 1, -1);
        CGContextDrawImage(c, (CGRect){{0, 0}, {ICON, ICON}}, e->icon);
        CGContextRestoreGState(c);
    }
    CFStringRef main = main_text(e);
    if (!main) return;
    put(c, fit(main, F_ROW, pal->title, right - left, is_path(main) ? kCTLineTruncationStart : kCTLineTruncationEnd), left, cy);
}

// All of the panel's looks live here. Below the first line the context is y-down.
static void draw(CGContextRef c) {
    CGRect all = {{0, 0}, size};
    CGContextClearRect(c, all);
    CGContextSaveGState(c);
    CGContextTranslateCTM(c, 0, size.height);
    CGContextScaleCTM(c, 1, -1);
    CGContextSetTextMatrix(c, CGAffineTransformMakeScale(1, -1));

    fill_rrect(c, all, RADIUS, pal->tint, pal->tint_a);
    edge_rrect(c, all);

    draw_header(c);
    for (int r = 0; r < rows; r++) draw_row(c, &ents[top + r], HEADER_H + r * ROW_H);
    CGContextRestoreGState(c);
}

static CGSize hl_size(void) {
    return CGSizeMake(size.width - 2 * ROW_INSET, ROW_H);
}

// The bottom row's highlight sits concentric with the panel's bottom corners.
static void draw_highlight(CGContextRef c) {
    CGSize sz = hl_size();
    CGFloat br = hl_last ? RADIUS - ROW_INSET : ROW_RADIUS;
    CGContextClearRect(c, (CGRect){{0, 0}, sz});
    CGContextSaveGState(c);
    CGContextTranslateCTM(c, 0, sz.height);
    CGContextScaleCTM(c, 1, -1);
    CGContextMoveToPoint(c, ROW_RADIUS, 0);
    CGContextAddArcToPoint(c, sz.width, 0, sz.width, br, ROW_RADIUS);
    CGContextAddArcToPoint(c, sz.width, sz.height, sz.width - br, sz.height, br);
    CGContextAddArcToPoint(c, 0, sz.height, 0, sz.height - br, br);
    CGContextAddArcToPoint(c, 0, 0, ROW_RADIUS, 0, ROW_RADIUS);
    CGContextClosePath(c);
    color(c, pal->highlight, pal->highlight_a, false);
    CGContextFillPath(c);
    CGContextRestoreGState(c);
}

static CGPoint hl_origin(void) {
    double hy = fmin(fmax(pos - top, 0), rows - 1);
    return CGPointMake(origin.x + ROW_INSET, origin.y + HEADER_H + hy * ROW_H);
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

// The panel's shape: horizontal half-point strips approximating the rounded rect, so the
// background blur does not show outside the corners.
static CFTypeRef rounded_region(CGSize sz) {
    HIMutableShapeRef shape = HIShapeCreateMutable();
    int steps = (int)(RADIUS * 2);
    for (int k = 0; k < steps; k++) {
        CGFloat dy = RADIUS - (k + 0.5) / 2, dx = RADIUS - sqrt(RADIUS * RADIUS - dy * dy);
        CGRect top = {{dx, k / 2.0}, {sz.width - 2 * dx, 0.5}};
        CGRect bot = {{dx, sz.height - (k + 1) / 2.0}, {sz.width - 2 * dx, 0.5}};
        HIShapeUnionWithRect(shape, &top);
        HIShapeUnionWithRect(shape, &bot);
    }
    CGRect mid = {{0, RADIUS}, {sz.width, sz.height - 2 * RADIUS}};
    HIShapeUnionWithRect(shape, &mid);
    return shape;
}

// A translucent, mouse-transparent window at (x, y) of the given size, drawn at 2x.
// With blur the background is blurred and the window takes the panel's rounded shape.
static uint32_t make_window(CGPoint at, CGSize sz, bool blur, CGContextRef *out) {
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
    if (blur) {
        CFTypeRef shape = rounded_region(sz);
        SLSSetWindowShape(cid, w, 0, 0, shape);
        CFRelease(shape);
        SLSSetWindowBackgroundBlurRadius(cid, w, BLUR);
    }
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
    pal = appearance_dark() ? &DARK : &LIGHT;
    bool ok = true;
    for (int i = 0; i < NFONTS; i++) ok = (fonts[i] = make_font(font_specs[i].size, font_specs[i].weight)) && ok;
    if (!ok) {
        close_panel();
        return;
    }
    rows = n < MAX_ROWS ? n : MAX_ROWS;
    top = sel < rows ? 0 : sel - rows + 1;
    pos = from_pos = sel;
    size = CGSizeMake(fit_width(), HEADER_H + rows * ROW_H + ROW_INSET);
    CGRect d = CGDisplayBounds(CGMainDisplayID());
    origin = CGPointMake(d.origin.x + (d.size.width - size.width) / 2, d.origin.y + (d.size.height - size.height) / 2);
    hl_last = sel - top == rows - 1;
    wid = make_window(origin, size, true, &ctx);
    CGPoint o = hl_origin();
    hwid = wid ? make_window(o, hl_size(), false, &hctx) : 0;
    if (!hwid || !ctx || !hctx) {
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
    bool last = sel - top == rows - 1;
    if (last != hl_last) {
        hl_last = last;
        draw_highlight(hctx);
        CGContextFlush(hctx);
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
