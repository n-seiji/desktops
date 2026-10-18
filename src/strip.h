#pragma once
// Geometry of the menu bar strip: one square slot per glyph, a "+k" slot after them when windows
// do not fit, and the desktop's key box last. The strip is centred in the status item's button.
#include <stdbool.h>

enum { STRIP_SLOT = 20, STRIP_GAP = 4 }; // points

// Slots that hold a window glyph or the "+k" count, without the key box.
int strip_slots(int n, int over);
// Width in points of a strip with that many slots before the key box.
int strip_width(int slots);

typedef struct {
    int slot;      // the glyph under the pointer, or -1
    bool overflow; // the "+k" slot is under the pointer
} strip_hit_t;
// What is at x (points from the left edge of the button, whose width is button_w) in a strip of n
// glyphs and, when over > 0, a "+k" slot. Gaps, the key box and everything outside hit nothing.
strip_hit_t strip_hit(double x, double button_w, int n, int over);
// Left edge of the "+k" slot in button coordinates.
double strip_overflow_left(double button_w, int n, int over);
