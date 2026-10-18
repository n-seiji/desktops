#include "order.h"

void order_group(const pid_t *pids, const uint32_t *wids, int n, pid_t *out_pids, uint32_t *out_wids) {
    int m = 0;
    for (int i = 0; i < n; i++) {
        bool seen = false;
        for (int j = 0; j < i && !seen; j++) seen = pids[j] == pids[i];
        if (seen) continue;
        for (int j = i; j < n; j++) {
            if (pids[j] != pids[i]) continue;
            out_pids[m] = pids[j];
            out_wids[m++] = wids[j];
        }
    }
}

void order_split(int n, int slots, int *shown, int *over) {
    *shown = n;
    *over = 0;
    if (n <= slots) return;
    *shown = slots - 1;
    *over = n - *shown;
}

uint32_t order_hash(const uint32_t *wids, int n) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < n; i++) hash = (hash ^ wids[i]) * 16777619u;
    return hash;
}
