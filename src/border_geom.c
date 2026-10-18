#include "border_geom.h"
#include <math.h>

#define SIDE_THICKNESS ceil(RING_WIDTH) // left/right only hold the straight part of the ring

CGFloat border_edge_thickness(CGFloat radius) {
    return radius + RING_OUTSET;
}

CGRect border_edge_rect(int e, CGRect o, CGFloat radius) {
    CGFloat t = border_edge_thickness(radius), s = SIDE_THICKNESS;
    CGRect r;
    switch (e) {
    case EDGE_TOP: r = CGRectMake(o.origin.x, o.origin.y, o.size.width, t); break;
    case EDGE_BOTTOM: r = CGRectMake(o.origin.x, o.origin.y + o.size.height - t, o.size.width, t); break;
    case EDGE_LEFT: r = CGRectMake(o.origin.x, o.origin.y + t, s, o.size.height - 2 * t); break;
    default: r = CGRectMake(o.origin.x + o.size.width - s, o.origin.y + t, s, o.size.height - 2 * t);
    }
    return CGRectIntegral(r);
}

CGFloat border_ring_radius(CGFloat radius, CGSize outer) {
    CGFloat ring_r = radius + RING_GAP + RING_WIDTH / 2;
    CGFloat max_r = fmin(outer.width - RING_WIDTH, outer.height - RING_WIDTH) / 2;
    return ring_r > max_r ? max_r : ring_r;
}

bool border_too_small(CGSize outer, CGFloat radius) {
    return outer.width < 2 * border_edge_thickness(radius) || outer.height < 2 * border_edge_thickness(radius);
}
