#include "../src/hist.h"
#include <stdio.h>
#include <string.h>

static int failed;

static uint32_t dead[16];
static int ndead;

static bool alive(uint32_t wid) {
    for (int i = 0; i < ndead; i++)
        if (dead[i] == wid) return false;
    return true;
}

static void kill_wids(const uint32_t *wids, int n) {
    memcpy(dead, wids, sizeof *wids * n);
    ndead = n;
}

static hist_t make(const uint32_t *wids, int n) {
    hist_t h = {.n = 0, .at = -1};
    for (int i = 0; i < n; i++) hist_visit(&h, wids[i]);
    ndead = 0;
    return h;
}

static void print(const hist_t *h) {
    printf("{n=%d at=%d wids", h->n, h->at);
    for (int i = 0; i < h->n; i++) printf(" %u", h->wid[i]);
    printf("}");
}

static void expect(const char *what, const hist_t *h, int at, const uint32_t *want, int n) {
    if (h->n == n && h->at == at && !memcmp(h->wid, want, sizeof *want * n)) return;
    hist_t w = {.n = n, .at = at};
    memcpy(w.wid, want, sizeof *want * n);
    printf("FAIL %s\n  got  ", what);
    print(h);
    printf("\n  want ");
    print(&w);
    printf("\n");
    failed++;
}

static void stepped(const char *what, uint32_t got, uint32_t want) {
    if (got == want) return;
    printf("FAIL %s = %u; want %u\n", what, got, want);
    failed++;
}

int main(void) {
    {
        hist_t h = {.n = 0, .at = -1};
        hist_visit(&h, 5);
        expect("visit into an empty history", &h, 0, (uint32_t[]){5}, 1);
    }
    {
        hist_t h = {.n = 0, .at = -1};
        stepped("back in an empty history", hist_step(&h, -1, alive), 0);
        stepped("forward in an empty history", hist_step(&h, 1, alive), 0);
        expect("steps in an empty history", &h, -1, (uint32_t[]){0}, 0);
    }
    {
        hist_t h = make((uint32_t[]){1, 2, 3}, 3);
        hist_visit(&h, 3);
        expect("same window again", &h, 2, (uint32_t[]){1, 2, 3}, 3);
        hist_visit(&h, 1);
        expect("visit moves an older entry to the end", &h, 2, (uint32_t[]){2, 3, 1}, 3);
    }
    {
        uint32_t ids[HIST_MAX + 1];
        for (int i = 0; i < HIST_MAX + 1; i++) ids[i] = i + 1;
        hist_t h = make(ids, HIST_MAX + 1);
        expect("the 11th window drops the oldest", &h, HIST_MAX - 1, ids + 1, HIST_MAX);
    }
    {
        uint32_t ids[HIST_MAX];
        for (int i = 0; i < HIST_MAX; i++) ids[i] = i + 1;
        hist_t h = make(ids, HIST_MAX);
        hist_visit(&h, 5);
        expect("a window already in a full history is moved, nothing falls off", &h, HIST_MAX - 1,
               (uint32_t[]){1, 2, 3, 4, 6, 7, 8, 9, 10, 5}, HIST_MAX);
    }
    {
        uint32_t ids[HIST_MAX];
        for (int i = 0; i < HIST_MAX; i++) ids[i] = i + 1;
        hist_t h = make(ids, HIST_MAX);
        hist_step(&h, -1, alive);
        hist_step(&h, -1, alive);
        hist_visit(&h, 99);
        expect("a new window after going back in a full history", &h, HIST_MAX - 2,
               (uint32_t[]){1, 2, 3, 4, 5, 6, 7, 8, 99}, HIST_MAX - 1);
    }
    {
        hist_t h = make((uint32_t[]){1, 2, 3}, 3);
        stepped("back", hist_step(&h, -1, alive), 2);
        stepped("back again", hist_step(&h, -1, alive), 1);
        stepped("back at the start", hist_step(&h, -1, alive), 0);
        expect("back at the start keeps at", &h, 0, (uint32_t[]){1, 2, 3}, 3);
        stepped("forward", hist_step(&h, 1, alive), 2);
        stepped("forward again", hist_step(&h, 1, alive), 3);
        stepped("forward at the end", hist_step(&h, 1, alive), 0);
        expect("forward at the end keeps at", &h, 2, (uint32_t[]){1, 2, 3}, 3);
    }
    {
        hist_t h = make((uint32_t[]){1, 2, 3}, 3);
        hist_step(&h, -1, alive);
        hist_visit(&h, 2);
        expect("the focus notice after going back changes nothing", &h, 1, (uint32_t[]){1, 2, 3}, 3);
    }
    {
        hist_t h = make((uint32_t[]){1, 2, 3}, 3);
        hist_step(&h, -1, alive);
        hist_visit(&h, 9);
        expect("visit after going back drops the forward part", &h, 2, (uint32_t[]){1, 2, 9}, 3);
    }
    {
        hist_t h = make((uint32_t[]){1, 2, 3, 4}, 4);
        hist_step(&h, -1, alive);
        hist_step(&h, -1, alive);
        hist_visit(&h, 4);
        expect("a forward window is truncated before it is deduped", &h, 2, (uint32_t[]){1, 2, 4}, 3);
    }
    {
        hist_t h = make((uint32_t[]){1, 2, 3, 4}, 4);
        kill_wids((uint32_t[]){2, 3}, 2);
        stepped("back skips dead windows", hist_step(&h, -1, alive), 1);
        expect("dead entries below at are removed and at shifts", &h, 0, (uint32_t[]){1, 4}, 2);
    }
    {
        hist_t h = make((uint32_t[]){1, 2, 3, 4}, 4);
        hist_step(&h, -1, alive);
        hist_step(&h, -1, alive);
        hist_step(&h, -1, alive);
        kill_wids((uint32_t[]){2, 3}, 2);
        stepped("forward skips dead windows", hist_step(&h, 1, alive), 4);
        expect("dead entries above at are removed", &h, 1, (uint32_t[]){1, 4}, 2);
    }
    {
        hist_t h = make((uint32_t[]){1, 2, 3}, 3);
        kill_wids((uint32_t[]){1, 2}, 2);
        stepped("nothing alive to go back to", hist_step(&h, -1, alive), 0);
        expect("all-dead entries are removed", &h, 0, (uint32_t[]){3}, 1);
    }
    {
        hist_t h = make((uint32_t[]){1, 2, 3}, 3);
        hist_step(&h, -1, alive);
        kill_wids((uint32_t[]){2}, 1);
        stepped("back from a dead current entry", hist_step(&h, -1, alive), 1);
        expect("the dead current entry stays", &h, 0, (uint32_t[]){1, 2, 3}, 3);
        stepped("forward after leaving a dead entry", hist_step(&h, 1, alive), 3);
        expect("the dead entry is removed when passed", &h, 1, (uint32_t[]){1, 3}, 2);
    }

    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
