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
    // are 20.5pt tall and snap out to 21; the sides are 2pt wide and run between them.
    CGRect outer = {{100, 50}, {500, 300}};
    rect("top", border_edge_rect(EDGE_TOP, outer, 16), (CGRect){{100, 50}, {500, 21}});
    rect("bottom", border_edge_rect(EDGE_BOTTOM, outer, 16), (CGRect){{100, 329}, {500, 21}});
    rect("left", border_edge_rect(EDGE_LEFT, outer, 16), (CGRect){{100, 70}, {2, 260}});
    rect("right", border_edge_rect(EDGE_RIGHT, outer, 16), (CGRect){{598, 70}, {2, 260}});

    // A half-point origin is snapped outwards, so no row along an edge stays unpainted.
    CGRect odd = {{100.5, 50.25}, {500, 300}};
    rect("top at a fractional origin", border_edge_rect(EDGE_TOP, odd, 16), (CGRect){{100, 50}, {501, 21}});
    rect("right at a fractional origin", border_edge_rect(EDGE_RIGHT, odd, 16), (CGRect){{598, 70}, {3, 260}});

    // Bigger corner radii make taller top and bottom edges and shorter sides.
    rect("top with radius 26", border_edge_rect(EDGE_TOP, outer, 26), (CGRect){{100, 50}, {500, 31}});
    rect("left with radius 26", border_edge_rect(EDGE_LEFT, outer, 26), (CGRect){{100, 80}, {2, 240}});

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
