#import <AppKit/AppKit.h>
#include "menubar.h"
#include "menubar_priv.h"
#include "apps.h"
#include "glyph.h"

// The glyph of each app on the strip: a white mask of its icon (see glyph_cut), or hand-drawn line art
// for icons that have no single background to cut away. Built when an app first needs one and kept
// until the app quits (menubar_forget).

typedef struct {
    pid_t pid;
    CGImageRef mask; // NULL when the app has no icon
} glyph_t;

static glyph_t *glyphs;
static int nglyphs;

// White glyph for an app icon: the tile behind the symbol disappears (see glyph_cut).
static CGImageRef glyph_mask(CGImageRef icon) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef c = CGBitmapContextCreate(NULL, GLYPH_PX, GLYPH_PX, 8, GLYPH_PX * 4, cs,
                                           (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    if (!c) return NULL;
    CGContextDrawImage(c, CGRectMake(0, 0, GLYPH_PX, GLYPH_PX), icon);
    glyph_cut(CGBitmapContextGetData(c));
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

CGImageRef menubar_mask(pid_t pid) {
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

void menubar_forget(pid_t pid) {
    for (int i = 0; i < nglyphs; i++) {
        if (glyphs[i].pid != pid) continue;
        if (glyphs[i].mask) CGImageRelease(glyphs[i].mask);
        glyphs[i] = glyphs[--nglyphs];
        return;
    }
}
