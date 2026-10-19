#pragma once
// Focus history across desktops, browser style: back and forward through the last HIST_MAX windows.
#include <stdbool.h>
#include <stdint.h>

#define HIST_MAX 10

typedef struct {
    uint32_t wid[HIST_MAX];
    int n, at; // at is the current entry, -1 while empty
} hist_t;

// wid took focus. The same window as the current entry changes nothing; otherwise the forward part is
// dropped, an older entry of wid is removed, and wid becomes the newest entry (the oldest falls off at HIST_MAX).
void hist_visit(hist_t *h, uint32_t wid);
// Moves one entry back (dir < 0) or forward (dir > 0), removing the entries in the way that ok rejects.
// Returns the window reached, or 0 (and keeps the position) when there is none.
uint32_t hist_step(hist_t *h, int dir, bool (*ok)(uint32_t));
