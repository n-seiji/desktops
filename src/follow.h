#pragma once
// What to do when macOS focuses a window.
#include <stdbool.h>

typedef enum {
    FOCUS_RECORD,  // the window is on this desktop: note it
    FOCUS_FOLLOW,  // a parked window was called for: check again shortly, then switch to its desktop
    FOCUS_REFOCUS, // a parked window after a switch settled, and this desktop has windows: focus one of them
    FOCUS_STALE,   // same, but this desktop is empty: leave the parked window focused and ignore it
    FOCUS_IGNORE,  // macOS handing focus around after a switch or a close, or the window we just parked
} focus_act_t;

focus_act_t focus_act(bool parked, bool settling, bool recheck, bool is_stale, bool cur_has_win);

typedef enum {
    FOLLOW_DROP,   // the call is over or handled elsewhere: do nothing
    FOLLOW_LEFT,   // macOS focused the parked window because the user's window here closed, minimized or quit
    FOLLOW_SWITCH, // the call stands: switch to the window's desktop
} follow_act_t;

// macOS focuses another window of the app about 0.7 s before it reports that the focused one was minimized,
// so a call is acted on only after a short delay, when its cause is clearer.
// still_focused: the window is still the focused one. now_here: it is on this desktop now.
// settling: a quiet period is active. cur_left: the window last used on this desktop is minimized, closed or its app quit.
follow_act_t follow_check(bool still_focused, bool now_here, bool settling, bool cur_left);
