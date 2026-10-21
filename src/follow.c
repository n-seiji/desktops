#include "follow.h"

focus_act_t focus_act(bool parked, bool settling, bool recheck, bool is_stale, bool cur_has_win) {
    if (!parked) return FOCUS_RECORD;
    if (settling || is_stale) return FOCUS_IGNORE;
    if (recheck) return cur_has_win ? FOCUS_REFOCUS : FOCUS_STALE;
    return FOCUS_FOLLOW;
}

follow_act_t follow_check(bool still_focused, bool now_here, bool settling, bool cur_left) {
    if (!still_focused || now_here || settling) return FOLLOW_DROP;
    return cur_left ? FOLLOW_LEFT : FOLLOW_SWITCH;
}
