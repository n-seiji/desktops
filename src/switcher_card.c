#include "swkit.h"
#include "extern.h"
#include <dispatch/dispatch.h>
#include <malloc/malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Card switchers (square and peek). One glass square on the main display with one window shown
// large. Like the list panel, every large surface is drawn once per open: the stage is flushed
// once, each card is its own small window that is moved (and for peek scaled and faded) while it
// animates, and the counter and the dots (square only) are tiny windows redrawn per step. Everything exists
// only while ⌥ is held after a Tab; the animation timer runs only while cards move.
//
// The windows of the entries form a strip with unwrapped positions v (the entry is v mod n).
// A card holds one position and sits at p = v - sel_v slots from the centre. Square slides a
// card a short way while it fades, so it never hangs far outside the stage: SkyLight does not
// clip a window's drawn content to SLSSetWindowShape, and a shape change blanks the window
// until it is flushed again. Peek puts the neighbours behind the stage at a smaller size, so they
// peek out on both sides; it has no dots.

#define STAGE_W 640.0
#define STAGE_H_PEEK 460.0 // with the caption row
#define STAGE_H_SQUARE 420.0 // no caption, with the dots
#define STAGE_R 40.0
#define BLUR 30
#define THUMB_W 576.0
#define THUMB_H 330.0
#define THUMB_R 12.0
#define THUMB_X 32.0 // the thumbnail box in the stage
#define THUMB_Y 56.0
#define CARD_PADX 16.0 // room around the thumbnail for its shadow
#define CARD_PADT 12.0
#define CARD_W (THUMB_W + 2 * CARD_PADX)
#define CARD_H_PEEK 392.0
#define CARD_H_SQUARE 362.0 // the thumbnail and its shadow
#define CARD_X (THUMB_X - CARD_PADX) // the card window's origin in the stage
#define CARD_Y (THUMB_Y - CARD_PADT)
#define CAP_CY (410.0 - CARD_Y) // the caption's centre line in the card
#define ICON 28.0
#define ICON_GAP 12.0
#define PLACE_ICON 128.0
#define INSET 32.0
#define HEADER_CY 32.0
#define DOT 7.0
#define DOT_GAP 8.0
#define PILL 22.0
#define DOTS_CY 403.5
#define MAX_DOTS 12
#define COUNTER_W 80.0
#define COUNTER_H 20.0
#define SQUARE_DX 120.0 // how far a square card slides before it has faded out
#define SQUARE_FADE 1.6
#define PEEK_DX 480.0
#define PEEK_SCALE 0.68
#ifndef MOVE_MS
#define MOVE_MS 340.0
#endif
#define MAX_CARDS 4
#define CARD_RES 1 // the cards are drawn at 1x: 2x would add ~15MB at peak (the stage, the counter and the dots stay sharp)

enum { F_HEADER, F_CAPTION, NFONTS };
static const struct { CGFloat size, weight; } font_specs[NFONTS] = {
    {14, 0.23}, {19, 0.3},
};

typedef struct {
    double x, s, a; // offset from the stage centre in pt, scale, alpha
} pose_t;

typedef struct {
    uint32_t wid;
    CGContextRef ctx;
    bool used;
    long v;
    double from, to; // position in slots, p
    CGImageRef img;  // the window's screenshot, only while it is being drawn
    bool shot;       // the screenshot is already in the card
} card_t;

static int cid;
static uint32_t stage, counter, dots;
static CGContextRef stage_ctx, counter_ctx, dots_ctx;
static card_t cards[MAX_CARDS];
static int pool;
static sw_entry_t *ents;
static int n;
static long sel_v;
static int last_dir;
static bool peek, thumbs, dark;
static unsigned open_gen;
static palette_t pal;
static CTFontRef fonts[NFONTS];
static CGPoint so; // the stage's origin
static dispatch_source_t timer;
static double t0;

static CGFloat stage_h(void) {
    return peek ? STAGE_H_PEEK : STAGE_H_SQUARE;
}

static CGFloat card_h(void) {
    return peek ? CARD_H_PEEK : CARD_H_SQUARE;
}

static int idx_of(long v) {
    return (int)(((v % n) + n) % n);
}

// cubic-bezier(0.22, 0.9, 0.25, 1): most of the travel happens in the first 80ms.
static double bez(double a, double b, double t) {
    double u = 1 - t;
    return 3 * u * u * t * a + 3 * u * t * t * b + t * t * t;
}

