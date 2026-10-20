#include "../src/border_geom.h"
#include <math.h>
#include <stdio.h>

static int failed;

static void rect(const char *what, CGRect got, CGRect want) {
    if (got.origin.x == want.origin.x && got.origin.y == want.origin.y && got.size.width == want.size.width &&
        got.size.height == want.size.height)
        return;
    printf("FAIL %s = {%g,%g %gx%g}; want {%g,%g %gx%g}\n", what, got.origin.x, got.origin.y, got.size.width, got.size.height,
           want.origin.x, want.origin.y, want.size.width, want.size.height);
    failed++;
}

static void near(const char *what, double got, double want) {
    if (fabs(got - want) < 1e-9) return;
    printf("FAIL %s = %.12g; want %.12g\n", what, got, want);
    failed++;
}

static void flag(const char *what, bool got, bool want) {
    if (got == want) return;
    printf("FAIL %s = %d; want %d\n", what, got, want);
    failed++;
}

int main(void) {
    // A window of radius 16 whose ring's outer rect is 500x300 at (100, 50): the top and bottom edges
    // are 20.5pt tall and snap out to 21; the sides are 2pt wide and run exactly between them.
    CGRect outer = {{100, 50}, {500, 300}};
    CGRect all = CGRectInfinite;
    rect("top", border_edge_rect(EDGE_TOP, outer, 16, all), (CGRect){{100, 50}, {500, 21}});
    rect("bottom", border_edge_rect(EDGE_BOTTOM, outer, 16, all), (CGRect){{100, 329}, {500, 21}});
    rect("left", border_edge_rect(EDGE_LEFT, outer, 16, all), (CGRect){{100, 71}, {2, 258}});
    rect("right", border_edge_rect(EDGE_RIGHT, outer, 16, all), (CGRect){{598, 71}, {2, 258}});

    // A half-point origin is snapped outwards, so no row along an edge stays unpainted.
    CGRect odd = {{100.5, 50.25}, {500, 300}};
    rect("top at a fractional origin", border_edge_rect(EDGE_TOP, odd, 16, all), (CGRect){{100, 50}, {501, 21}});
    rect("bottom at a fractional origin", border_edge_rect(EDGE_BOTTOM, odd, 16, all), (CGRect){{100, 329}, {501, 22}});
    rect("right at a fractional origin", border_edge_rect(EDGE_RIGHT, odd, 16, all), (CGRect){{598, 71}, {3, 258}});

    // At the smallest size that fits, the snapped top and bottom meet and the sides are empty, not a sliver over them.
    CGRect tight = {{100, 0.25}, {500, 41}};
    flag("sides of the smallest ring are empty", CGRectIsEmpty(border_edge_rect(EDGE_LEFT, tight, 16, all)), true);

    // Bigger corner radii make taller top and bottom edges and shorter sides.
    rect("top with radius 26", border_edge_rect(EDGE_TOP, outer, 26, all), (CGRect){{100, 50}, {500, 31}});
    rect("left with radius 26", border_edge_rect(EDGE_LEFT, outer, 26, all), (CGRect){{100, 81}, {2, 238}});

    // A ring whose bottom passes the display is laid out in full; only the edge windows are cut.
    CGRect low = {{100, 1000}, {500, 244.5}};
    CGRect display = {{0, 0}, {1920, 1243}};
    rect("top inside the clip", border_edge_rect(EDGE_TOP, low, 16, display), (CGRect){{100, 1000}, {500, 21}});
    rect("bottom cut at the clip", border_edge_rect(EDGE_BOTTOM, low, 16, display), (CGRect){{100, 1224}, {500, 19}});
    rect("left inside the clip", border_edge_rect(EDGE_LEFT, low, 16, display), (CGRect){{100, 1021}, {2, 203}});
    rect("right inside the clip", border_edge_rect(EDGE_RIGHT, low, 16, display), (CGRect){{598, 1021}, {2, 203}});

    // A window at the display's left side: the left edge is gone and the top and bottom start at the display.
    CGRect flush = {{-4.5, 100}, {500, 300}};
    flag("left fully outside the clip is empty", CGRectIsEmpty(border_edge_rect(EDGE_LEFT, flush, 16, display)), true);
    rect("top cut at the clip's left", border_edge_rect(EDGE_TOP, flush, 16, display), (CGRect){{0, 100}, {496, 21}});
    rect("right beside a left cut", border_edge_rect(EDGE_RIGHT, flush, 16, display), (CGRect){{493, 121}, {3, 258}});

    CGRect shallow = {{0, 0}, {1920, 1100}};
    flag("bottom fully outside the clip is empty", CGRectIsEmpty(border_edge_rect(EDGE_BOTTOM, low, 16, shallow)), true);
    rect("left cut at a shallower clip", border_edge_rect(EDGE_LEFT, low, 16, shallow), (CGRect){{100, 1021}, {2, 79}});
    flag("top inside the clip is not empty", CGRectIsEmpty(border_edge_rect(EDGE_TOP, low, 16, shallow)), false);

    near("ring radius of a normal window", border_ring_radius(16, (CGSize){500, 300}), 16 + 3 + 0.75);
    near("ring radius is limited by the shorter side", border_ring_radius(16, (CGSize){30, 200}), (30 - 1.5) / 2);
    near("ring radius of a square", border_ring_radius(40, (CGSize){60, 60}), (60 - 1.5) / 2);

    flag("normal window is not too small", border_too_small((CGSize){500, 300}, 16), false);
    flag("exactly two thicknesses wide fits", border_too_small((CGSize){41, 100}, 16), false);
    flag("narrower than two thicknesses", border_too_small((CGSize){40.9, 100}, 16), true);
    flag("shorter than two thicknesses", border_too_small((CGSize){500, 40}, 16), true);
    flag("a bigger radius needs more room", border_too_small((CGSize){50, 100}, 26), true);

    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
