#pragma once
// Geometry of the focus ring: four thin windows (top, bottom, left, right) around the focused
// window, so the backing store stays small. The top and bottom ones hold the corner arcs.
#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>

#define RING_WIDTH 1.5
#define RING_GAP 3.0
#define DEFAULT_RADIUS 16.0 // used when WindowServer does not report the window's corner radius
#define RING_OUTSET (RING_GAP + RING_WIDTH) // window frame to the ring's outer edge

enum { EDGE_TOP, EDGE_BOTTOM, EDGE_LEFT, EDGE_RIGHT, EDGE_COUNT };

// Height of the top and bottom edge windows for a window with corner radius `radius`.
CGFloat border_edge_thickness(CGFloat radius);
// Edge window e around outer (the ring's outer rect), snapped out to whole points: the ring itself
// sits at fractional offsets inside, and a window at a half-point origin leaves an unpainted row.
CGRect border_edge_rect(int e, CGRect outer, CGFloat radius);
// Corner radius of the ring's centre line: the window's radius plus the gap and half the width,
// at most half the shorter side of the centre line's rect.
CGFloat border_ring_radius(CGFloat radius, CGSize outer);
// The ring does not fit around a window this small: it is not drawn.
bool border_too_small(CGSize outer, CGFloat radius);
