#include "../src/strip.h"
#include <stdio.h>

static int failed;

static void eq(const char *what, double got, double want) {
    if (got == want) return;
    printf("FAIL %s = %g; want %g\n", what, got, want);
    failed++;
}

static void hit(const char *what, double x, double button_w, int n, int over, int slot, bool overflow) {
    strip_hit_t h = strip_hit(x, button_w, n, over);
    if (h.slot == slot && h.overflow == overflow) return;
    printf("FAIL strip_hit(%s) = {slot %d, overflow %d}; want {slot %d, overflow %d}\n", what, h.slot, h.overflow, slot, overflow);
    failed++;
}

int main(void) {
    eq("strip_slots(3, 0)", strip_slots(3, 0), 3);
    eq("strip_slots(9, 4)", strip_slots(9, 4), 10);
    eq("strip_width(0): the key box alone", strip_width(0), 20);
    eq("strip_width(1)", strip_width(1), 44);
    eq("strip_width(10)", strip_width(10), 11 * 20 + 10 * 4);

    // One glyph: the strip is 44pt wide, centred in an 84pt button, so it starts at 20.
    hit("left of the strip", 19, 84, 1, 0, -1, false);
    hit("first pixel of the glyph", 20, 84, 1, 0, 0, false);
    hit("middle of the glyph", 30, 84, 1, 0, 0, false);
    hit("last pixel of the glyph", 39.9, 84, 1, 0, 0, false);
    hit("gap after the glyph", 40, 84, 1, 0, -1, false);
    hit("key box", 55, 84, 1, 0, -1, false);
    hit("right of the strip", 70, 84, 1, 0, -1, false);
    hit("negative x", -5, 84, 1, 0, -1, false);

    // Two glyphs and a "+3" slot in a 100pt button: slots start at 4, 28 and 52.
    hit("first of two glyphs", 10, 100, 2, 3, 0, false);
    hit("second glyph", 30, 100, 2, 3, 1, false);
    hit("gap before the +k slot", 49, 100, 2, 3, -1, false);
    hit("+k slot", 60, 100, 2, 3, -1, true);
    hit("gap after the +k slot", 74, 100, 2, 3, -1, false);
    hit("key box after +k", 80, 100, 2, 3, -1, false);
    eq("strip_overflow_left", strip_overflow_left(100, 2, 3), 52);

    // Without overflow the third slot is the key box.
    hit("key box is not +k", 66, 100, 2, 0, -1, false);

    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
