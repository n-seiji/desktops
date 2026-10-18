#include "cardmath.h"
#include <math.h>

static double bez(double a, double b, double t) {
    double u = 1 - t;
    return 3 * u * u * t * a + 3 * u * t * t * b + t * t * t;
}

double cardmath_ease(double k) {
    double lo = 0, hi = 1, t = k;
    for (int i = 0; i < 24; i++) {
        double x = bez(0.22, 0.25, t);
        if (x < k) lo = t;
        else hi = t;
        t = (lo + hi) / 2;
    }
    return bez(0.9, 1, t);
}

cardmath_pose_t cardmath_pose(double p, double dx, double fade) {
    return (cardmath_pose_t){p * dx, fmax(0, 1 - fade * fabs(p))};
}

int cardmath_needed(long sel_v, int last_dir, int n, long *out) {
    int m = 0;
    out[m++] = sel_v;
    if (n < 2) return m;
    if (last_dir) out[m++] = sel_v - last_dir;
    return m;
}

CGRect cardmath_fit(CGSize img, CGRect box) {
    double f = fmin(box.size.width / img.width, box.size.height / img.height);
    CGRect r = {{0, 0}, {img.width * f, img.height * f}};
    r.origin = CGPointMake(box.origin.x + (box.size.width - r.size.width) / 2, box.origin.y + (box.size.height - r.size.height) / 2);
    return r;
}

double cardmath_slide(double from, double to, double k) {
    if (k >= 1) return to;
    k = 1 - k;
    return to + (from - to) * k * k * k;
}
