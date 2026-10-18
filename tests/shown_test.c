#include "../src/shown.h"
#include <stdio.h>

static int failed;

static void visible(const char *what, double alpha, CGRect r, const CGRect *d, uint32_t nd, bool want) {
    bool got = shown_visible(alpha, r, d, nd);
    if (got == want) return;
    printf("FAIL shown_visible(%s) = %d; want %d\n", what, got, want);
    failed++;
}

static void grace(const char *what, bool listed, bool was, double now, double shown_at, bool want, bool want_was) {
    bool w = was;
    bool got = shown_grace(listed, &w, now, shown_at);
    if (got == want && w == want_was) return;
    printf("FAIL shown_grace(%s) = %d, was_listed %d; want %d, %d\n", what, got, w, want, want_was);
    failed++;
}

static void parked(const char *what, CGPoint p, bool want) {
    CGRect main_display = {{0, 0}, {1920, 1212}};
    bool got = shown_is_parked(p, main_display);
    if (got == want) return;
    printf("FAIL shown_is_parked(%s) = %d; want %d\n", what, got, want);
    failed++;
}

static void front(const char *what, double now, double until, uint32_t fresh, uint32_t focused, uint32_t want) {
    uint32_t got = shown_front(now, until, fresh, focused);
    if (got == want) return;
    printf("FAIL shown_front(%s) = %u; want %u\n", what, got, want);
    failed++;
}

int main(void) {
    CGRect main_display = {{0, 0}, {1920, 1212}};
    CGRect second = {{1920, 0}, {1280, 800}};
    CGRect both[] = {main_display, second};

    visible("inside", 1, (CGRect){{100, 100}, {800, 600}}, &main_display, 1, true);
    visible("transparent", 0, (CGRect){{100, 100}, {800, 600}}, &main_display, 1, false);
    visible("parked at the corner", 1, (CGRect){{1919, 1211}, {1885, 1192}}, &main_display, 1, false);
    visible("32pt still on the display", 1, (CGRect){{1888, 100}, {800, 600}}, &main_display, 1, true);
    visible("31pt on the display", 1, (CGRect){{1889, 100}, {800, 600}}, &main_display, 1, false);
    visible("small window fully inside", 1, (CGRect){{10, 10}, {10, 10}}, &main_display, 1, true);
    visible("small window half off", 1, (CGRect){{1915, 10}, {10, 10}}, &main_display, 1, false);
    visible("empty window", 1, (CGRect){{10, 10}, {0, 0}}, &main_display, 1, false);
    visible("on the second display", 1, (CGRect){{2000, 100}, {400, 300}}, both, 2, true);
    visible("on the second display only", 1, (CGRect){{2000, 100}, {400, 300}}, &main_display, 1, false);
    visible("no displays", 1, (CGRect){{100, 100}, {800, 600}}, both, 0, false);

    grace("long unparked, listed", true, false, 10, 1, true, true);
    grace("long unparked, not listed", false, true, 10, 1, false, false);
    grace("just unparked, not listed yet", false, true, 1.2, 1, true, true);
    grace("just unparked, not listed and never was", false, false, 1.2, 1, false, false);
    grace("just unparked, listed", true, false, 1.2, 1, true, false);
    grace("exactly at the end of the period", false, true, 0.3, 0, true, true);
    grace("just past the period", false, true, 0.31, 0, false, false);

    front("just focused by us", 5, 5.3, 42, 7, 42);
    front("period over", 5.3, 5.3, 42, 7, 7);
    front("nothing focused by us", 5, 5.3, 0, 7, 7);
    front("nothing focused at all", 6, 5.3, 42, 0, 0);

    CGPoint park = shown_park_point(main_display);
    if (park.x != 1919 || park.y != 1211) {
        printf("FAIL shown_park_point = {%g,%g}; want {1919,1211}\n", park.x, park.y);
        failed++;
    }
    parked("the park point", (CGPoint){1919, 1211}, true);
    parked("4pt left of it", (CGPoint){1915, 1211}, true);
    parked("5pt left of it", (CGPoint){1914, 1211}, false);
    parked("64pt above it", (CGPoint){1919, 1147}, true);
    parked("65pt above it", (CGPoint){1919, 1146}, false);
    parked("a normal window", (CGPoint){100, 100}, false);
    parked("past the corner", (CGPoint){2500, 1500}, true);

    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