static double ease(double k) {
    double lo = 0, hi = 1, t = k;
    for (int i = 0; i < 24; i++) {
        double x = bez(0.22, 0.25, t);
        if (x < k) lo = t;
        else hi = t;
        t = (lo + hi) / 2;
    }
    return bez(0.9, 1, t);
}

static pose_t pose(double p) {
    if (!peek) return (pose_t){p * SQUARE_DX, 1, fmax(0, 1 - SQUARE_FADE * fabs(p))};
    double ap = fabs(p), near = fmin(ap, 1);
    double a = fmin(1, fmax(0, 2 - ap)); // opaque, until it leaves past the neighbour slot
    return (pose_t){p * PEEK_DX, 1 - (1 - PEEK_SCALE) * near, a};
}

static void begin(CGContextRef c, CGSize sz) {
    CGContextClearRect(c, (CGRect){{0, 0}, sz});
    CGContextSaveGState(c);
    CGContextTranslateCTM(c, 0, sz.height);
    CGContextScaleCTM(c, 1, -1);
    CGContextSetTextMatrix(c, CGAffineTransformMakeScale(1, -1));
}

static void finish(CGContextRef c) {
    CGContextRestoreGState(c);
    CGContextFlush(c);
}

static uint32_t dim_text(void) {
    return sw_mix(pal.title, pal.tint, 0.45);
}

static void draw_stage(void) {
    CGSize sz = {STAGE_W, stage_h()};
    begin(stage_ctx, sz);
    sw_fill_rrect(stage_ctx, (CGRect){{0, 0}, sz}, STAGE_R, pal.tint, pal.tint_a);
    sw_edge_rrect(stage_ctx, (CGRect){{0, 0}, sz}, STAGE_R, &pal);
    char buf[32];
    snprintf(buf, sizeof buf, "Desktop %d", wm_current());
    CFStringRef s = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    sw_put(stage_ctx, sw_fit(fonts[F_HEADER], s, dim_text(), STAGE_W / 2, kCTLineTruncationEnd), INSET, HEADER_CY);
    CFRelease(s);
    finish(stage_ctx);
}

static void draw_counter(void) {
    CGSize sz = {COUNTER_W, COUNTER_H};
    begin(counter_ctx, sz);
    char buf[32];
    snprintf(buf, sizeof buf, "%d / %d", idx_of(sel_v) + 1, n);
    CFStringRef s = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    CTLineRef l = sw_fit(fonts[F_HEADER], s, dim_text(), COUNTER_W, kCTLineTruncationEnd);
    CGFloat w = CTLineGetTypographicBounds(l, NULL, NULL, NULL);
    sw_put(counter_ctx, l, COUNTER_W - w, COUNTER_H / 2);
    CFRelease(s);
    finish(counter_ctx);
}

static CGFloat dots_w(void) {
    return MAX_DOTS * (DOT + DOT_GAP) + PILL;
}

static void draw_dots(void) {
    CGSize sz = {dots_w(), DOT + 4};
    begin(dots_ctx, sz);
    int nd = n < MAX_DOTS ? n : MAX_DOTS, at = idx_of(sel_v);
    int first = n <= MAX_DOTS ? 0 : at - MAX_DOTS / 2;
    if (first < 0) first = 0;
    if (first > n - nd) first = n - nd;
    CGFloat x = (sz.width - ((nd - 1) * (DOT + DOT_GAP) + PILL)) / 2, y = (sz.height - DOT) / 2;
    for (int i = first; i < first + nd; i++) {
        bool on = i == at;
        CGFloat w = on ? PILL : DOT;
        sw_fill_rrect(dots_ctx, (CGRect){{x, y}, {w, DOT}}, DOT / 2, on ? pal.title : dim_text(), 1);
        x += w + DOT_GAP;
    }
    finish(dots_ctx);
}

static void stroke_rrect(CGContextRef c, CGRect r, CGFloat radius, uint32_t rgb, CGFloat a) {
    CGPathRef p = CGPathCreateWithRoundedRect(CGRectInset(r, 0.5, 0.5), radius - 0.5, radius - 0.5, NULL);
    sw_color(c, rgb, a, true);
    CGContextSetLineWidth(c, 1);
    CGContextAddPath(c, p);
    CGContextStrokePath(c);
    CGPathRelease(p);
}

