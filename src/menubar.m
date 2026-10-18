#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "jd.h"

// Optional menu bar item: one line-art glyph per window on the current desktop (an app's
// windows side by side), then the desktop's key in an outlined box at the right end. The
// item grows and shrinks to the left, so the key stays where it is. The focused window's
// glyph is cut out of a filled tile. Past MAX_SLOTS windows the last slot reads "+k".
// The image is a template, so macOS tints it for light and dark menu bars.

enum {
    MAX_WINS = 64,
    MAX_SLOTS = 10,  // glyph slots; more windows than that turn the last slot into "+k"
    SCALE = 2,       // pixels per point
    HEIGHT = 22,     // points, the status item's thickness
    SLOT = 20,       // key box and glyph tile size
    PAD = 1,         // tile edge to glyph
    GAP = 4,
    GLYPH_PX = 40,
    RIM = 2,         // pixels shaved off the icon's outline
    LO = 75,         // colour distance from the tile where the glyph starts to show...
    HI = 195,        // ...and where it is fully opaque
};

typedef struct {
    pid_t pid;
    CGImageRef mask; // NULL when the app has no icon
} glyph_t;

static NSStatusItem *item;
static NSMenu *menu;
static glyph_t *glyphs;
static int nglyphs;

static int last_ws;
static int last_n = -1;
static int last_over;
static pid_t last_pids[MAX_WINS];
static uint32_t last_wids[MAX_WINS];
static uint32_t last_front;

// Index of the window slot under x (points from the left edge of the strip), or -1 for a gap,
// the overflow slot, the key box or outside.
static int slot_at(CGFloat x, int n) {
    if (x < 0) return -1;
    int i = (int)(x / (SLOT + GAP));
    return i < n && x - i * (SLOT + GAP) < SLOT ? i : -1;
}

@interface JDMenuTarget : NSObject
- (void)clicked:(id)sender;
- (void)quit:(id)sender;
@end

@implementation JDMenuTarget
- (void)clicked:(id)sender {
    NSButton *b = sender;
    NSEvent *e = NSApp.currentEvent;
    if (!e) return;
    if (e.type == NSEventTypeRightMouseUp || e.modifierFlags & NSEventModifierFlagControl) {
        [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(0, b.bounds.size.height + 6) inView:b];
        return;
    }
    if (last_n <= 0) return;
    int slots = last_n + (last_over > 0);
    CGFloat strip = (slots + 1) * SLOT + slots * GAP;
    CGFloat x = [b convertPoint:e.locationInWindow fromView:nil].x - (b.bounds.size.width - strip) / 2;
    int i = slot_at(x, last_n);
    if (i >= 0) wm_focus_wid(last_wids[i]);
}
- (void)quit:(id)sender {
    (void)sender;
    kill(getpid(), SIGTERM); // the same path as a signal: parked windows come back first
}
@end

static JDMenuTarget *target;

// White glyph for an app icon: the tile behind the symbol disappears. The tile is the
// icon's most common colour; a pixel's alpha is how far its colour is from that
// background, so only the symbol's strokes and shapes stay.
static CGImageRef glyph_mask(CGImageRef icon) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef c = CGBitmapContextCreate(NULL, GLYPH_PX, GLYPH_PX, 8, GLYPH_PX * 4, cs,
                                           (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    if (!c) return NULL;
    CGContextDrawImage(c, CGRectMake(0, 0, GLYPH_PX, GLYPH_PX), icon);
    uint8_t *px = CGBitmapContextGetData(c);

    enum { N = GLYPH_PX * GLYPH_PX, BINS = 4096 };
    static uint16_t votes[BINS];
    memset(votes, 0, sizeof votes);
    int best = 0;
    for (int i = 0; i < N; i++) {
        const uint8_t *p = px + i * 4;
        if (p[3] < 250) continue;
        int bin = (p[0] >> 4) << 8 | (p[1] >> 4) << 4 | p[2] >> 4;
        if (++votes[bin] > votes[best]) best = bin;
    }
    int bg[3] = {(best >> 8 & 15) * 17, (best >> 4 & 15) * 17, (best & 15) * 17};

    // The tile's own rim (bevel, shadow, antialiased edge) is not part of the symbol:
    // keep only pixels that are RIM pixels away from anything transparent.
    static uint8_t inner[N], tmp[N];
    for (int i = 0; i < N; i++) inner[i] = px[i * 4 + 3];
    for (int pass = 0; pass < RIM; pass++) {
        for (int y = 0; y < GLYPH_PX; y++)
            for (int x = 0; x < GLYPH_PX; x++) {
                uint8_t m = inner[y * GLYPH_PX + x];
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int u = x + dx, v = y + dy;
                        uint8_t o = u < 0 || v < 0 || u >= GLYPH_PX || v >= GLYPH_PX ? 0 : inner[v * GLYPH_PX + u];
                        if (o < m) m = o;
                    }
                tmp[y * GLYPH_PX + x] = m;
            }
        memcpy(inner, tmp, N);
    }

    for (int i = 0; i < N; i++) {
        uint8_t *p = px + i * 4;
        int a = p[3], out = 0;
        if (a) {
            int d = 0;
            for (int k = 0; k < 3; k++) {
                int v = p[k] * 255 / a; // un-premultiply
                d += abs(v - bg[k]);
            }
            int t = (d - LO) * 255 / (HI - LO); // soft threshold
            t = t < 0 ? 0 : t > 255 ? 255 : t;
            out = t * inner[i] / 255;
        }
        p[0] = p[1] = p[2] = p[3] = out;
    }
    CGImageRef m = CGBitmapContextCreateImage(c);
    CGContextRelease(c);
    return m;
}

