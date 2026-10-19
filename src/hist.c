#include "hist.h"

static void drop(hist_t *h, int i) {
    for (int j = i; j < h->n - 1; j++) h->wid[j] = h->wid[j + 1];
    h->n--;
}

void hist_visit(hist_t *h, uint32_t wid) {
    if (h->at >= 0 && h->wid[h->at] == wid) return;
    h->n = h->at + 1;
    for (int i = 0; i < h->n; i++)
        if (h->wid[i] == wid) {
            drop(h, i);
            break;
        }
    if (h->n == HIST_MAX) drop(h, 0);
    h->wid[h->n++] = wid;
    h->at = h->n - 1;
}

uint32_t hist_step(hist_t *h, int dir, bool (*ok)(uint32_t)) {
    int step = dir < 0 ? -1 : 1;
    for (int i = h->at + step; i >= 0 && i < h->n; i += step) {
        if (ok(h->wid[i])) {
            h->at = i;
            return h->wid[i];
        }
        drop(h, i);
        if (step < 0) h->at--;
        else i--;
    }
    return 0;
}