static void draw_image(CGContextRef c, CGImageRef img, CGRect r, bool round) {
    CGContextSaveGState(c);
    if (round) {
        CGPathRef p = CGPathCreateWithRoundedRect(r, THUMB_R, THUMB_R, NULL);
        CGContextAddPath(c, p);
        CGContextClip(c);
        CGPathRelease(p);
    }
    CGContextTranslateCTM(c, r.origin.x, r.origin.y + r.size.height);
    CGContextScaleCTM(c, 1, -1);
    CGContextDrawImage(c, (CGRect){{0, 0}, r.size}, img);
    CGContextRestoreGState(c);
}

// The thumbnail (or the app icon until the screenshot is there) and, for peek, the caption.
static void draw_card(card_t *k) {
    const sw_entry_t *e = &ents[idx_of(k->v)];
    CGContextRef c = k->ctx;
    CGSize sz = {CARD_W, card_h()};
    begin(c, sz);
    CGRect box = {{CARD_PADX, CARD_PADT}, {THUMB_W, THUMB_H}};
    if (k->img) {
        CGFloat iw = CGImageGetWidth(k->img), ih = CGImageGetHeight(k->img), f = fmin(THUMB_W / iw, THUMB_H / ih);
        CGRect r = {{0, 0}, {iw * f, ih * f}};
        r.origin = CGPointMake(box.origin.x + (THUMB_W - r.size.width) / 2, box.origin.y + (THUMB_H - r.size.height) / 2);
        CGContextSaveGState(c);
        CGColorRef sh = CGColorCreateGenericGray(0, 0.4);
        CGContextSetShadowWithColor(c, CGSizeMake(0, -6), 14, sh);
        CGColorRelease(sh);
        sw_fill_rrect(c, r, THUMB_R, 0x000000, 1);
        CGContextRestoreGState(c);
        draw_image(c, k->img, r, true);
        stroke_rrect(c, r, THUMB_R, 0xffffff, 0.12);
    } else {
        sw_fill_rrect(c, box, THUMB_R, dark ? 0xffffff : 0x000000, dark ? 0.06 : 0.05);
        stroke_rrect(c, box, THUMB_R, dark ? 0xffffff : 0x000000, 0.12);
        CGImageRef big = apps_icon(e->pid, (int)(PLACE_ICON * 2));
        if (big) {
            draw_image(c, big, (CGRect){{box.origin.x + (THUMB_W - PLACE_ICON) / 2, box.origin.y + (THUMB_H - PLACE_ICON) / 2},
                                        {PLACE_ICON, PLACE_ICON}}, false);
            CGImageRelease(big);
        }
    }
    if (peek) {
        CFStringRef text = sw_text(e);
        uint32_t rgb = 0xffffff; // the neighbours sit over the desktop
        CTLineRef l = text ? sw_fit(fonts[F_CAPTION], text, rgb, THUMB_W - ICON - ICON_GAP,
                                    sw_is_path(text) ? kCTLineTruncationStart : kCTLineTruncationEnd) : NULL;
        CGFloat tw = l ? CTLineGetTypographicBounds(l, NULL, NULL, NULL) : 0;
        CGFloat x = CARD_PADX + (THUMB_W - (ICON + ICON_GAP + tw)) / 2;
        if (e->icon) draw_image(c, e->icon, (CGRect){{x, CAP_CY - ICON / 2}, {ICON, ICON}}, false);
        if (l) {
            CGColorRef sh = CGColorCreateGenericGray(0, dark ? 0.55 : 0.7);
            CGContextSetShadowWithColor(c, CGSizeMake(0, -1), 4, sh);
            CGColorRelease(sh);
            sw_put(c, l, x + ICON + ICON_GAP, CAP_CY);
        }
    }
    finish(c);
}

static void request_thumb(int ci) {
    if (!thumbs) return;
    long v = cards[ci].v;
    unsigned gen = open_gen;
    thumb_capture(ents[idx_of(v)].wid, ^(CGImageRef img) {
        card_t *k = &cards[ci];
        if (gen != open_gen || !img || !k->used || k->v != v || k->shot) return;
        k->img = CGImageRetain(img);
        draw_card(k);
        CGImageRelease(k->img);
        k->img = NULL;
        k->shot = true;
    });
}

static void set_alpha(card_t *k, double a) {
    SLSSetWindowAlpha(cid, k->wid, (float)a);
}