// Line art for icons whose colours have no single background to cut away: the mask built
// from the icon would be a blob, so these are drawn by hand into the same mask format.
static void draw_chrome(CGContextRef c) {
    CGFloat o = 14, ring = 7, disc = 5.2, mid = GLYPH_PX / 2.0;
    CGContextSetGrayFillColor(c, 1, 1);
    CGContextSetGrayStrokeColor(c, 1, 1);
    CGContextAddEllipseInRect(c, CGRectMake(mid - o, mid - o, 2 * o, 2 * o));
    CGContextAddEllipseInRect(c, CGRectMake(mid - ring, mid - ring, 2 * ring, 2 * ring));
    CGContextEOFillPath(c);
    CGContextFillEllipseInRect(c, CGRectMake(mid - disc, mid - disc, 2 * disc, 2 * disc));
    CGContextSetBlendMode(c, kCGBlendModeClear);
    CGContextSetLineWidth(c, 1.4);
    for (int k = 0; k < 3; k++) {
        CGFloat a = (30 + 120 * k) * M_PI / 180;
        CGContextMoveToPoint(c, mid + (ring - 0.5) * cos(a), mid + (ring - 0.5) * sin(a));
        CGContextAddLineToPoint(c, mid + (o + 1) * cos(a), mid + (o + 1) * sin(a));
    }
    CGContextStrokePath(c);
}

static const struct {
    const char *bundle;
    void (*draw)(CGContextRef);
} art[] = {
    {"com.google.Chrome", draw_chrome},
};

