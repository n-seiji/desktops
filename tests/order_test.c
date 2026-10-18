#include "../src/order.h"
#include <stdio.h>
#include <string.h>

static int failed;

static void group(const char *what, const pid_t *pids, const uint32_t *wids, int n, const pid_t *want_pids, const uint32_t *want_wids) {
    pid_t op[16];
    uint32_t ow[16];
    order_group(pids, wids, n, op, ow);
    if (!memcmp(op, want_pids, sizeof *op * n) && !memcmp(ow, want_wids, sizeof *ow * n)) return;
    printf("FAIL order_group(%s) = pids", what);
    for (int i = 0; i < n; i++) printf(" %d", op[i]);
    printf(", wids");
    for (int i = 0; i < n; i++) printf(" %u", ow[i]);
    printf("\n");
    failed++;
}

static void split(int n, int slots, int want_shown, int want_over) {
    int shown, over;
    order_split(n, slots, &shown, &over);
    if (shown == want_shown && over == want_over) return;
    printf("FAIL order_split(%d, %d) = {%d, %d}; want {%d, %d}\n", n, slots, shown, over, want_shown, want_over);
    failed++;
}

static void hash(const char *what, const uint32_t *ids, int n, uint32_t want) {
    uint32_t got = order_hash(ids, n);
    if (got == want) return;
    printf("FAIL order_hash(%s) = %u; want %u\n", what, got, want);
    failed++;
}

int main(void) {
    // Windows come in tracking order: apps interleave, the strip groups them.
    group("interleaved apps", (pid_t[]){1, 2, 1, 3, 2}, (uint32_t[]){10, 20, 11, 30, 21}, 5,
          (pid_t[]){1, 1, 2, 2, 3}, (uint32_t[]){10, 11, 20, 21, 30});
    group("already grouped", (pid_t[]){5, 5, 7}, (uint32_t[]){1, 2, 3}, 3, (pid_t[]){5, 5, 7}, (uint32_t[]){1, 2, 3});
    group("app order follows its first window", (pid_t[]){9, 4, 9, 4}, (uint32_t[]){1, 2, 3, 4}, 4,
          (pid_t[]){9, 9, 4, 4}, (uint32_t[]){1, 3, 2, 4});
    group("one window", (pid_t[]){3}, (uint32_t[]){8}, 1, (pid_t[]){3}, (uint32_t[]){8});
    group("empty", (pid_t[]){0}, (uint32_t[]){0}, 0, (pid_t[]){0}, (uint32_t[]){0});

    split(0, 10, 0, 0);
    split(5, 10, 5, 0);
    split(10, 10, 10, 0);
    split(11, 10, 9, 2);
    split(64, 10, 9, 55);

    hash("no ids", (uint32_t[]){0}, 0, 2166136261u);
    hash("one id", (uint32_t[]){1}, 1, 67918732u);
    hash("two ids", (uint32_t[]){7, 9}, 2, 2825860797u);
    if (order_hash((uint32_t[]){7, 9}, 2) == order_hash((uint32_t[]){9, 7}, 2)) {
        printf("FAIL order_hash ignores the order of the ids\n");
        failed++;
    }

    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