// Moves card k to p slots from the centre and applies the pose.
static void place(card_t *k, double p) {
    pose_t q = pose(p);
    CGPoint o = {so.x + CARD_X + q.x, so.y + CARD_Y};
    if (!peek) {
        SLSMoveWindow(cid, k->wid, &o);
        set_alpha(k, q.a);
        return;
    }
    // The card scales about the stage's centre, so its window centre follows scaled.
    CGPoint centre = {so.x + STAGE_W / 2 + q.x, so.y + STAGE_H_PEEK / 2 + q.s * (CARD_Y + CARD_H_PEEK / 2 - STAGE_H_PEEK / 2)};
    CGFloat is = 1 / q.s, tx = CARD_W / 2 - centre.x * is, ty = CARD_H_PEEK / 2 - centre.y * is;
    SLSSetWindowTransform(cid, k->wid, CGAffineTransformMake(is, 0, 0, is, tx, ty));
    set_alpha(k, q.a);
}

static void stop_timer(void) {
    if (!timer) return;
    dispatch_source_cancel(timer);
    dispatch_release(timer);
    timer = NULL;
}

static void release_card(card_t *k) {
    if (k->wid) {
        SLSOrderWindow(cid, k->wid, 0, 0);
        if (k->ctx) CGContextRelease(k->ctx);
        SLSReleaseWindow(cid, k->wid);
    }
    if (k->img) CGImageRelease(k->img);
    *k = (card_t){0};
}

static void close_cards(void) {
    stop_timer();
    thumb_end();
    open_gen++;
    for (int i = 0; i < MAX_CARDS; i++) release_card(&cards[i]);
    uint32_t *ws[] = {&stage, &counter, &dots};
    CGContextRef *cs[] = {&stage_ctx, &counter_ctx, &dots_ctx};
    for (int i = 0; i < 3; i++) {
        if (!*ws[i]) continue;
        SLSOrderWindow(cid, *ws[i], 0, 0);
        if (*cs[i]) CGContextRelease(*cs[i]);
        SLSReleaseWindow(cid, *ws[i]);
        *ws[i] = 0;
        *cs[i] = NULL;
    }
    for (int i = 0; i < NFONTS; i++) {
        if (fonts[i]) CFRelease(fonts[i]);
        fonts[i] = NULL;
    }
    sw_free_entries(ents, n);
    ents = NULL;
    n = 0;
    malloc_zone_pressure_relief(NULL, 0);
}

static void tick(void) {
    double k = (CFAbsoluteTimeGetCurrent() - t0) * 1000 / MOVE_MS;
    bool done = k >= 1;
    double e = done ? 1 : ease(k);
    SLSDisableUpdate(cid);
    for (int i = 0; i < pool; i++) {
        card_t *c = &cards[i];
        if (!c->used) continue;
        place(c, c->from + (c->to - c->from) * e);
        if (done) c->from = c->to;
    }
    SLSReenableUpdate(cid);
    if (done) stop_timer();
}

// The card windows' positions that must exist with sel_v selected, into out. Peek shows the two
// neighbours and keeps the card that just left, square only the card that just left.
static int needed(long *out) {
    int m = 0;
    out[m++] = sel_v;
    if (n < 2) return m;
    if (peek) {
        out[m++] = sel_v - 1;
        out[m++] = sel_v + 1;
        if (last_dir) out[m++] = sel_v - 2 * last_dir;
    } else if (last_dir) {
        out[m++] = sel_v - last_dir;
    }
    return m;
}

// Peek draws the neighbours behind the stage so they peek out from under it; the selected card is on
// top. Re-ordering a window below the stage does not take, so it goes by window level.
static void order(card_t *k) {
    int base = CGWindowLevelForKey(kCGPopUpMenuWindowLevelKey);
    if (peek) SLSSetWindowLevel(cid, k->wid, k->v == sel_v ? base + 1 : base - 1);
    SLSOrderWindow(cid, k->wid, 1, stage);
}

static card_t *new_card(long v) {
    for (int i = 0; i < pool; i++) {
        card_t *k = &cards[i];
        if (k->used) continue;
        if (!k->wid) {
            k->wid = sw_window(cid, so, (CGSize){CARD_W, card_h()}, 0, 0, CARD_RES, &k->ctx);
            if (!k->wid || !k->ctx) {
                release_card(k);
                return NULL;
            }
            set_alpha(k, 0);
        }
        k->shot = false;
        k->used = true;
        k->v = v;
        order(k);
        draw_card(k);
        request_thumb(i);
        return k;
    }
    return NULL;
}