static CGImageRef drawn_mask(NSString *bundle) {
    for (size_t i = 0; i < sizeof art / sizeof *art; i++) {
        if (![bundle isEqualToString:@(art[i].bundle)]) continue;
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef c = CGBitmapContextCreate(NULL, GLYPH_PX, GLYPH_PX, 8, GLYPH_PX * 4, cs,
                                               (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
        CGColorSpaceRelease(cs);
        if (!c) return NULL;
        art[i].draw(c);
        CGImageRef m = CGBitmapContextCreateImage(c);
        CGContextRelease(c);
        return m;
    }
    return NULL;
}

static CGImageRef mask_for(pid_t pid) {
    for (int i = 0; i < nglyphs; i++)
        if (glyphs[i].pid == pid) return glyphs[i].mask;
    glyph_t *n = realloc(glyphs, sizeof *glyphs * (nglyphs + 1));
    if (!n) return NULL;
    glyphs = n;
    NSString *bundle = [NSRunningApplication runningApplicationWithProcessIdentifier:pid].bundleIdentifier;
    CGImageRef mask = drawn_mask(bundle);
    if (!mask) {
        CGImageRef icon = apps_icon(pid, GLYPH_PX);
        mask = icon ? glyph_mask(icon) : NULL;
        if (icon) CGImageRelease(icon);
    }
    glyphs[nglyphs++] = (glyph_t){pid, mask};
    return mask;
}

// The key box sits in the slot at x; the glyph is centred by its real outline bounds, not
// the line box, and snapped to the pixel grid so it stays crisp at 1x and 2x.
static void draw_key(CGContextRef c, CGFloat x, char key) {
    CGRect box = CGRectMake(x + 0.5, (HEIGHT - SLOT) / 2.0 + 0.5, SLOT - 1, SLOT - 1);
    CGPathRef path = CGPathCreateWithRoundedRect(box, 4.5, 4.5, NULL);
    CGContextSetLineWidth(c, 1);
    CGContextAddPath(c, path);
    CGContextStrokePath(c);
    CGPathRelease(path);

    CTFontRef font = CTFontCreateUIFontForLanguage(kCTFontUIFontEmphasizedSystem, 13, NULL);
    CFStringRef text = CFStringCreateWithBytes(NULL, (const UInt8 *)&key, 1, kCFStringEncodingASCII, false);
    CFTypeRef keys[] = {kCTFontAttributeName, kCTForegroundColorFromContextAttributeName};
    CFTypeRef vals[] = {font, kCFBooleanTrue};
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, (const void **)keys, (const void **)vals, 2,
                                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef str = CFAttributedStringCreate(NULL, text, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(str);
    CGRect b = CTLineGetBoundsWithOptions(line, kCTLineBoundsUseGlyphPathBounds);
    CGFloat px = x + SLOT / 2.0 - CGRectGetMidX(b), py = HEIGHT / 2.0 - CGRectGetMidY(b);
    CGContextSetTextPosition(c, round(px * SCALE) / SCALE, round(py * SCALE) / SCALE);
    CTLineDraw(line, c);
    CFRelease(line);
    CFRelease(str);
    CFRelease(attrs);
    CFRelease(text);
    CFRelease(font);
}

static void draw_more(CGContextRef c, CGFloat x, int k) {
    char buf[8];
    snprintf(buf, sizeof buf, "+%d", k > 99 ? 99 : k);
    CTFontRef font = CTFontCreateUIFontForLanguage(kCTFontUIFontEmphasizedSystem, 11, NULL);
    CFStringRef text = CFStringCreateWithCString(NULL, buf, kCFStringEncodingASCII);
    CFTypeRef keys[] = {kCTFontAttributeName, kCTForegroundColorFromContextAttributeName};
    CFTypeRef vals[] = {font, kCFBooleanTrue};
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, (const void **)keys, (const void **)vals, 2,
                                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef str = CFAttributedStringCreate(NULL, text, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(str);
    CGRect b = CTLineGetBoundsWithOptions(line, kCTLineBoundsUseGlyphPathBounds);
    CGFloat px = x + SLOT / 2.0 - CGRectGetMidX(b), py = HEIGHT / 2.0 - CGRectGetMidY(b);
    CGContextSetTextPosition(c, round(px * SCALE) / SCALE, round(py * SCALE) / SCALE);
    CTLineDraw(line, c);
    CFRelease(line);
    CFRelease(str);
    CFRelease(attrs);
    CFRelease(text);
    CFRelease(font);
}

static void draw_app(CGContextRef c, CGRect slot, CGImageRef mask, bool focused) {
    CGRect art = CGRectInset(slot, PAD, PAD);
    if (focused) {
        CGPathRef tile = CGPathCreateWithRoundedRect(slot, 5, 5, NULL);
        CGContextAddPath(c, tile);
        CGContextFillPath(c);
        CGPathRelease(tile);
        CGContextSetBlendMode(c, kCGBlendModeDestinationOut);
    }
    if (mask) CGContextDrawImage(c, art, mask);
    CGContextSetBlendMode(c, kCGBlendModeNormal);
}

static void render(int ws, const pid_t *pids, const uint32_t *wids, int n, int over, uint32_t front) {
    int slots = n + (over > 0);
    int width = (slots + 1) * SLOT + slots * GAP;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef c = CGBitmapContextCreate(NULL, width * SCALE, HEIGHT * SCALE, 8, 0, cs,
                                           (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    if (!c) return;
    CGContextScaleCTM(c, SCALE, SCALE);
    CGContextSetInterpolationQuality(c, kCGInterpolationHigh);
    CGContextSetGrayFillColor(c, 1, 1);
    CGContextSetGrayStrokeColor(c, 1, 1);

    for (int i = 0; i < n; i++) {
        CGRect slot = CGRectMake(i * (SLOT + GAP), (HEIGHT - SLOT) / 2.0, SLOT, SLOT);
        draw_app(c, slot, mask_for(pids[i]), wids[i] == front);
    }
    if (over > 0) draw_more(c, n * (SLOT + GAP), over);
    draw_key(c, slots * (SLOT + GAP), config_key_char(ws - 1));

    CGImageRef strip = CGBitmapContextCreateImage(c);
    CGContextRelease(c);
    NSImage *img = [[NSImage alloc] initWithCGImage:strip size:NSMakeSize(width, HEIGHT)];
    CGImageRelease(strip);
    img.template = YES;
    item.button.image = img;
}


void menubar_init(void) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    target = [JDMenuTarget new];
    menu = [NSMenu new];
    NSMenuItem *quit = [menu addItemWithTitle:@"Quit just-desktops" action:@selector(quit:) keyEquivalent:@""];
    quit.target = target;
    item = [NSStatusBar.systemStatusBar statusItemWithLength:NSVariableStatusItemLength];
    // No permanent menu: it would swallow left clicks. It opens on a right or control click.
    item.button.target = target;
    item.button.action = @selector(clicked:);
    [item.button sendActionOn:NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp];
}

// n windows get a glyph (pids[i] owns wids[i]); over more are only counted. front is the
// focused window's id, 0 for none.
void menubar_update(int ws, const pid_t *pids, const uint32_t *wids, int n, int over, uint32_t front) {
    if (!item) return;
    if (n > MAX_WINS) n = MAX_WINS;
    if (ws == last_ws && n == last_n && over == last_over && front == last_front &&
        !memcmp(pids, last_pids, sizeof *pids * n) && !memcmp(wids, last_wids, sizeof *wids * n))
        return;
    last_ws = ws;
    last_n = n;
    last_over = over;
    last_front = front;
    memcpy(last_pids, pids, sizeof *pids * n);
    memcpy(last_wids, wids, sizeof *wids * n);
    @autoreleasepool {
        render(ws, pids, wids, n, over, front);
    }
}

void menubar_forget(pid_t pid) {
    for (int i = 0; i < nglyphs; i++) {
        if (glyphs[i].pid != pid) continue;
        if (glyphs[i].mask) CGImageRelease(glyphs[i].mask);
        glyphs[i] = glyphs[--nglyphs];
        return;
    }
}
