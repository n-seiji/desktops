#include "../src/cardmath.h"
#include <math.h>
#include <stdio.h>

static int failed;

static void near(const char *what, double got, double want) {
    if (fabs(got - want) < 1e-9) return;
    printf("FAIL %s = %.12g; want %.12g\n", what, got, want);
    failed++;
}

static void eq(const char *what, long got, long want) {
    if (got == want) return;
    printf("FAIL %s = %ld; want %ld\n", what, got, want);
    failed++;
}

int main(void) {
    near("cardmath_ease(0)", cardmath_ease(0), 0);
    near("cardmath_ease(1)", cardmath_ease(1), 1);
    double prev = 0;
    for (int i = 1; i <= 100; i++) {
        double e = cardmath_ease(i / 100.0);
        if (e < prev || e > 1 + 1e-9) {
            printf("FAIL cardmath_ease(%d/100) = %g after %g: not monotonic within 0..1\n", i, e, prev);
            failed++;
        }
        prev = e;
    }
    if (cardmath_ease(0.25) < 0.7) {
        printf("FAIL cardmath_ease(0.25) = %g; most of the travel should be done by a quarter of the time\n", cardmath_ease(0.25));
        failed++;
    }

    cardmath_pose_t p = cardmath_pose(0, 120, 1.6);
    near("pose of the centre card x", p.x, 0);
    near("pose of the centre card alpha", p.a, 1);
    p = cardmath_pose(0.5, 120, 1.6);
    near("pose half a slot right x", p.x, 60);
    near("pose half a slot right alpha", p.a, 0.2);
    p = cardmath_pose(-0.5, 120, 1.6);
    near("pose half a slot left x", p.x, -60);
    near("pose half a slot left alpha", p.a, 0.2);
    p = cardmath_pose(1, 120, 1.6);
    near("pose a slot away is invisible", p.a, 0);
    near("pose a slot away x", p.x, 120);

    long need[2];
    eq("needed with one window", cardmath_needed(5, 1, 1, need), 1);
    eq("needed with one window position", need[0], 5);
    eq("needed before any step", cardmath_needed(5, 0, 4, need), 1);
    eq("needed after a forward step", cardmath_needed(5, 1, 4, need), 2);
    eq("needed after a forward step: selected", need[0], 5);
    eq("needed after a forward step: left", need[1], 4);
    eq("needed after a backward step", cardmath_needed(-3, -1, 4, need), 2);
    eq("needed after a backward step: left", need[1], -2);

    CGRect wide = cardmath_fit((CGSize){1000, 500}, (CGRect){{10, 20}, {576, 330}});
    near("fit wide width", wide.size.width, 576);
    near("fit wide height", wide.size.height, 288);
    near("fit wide x", wide.origin.x, 10);
    near("fit wide y", wide.origin.y, 20 + 21);
    CGRect tall = cardmath_fit((CGSize){300, 600}, (CGRect){{0, 0}, {576, 330}});
    near("fit tall width", tall.size.width, 165);
    near("fit tall height", tall.size.height, 330);
    near("fit tall x", tall.origin.x, (576 - 165) / 2.0);

    near("slide at the start", cardmath_slide(3, 7, 0), 3);
    near("slide at the end", cardmath_slide(3, 7, 1), 7);
    near("slide past the end", cardmath_slide(3, 7, 1.5), 7);
    near("slide halfway", cardmath_slide(3, 7, 0.5), 7 - 4 * 0.125);
    near("slide backwards", cardmath_slide(7, 3, 0.5), 3 + 4 * 0.125);

    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