// Hands every needed position a card (reusing the ones that left) and aims every card at its slot.
// A surviving card keeps its from; a new one starts one slot beyond its target, on the side it enters from.
static void settle(bool animate) {
    long need[4];
    int m = needed(need);
    for (int i = 0; i < pool; i++) {
        card_t *k = &cards[i];
        bool keep = false;
        for (int j = 0; j < m && k->used && !keep; j++) keep = need[j] == k->v;
        if (k->used && !keep) {
            k->used = false;
            if (k->wid) set_alpha(k, 0);
        }
    }
    for (int j = 0; j < m; j++) {
        bool held = false;
        for (int i = 0; i < pool && !held; i++) held = cards[i].used && cards[i].v == need[j];
        if (held) continue;
        card_t *k = new_card(need[j]);
        if (k) k->from = (need[j] - sel_v) + last_dir;
    }
    for (int i = 0; i < pool; i++) {
        card_t *k = &cards[i];
        if (!k->used) continue;
        k->to = k->v - sel_v;
        if (!animate) k->from = k->to;
        if (k->wid) order(k);
    }
}

static void open_cards(int dir, bool is_peek) {
    int sel;
    n = wm_windows(dir, &ents, &sel);
    if (!n) {
        close_cards();
        return;
    }
    cid = SLSMainConnectionID();
    peek = is_peek;
    pool = peek ? MAX_CARDS : 2;
    dark = appearance_dark();
    pal = *sw_palette();
    if (dark) pal.tint_a = 0.82;
    bool ok = true;
    for (int i = 0; i < NFONTS; i++) ok = (fonts[i] = sw_font(font_specs[i].size, font_specs[i].weight)) && ok;
    CGRect d = CGDisplayBounds(CGMainDisplayID());
    so = CGPointMake(d.origin.x + (d.size.width - STAGE_W) / 2, d.origin.y + (d.size.height - stage_h()) / 2);
    if (ok) {
        stage = sw_window(cid, so, (CGSize){STAGE_W, stage_h()}, STAGE_R, BLUR, 2, &stage_ctx);
        counter = sw_window(cid, (CGPoint){so.x + STAGE_W - INSET - COUNTER_W, so.y + HEADER_CY - COUNTER_H / 2},
                            (CGSize){COUNTER_W, COUNTER_H}, 0, 0, 2, &counter_ctx);
        if (!peek)
            dots = sw_window(cid, (CGPoint){so.x + (STAGE_W - dots_w()) / 2, so.y + DOTS_CY - (DOT + 4) / 2},
                             (CGSize){dots_w(), DOT + 4}, 0, 0, 2, &dots_ctx);
    }
    if (!ok || !stage || !counter || !stage_ctx || !counter_ctx || (!peek && (!dots || !dots_ctx))) {
        close_cards();
        return;
    }
    sel_v = sel;
    last_dir = 0;
    thumbs = thumb_ready();
    if (thumbs) thumb_begin();
    draw_stage();
    draw_counter();
    if (dots) draw_dots();
    SLSOrderWindow(cid, stage, 1, 0);
    SLSOrderWindow(cid, counter, 1, stage);
    if (dots) SLSOrderWindow(cid, dots, 1, stage);
    settle(false);
    for (int i = 0; i < pool; i++)
        if (cards[i].used) place(&cards[i], cards[i].to);
}

void card_step(int dir, bool is_peek) {
    if (!stage) {
        open_cards(dir, is_peek);
        return;
    }
    if (n < 2) return;
    double k = timer ? (CFAbsoluteTimeGetCurrent() - t0) * 1000 / MOVE_MS : 1;
    double e = k >= 1 ? 1 : ease(k);
    for (int i = 0; i < pool; i++) {
        card_t *c = &cards[i];
        if (c->used) c->from += (c->to - c->from) * e;
    }
    sel_v += dir;
    last_dir = dir;
    draw_counter();
    if (dots) draw_dots();
    settle(true);
    SLSDisableUpdate(cid);
    for (int i = 0; i < pool; i++)
        if (cards[i].used) place(&cards[i], cards[i].from);
    SLSReenableUpdate(cid);
    t0 = CFAbsoluteTimeGetCurrent();
    if (timer) return;
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, NSEC_PER_SEC / 60, NSEC_PER_MSEC);
    dispatch_source_set_event_handler(timer, ^{ tick(); });
    dispatch_resume(timer);
}

void card_commit(void) {
    if (!stage) return;
    uint32_t target = ents[idx_of(sel_v)].wid;
    close_cards();
    wm_focus_wid(target);
}

void card_cancel(void) {
    if (stage) close_cards();
}
