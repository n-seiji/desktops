#pragma once
// Selection arithmetic for the switchers, the instant mode and the menu bar's overflow list.

int sel_clamp(int v, int lo, int hi);
// i wrapped into 0..n-1 (n > 0); a negative i counts from the end.
int sel_wrap(long i, int n);
// The first selection in a list of n entries: the one after the focused entry at (before it for
// dir < 0), the first (the last for dir < 0) when none is focused (at < 0). 0 for an empty list.
int sel_start(int at, int dir, int n);
// The first row of a view of `rows` rows, moved by the least that keeps sel in view.
int sel_top(int top, int sel, int rows);
// The first of `count` dots out of n, centred on at and kept inside 0..n-count.
int sel_window(int at, int n, int count);
// The row under y (measured from the first row's top) in a view of `rows` rows of height row_h that
// starts at entry top, or -1 for none.
int sel_row_at(double y, double row_h, int top, int rows);
