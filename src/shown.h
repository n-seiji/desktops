#pragma once
// Which windows count as shown right now, and which one the menu bar calls focused.
#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>
#include <stdint.h>

#define FRESH_S 0.3 // WindowServer takes about this long to list windows we just unparked

// WindowServer's view of a window: not transparent, with at least 32pt (or all of it, when smaller)
// inside one of the displays. A window parked at the corner of the main display fails this.
bool shown_visible(double alpha, CGRect window, const CGRect *displays, uint32_t ndisplays);

// A window unparked less than FRESH_S ago may not be listed as on screen yet: it counts as listed
// when it was before. listed is what WindowServer says now; *was_listed is what it said the last
// time the window was past that period, and is updated then.
bool shown_grace(bool listed, bool *was_listed, double now, double shown_at);

// The window the menu bar marks: the one just focused by us until FRESH_S has passed, because the
// focus macOS reports lags behind, else the reported one. 0 for none.
uint32_t shown_front(double now, double fresh_until, uint32_t fresh_wid, uint32_t focused_wid);
