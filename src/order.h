#pragma once
// Which windows the menu bar shows, and in which order.
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// The n windows of the desktop in tracking order, regrouped by app in order of each app's first
// window, an app's windows keeping their order, so focus changes never reorder the strip.
// out_pids and out_wids may not alias the inputs.
void order_group(const pid_t *pids, const uint32_t *wids, int n, pid_t *out_pids, uint32_t *out_wids);
// With more than `slots` windows the last slot reads "+k": *shown windows get a glyph and the *over
// others are only counted. Otherwise *shown is n and *over is 0.
void order_split(int n, int slots, int *shown, int *over);
// FNV-1a of window ids: tells the overflow list when the hidden set went stale.
uint32_t order_hash(const uint32_t *wids, int n);
