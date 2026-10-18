#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "jd.h"

// Optional menu bar item: one line-art glyph per app on the current desktop, then the
// desktop's key in an outlined box at the right end. The item grows and shrinks to the
// left, so the key stays where it is. The focused app's glyph is cut out of a filled tile.
// The image is a template, so macOS tints it for light and dark menu bars.

enum {
    MAX_APPS = 64,
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
static glyph_t *glyphs;
static int nglyphs;

static int last_ws;
static int last_n = -1;
static pid_t last_pids[MAX_APPS];
static pid_t last_front;

@interface JDMenuTarget : NSObject
@end

@implementation JDMenuTarget
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

static CGImageRef mask_for(pid_t pid) {
    for (int i = 0; i < nglyphs; i++)
        if (glyphs[i].pid == pid) return glyphs[i].mask;
    glyph_t *n = realloc(glyphs, sizeof *glyphs * (nglyphs + 1));
    if (!n) return NULL;
    glyphs = n;
    CGImageRef icon = apps_icon(pid, GLYPH_PX);
    CGImageRef mask = icon ? glyph_mask(icon) : NULL;
    if (icon) CGImageRelease(icon);
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

static void render(int ws, const pid_t *pids, int n, pid_t front) {
    int width = (n + 1) * SLOT + n * GAP;
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
        draw_app(c, slot, mask_for(pids[i]), pids[i] == front);
    }
    draw_key(c, n * (SLOT + GAP), config_key_char(ws - 1));

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
    NSMenu *menu = [NSMenu new];
    NSMenuItem *quit = [menu addItemWithTitle:@"Quit just-desktops" action:@selector(quit:) keyEquivalent:@""];
    quit.target = target;
    item = [NSStatusBar.systemStatusBar statusItemWithLength:NSVariableStatusItemLength];
    item.menu = menu;
}

void menubar_update(int ws, const pid_t *pids, int n, pid_t front) {
    if (!item) return;
    if (ws == last_ws && n == last_n && front == last_front && !memcmp(pids, last_pids, sizeof *pids * n)) return;
    last_ws = ws;
    last_n = n;
    last_front = front;
    memcpy(last_pids, pids, sizeof *pids * n);
    @autoreleasepool {
        render(ws, pids, n, front);
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
