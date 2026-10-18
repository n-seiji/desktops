#include "strip.h"

int strip_slots(int n, int over) {
    return n + (over > 0);
}

int strip_width(int slots) {
    return (slots + 1) * STRIP_SLOT + slots * STRIP_GAP;
}

static int slot_at(double x, int n) {
    if (x < 0) return -1;
    int i = (int)(x / (STRIP_SLOT + STRIP_GAP));
    return i < n && x - i * (STRIP_SLOT + STRIP_GAP) < STRIP_SLOT ? i : -1;
}

static double strip_left(double button_w, int n, int over) {
    return (button_w - strip_width(strip_slots(n, over))) / 2;
}

strip_hit_t strip_hit(double x, double button_w, int n, int over) {
    double sx = x - strip_left(button_w, n, over);
    return (strip_hit_t){slot_at(sx, n), over > 0 && slot_at(sx, n + 1) == n};
}

double strip_overflow_left(double button_w, int n, int over) {
    return strip_left(button_w, n, over) + n * (STRIP_SLOT + STRIP_GAP);
}
