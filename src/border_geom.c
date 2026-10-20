#include "border_geom.h"
#include <math.h>

#define SIDE_THICKNESS ceil(RING_WIDTH) // left/right only hold the straight part of the ring

// Height of the top and bottom edge windows: tall enough to hold the corner arcs.
static CGFloat edge_thickness(CGFloat radius) {
    return radius + RING_OUTSET;
}

CGRect border_edge_rect(int e, CGRect o, CGFloat radius, CGRect clip) {
    CGFloat t = edge_thickness(radius), s = SIDE_THICKNESS;
    CGRect top = CGRectIntegral(CGRectMake(o.origin.x, o.origin.y, o.size.width, t));
    CGRect bottom = CGRectIntegral(CGRectMake(o.origin.x, o.origin.y + o.size.height - t, o.size.width, t));
    CGRect r;
    switch (e) {
    case EDGE_TOP: r = top; break;
    case EDGE_BOTTOM: r = bottom; break;
    case EDGE_LEFT: r = CGRectIntegral(CGRectMake(o.origin.x, 0, s, 1)); break;
    default: r = CGRectIntegral(CGRectMake(o.origin.x + o.size.width - s, 0, s, 1));
    }
    if (e >= EDGE_LEFT) { // the sides run exactly between the snapped top and bottom, so they never overlap them
        r.origin.y = CGRectGetMaxY(top);
        r.size.height = fmax(0, CGRectGetMinY(bottom) - r.origin.y);
    }
    return CGRectIntersection(r, clip);
}

CGFloat border_ring_radius(CGFloat radius, CGSize outer) {
    CGFloat ring_r = radius + RING_GAP + RING_WIDTH / 2;
    CGFloat max_r = fmin(outer.width - RING_WIDTH, outer.height - RING_WIDTH) / 2;
    return ring_r > max_r ? max_r : ring_r;
}

bool border_too_small(CGSize outer, CGFloat radius) {
    return outer.width < 2 * edge_thickness(radius) || outer.height < 2 * edge_thickness(radius);
}
