#include "swkit.h"
#include "extern.h"
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

// The panel's width: the widest title plus the row's padding, within MIN_W..MAX_W.
static CGFloat fit_width(void) {
    CGFloat w = 0;
    for (int i = 0; i < n; i++) {
        CFStringRef s = sw_text(&ents[i]);
        if (!s) continue;
        CTLineRef l = sw_fit(fonts[F_ROW], s, 0, 1e9, kCTLineTruncationEnd);
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
    sw_put(c, sw_fit(fonts[F_HEADER], s, pal->header, size.width - 2 * (ROW_INSET + ROW_PAD), kCTLineTruncationEnd), ROW_INSET + ROW_PAD, 19);
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
    CFStringRef main = sw_text(e);
    if (!main) return;
    sw_put(c, sw_fit(fonts[F_ROW], main, pal->title, right - left, sw_is_path(main) ? kCTLineTruncationStart : kCTLineTruncationEnd), left, cy);
}

// All of the panel's looks live here. Below the first line the context is y-down.
static void draw(CGContextRef c) {
    CGRect all = {{0, 0}, size};
    CGContextClearRect(c, all);
    CGContextSaveGState(c);
    CGContextTranslateCTM(c, 0, size.height);
    CGContextScaleCTM(c, 1, -1);
    CGContextSetTextMatrix(c, CGAffineTransformMakeScale(1, -1));

    sw_fill_rrect(c, all, RADIUS, pal->tint, pal->tint_a);
    sw_edge_rrect(c, all, RADIUS, pal);

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
    sw_color(c, pal->highlight, pal->highlight_a, false);
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
    sw_free_entries(ents, n);
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

static void open_panel(int dir) {
    n = wm_windows(dir, &ents, &sel);
    if (!n) {
        close_panel();
        return;
    }
    cid = SLSMainConnectionID();
    pal = sw_palette();
    bool ok = true;
    for (int i = 0; i < NFONTS; i++) ok = (fonts[i] = sw_font(font_specs[i].size, font_specs[i].weight)) && ok;
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
    wid = sw_window(cid, origin, size, RADIUS, BLUR, &ctx);
    CGPoint o = hl_origin();
    hwid = wid ? sw_window(cid, o, hl_size(), 0, 0, &hctx) : 0;
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
    if (config_switcher_style() >= SW_SQUARE) {
        card_step(dir, config_switcher_style() == SW_PEEK);
        return;
    }
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
    if (config_switcher_style() >= SW_SQUARE) {
        card_commit();
        return;
    }
    if (!wid) return;
    uint32_t target = ents[sel].wid;
    close_panel();
    wm_focus_wid(target);
}

void switcher_cancel(void) {
    if (config_switcher_style() >= SW_SQUARE) {
        card_cancel();
        return;
    }
    if (wid) close_panel();
}
