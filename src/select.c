#include "select.h"
#include <math.h>

int sel_clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

int sel_wrap(long i, int n) {
    return (int)(((i % n) + n) % n);
}

int sel_start(int at, int dir, int n) {
    if (n <= 0) return 0;
    if (at < 0) return dir > 0 ? 0 : n - 1;
    return sel_wrap((long)at + dir, n);
}

int sel_top(int top, int sel, int rows) {
    if (sel < top) return sel;
    if (sel >= top + rows) return sel - rows + 1;
    return top;
}

int sel_window(int at, int n, int count) {
    int last = n - count;
    return sel_clamp(at - count / 2, 0, last < 0 ? 0 : last);
}

int sel_row_at(double y, double row_h, int top, int rows) {
    int k = (int)floor(y / row_h);
    return k < 0 || k >= rows ? -1 : top + k;
}
