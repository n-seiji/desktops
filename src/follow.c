#include "follow.h"

focus_act_t focus_act(bool parked, bool settling, bool recheck, bool is_stale, bool cur_has_win) {
    if (!parked) return FOCUS_RECORD;
    if (settling || is_stale) return FOCUS_IGNORE;
    if (recheck) return cur_has_win ? FOCUS_REFOCUS : FOCUS_STALE;
    return FOCUS_FOLLOW;
}

follow_act_t follow_check(bool still_focused, bool now_here, bool settling, bool cur_left) {
    if (!still_focused || now_here) return FOLLOW_DROP;
    if (settling) return FOLLOW_WAIT;
    return cur_left ? FOLLOW_LEFT : FOLLOW_SWITCH;
}

bool cur_left(bool tracked_here, bool known_minimized, bool alive, ax_read_t read, bool ax_minimized) {
    if (!tracked_here || known_minimized) return false;
    if (!alive) return true;
    return read == READ_INVALID || read == READ_BUSY || (read == READ_OK && ax_minimized);
}
