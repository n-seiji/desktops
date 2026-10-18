#include "../src/glyph.h"
#include <stdio.h>
#include <string.h>

static int failed;
static uint8_t img[GLYPH_PX * GLYPH_PX * 4];

static uint8_t *at(int x, int y) {
    return img + (y * GLYPH_PX + x) * 4;
}

static void fill(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            uint8_t *p = at(x, y);
            p[0] = (uint8_t)(r * a / 255);
            p[1] = (uint8_t)(g * a / 255);
            p[2] = (uint8_t)(b * a / 255);
            p[3] = a;
        }
}

static void alpha(const char *what, int x, int y, int want) {
    uint8_t *p = at(x, y);
    if (p[0] == want && p[1] == want && p[2] == want && p[3] == want) return;
    printf("FAIL glyph_cut(%s) at (%d,%d) = %d %d %d %d; want %d in every channel\n", what, x, y, p[0], p[1], p[2], p[3], want);
    failed++;
}

int main(void) {
    // A blue tile with a white square: the tile disappears, the symbol stays opaque.
    memset(img, 0, sizeof img);
    fill(0, 0, GLYPH_PX, GLYPH_PX, 0x33, 0x66, 0xcc, 255);
    fill(14, 14, 26, 26, 255, 255, 255, 255);
    glyph_cut(img);
    alpha("tile", 5, 5, 0);
    alpha("symbol", 20, 20, 255);
    alpha("symbol corner", 14, 14, 255);
    alpha("tile next to the symbol", 13, 13, 0);
    alpha("outer rim", 0, 0, 0);

    // The most common colour is the background even when it is white: a dark symbol on white.
    memset(img, 0, sizeof img);
    fill(0, 0, GLYPH_PX, GLYPH_PX, 255, 255, 255, 255);
    fill(10, 10, 30, 30, 0x20, 0x20, 0x20, 255);
    glyph_cut(img);
    alpha("white tile", 5, 5, 0);
    alpha("dark symbol", 20, 20, 255);

    // Colours close to the background fade in: distance 120 from a 0x66 grey is 95/255 opaque.
    memset(img, 0, sizeof img);
    fill(0, 0, GLYPH_PX, GLYPH_PX, 0x66, 0x66, 0x66, 255);
    fill(16, 16, 24, 24, 0x8e, 0x8e, 0x8e, 255);
    glyph_cut(img);
    alpha("faint symbol", 20, 20, 95);
    alpha("background grey", 8, 8, 0);

    // The rim of an icon with transparent corners is cut away (2px), the interior is kept.
    memset(img, 0, sizeof img);
    fill(4, 4, 36, 36, 0x33, 0x66, 0xcc, 255);
    fill(10, 10, 30, 30, 255, 255, 255, 255);
    fill(4, 4, 8, 8, 255, 255, 255, 255);
    glyph_cut(img);
    alpha("transparent corner", 1, 1, 0);
    alpha("white on the tile's outermost pixel", 4, 4, 0);
    alpha("white on the pixel behind the 2px rim", 6, 6, 255);
    alpha("white right at the rim edge", 5, 5, 0);
    alpha("symbol in the middle", 20, 20, 255);

    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
