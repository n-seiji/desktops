#include "shown.h"
#include <math.h>

bool shown_visible(double alpha, CGRect r, const CGRect *displays, uint32_t nd) {
    if (alpha <= 0) return false;
    for (uint32_t k = 0; k < nd; k++) {
        CGRect v = CGRectIntersection(r, displays[k]);
        if (!CGRectIsNull(v) && r.size.width > 0 && r.size.height > 0 &&
            v.size.width >= fmin(r.size.width, 32) && v.size.height >= fmin(r.size.height, 32))
            return true;
    }
    return false;
}

bool shown_grace(bool listed, bool *was_listed, double now, double shown_at) {
    if (now - shown_at > FRESH_S) {
        *was_listed = listed;
        return listed;
    }
    return listed || *was_listed;
}

uint32_t shown_front(double now, double fresh_until, uint32_t fresh_wid, uint32_t focused_wid) {
    return now < fresh_until && fresh_wid ? fresh_wid : focused_wid;
}
