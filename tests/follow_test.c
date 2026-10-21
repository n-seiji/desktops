#include "../src/follow.h"
#include <stdio.h>

static int failed;

static const char *name(focus_act_t a) {
    static const char *n[] = {"record", "follow", "refocus", "stale", "ignore"};
    return n[a];
}

int main(void) {
    static const struct {
        const char *what;
        bool parked, settling, recheck, is_stale, cur_has_win;
        focus_act_t want;
    } t[] = {
        {"window of this desktop", false, false, false, false, true, FOCUS_RECORD},
        {"window of this desktop while settling", false, true, false, false, true, FOCUS_RECORD},
        {"window of this desktop at recheck", false, false, true, false, true, FOCUS_RECORD},
        {"parked window called for", true, false, false, false, true, FOCUS_FOLLOW},
        {"parked window called for, desktop empty", true, false, false, false, false, FOCUS_FOLLOW},
        {"parked window while settling", true, true, false, false, true, FOCUS_IGNORE},
        {"parked window while settling, at recheck", true, true, true, false, true, FOCUS_IGNORE},
        {"the window just parked", true, false, false, true, true, FOCUS_IGNORE},
        {"the window just parked, at recheck", true, false, true, true, true, FOCUS_IGNORE},
        {"parked window at recheck", true, false, true, false, true, FOCUS_REFOCUS},
        {"parked window at recheck, desktop empty", true, false, true, false, false, FOCUS_STALE},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        focus_act_t got = focus_act(t[i].parked, t[i].settling, t[i].recheck, t[i].is_stale, t[i].cur_has_win);
        if (got == t[i].want) continue;
        printf("FAIL %s = %s; want %s\n", t[i].what, name(got), name(t[i].want));
        failed++;
    }
    static const struct {
        const char *what;
        bool still_focused, now_here, settling, cur_left;
        follow_act_t want;
    } c[] = {
        {"call still stands", true, false, false, false, FOLLOW_SWITCH},
        {"focus moved on", false, false, false, false, FOLLOW_DROP},
        {"focus moved on, window left", false, false, false, true, FOLLOW_DROP},
        {"window is now here", true, true, false, false, FOLLOW_DROP},
        {"settling started meanwhile", true, false, true, false, FOLLOW_WAIT},
        {"settling started meanwhile, window left", true, false, true, true, FOLLOW_WAIT},
        {"settling started, focus moved on", false, false, true, false, FOLLOW_DROP},
        {"settling started, window is now here", true, true, true, false, FOLLOW_DROP},
        {"window here was minimized, closed or quit", true, false, false, true, FOLLOW_LEFT},
    };
    static const char *const an[] = {"drop", "wait", "left", "switch"};
    for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
        follow_act_t got = follow_check(c[i].still_focused, c[i].now_here, c[i].settling, c[i].cur_left);
        if (got == c[i].want) continue;
        printf("FAIL check: %s = %s; want %s\n", c[i].what, an[got], an[c[i].want]);
        failed++;
    }
    static const struct {
        const char *what;
        bool tracked_here, known_minimized, alive;
        ax_read_t read;
        bool ax_minimized, want;
    } l[] = {
        {"window is fine", true, false, true, READ_OK, false, false},
        {"minimize in flight", true, false, true, READ_OK, true, true},
        {"close in flight", true, false, true, READ_INVALID, false, true},
        {"quit in flight", true, false, false, READ_OK, false, true},
        {"app busy or quitting", true, false, true, READ_BUSY, false, true},
        {"other read error", true, false, true, READ_OTHER, false, false},
        {"already minimized", true, true, true, READ_OK, true, false},
        {"already minimized, app quit since", true, true, false, READ_OK, true, false},
        {"already removed", false, false, false, READ_OTHER, false, false},
        {"moved to another desktop", false, false, true, READ_OK, true, false},
        {"never had a window here", false, false, true, READ_OTHER, false, false},
    };
    for (unsigned i = 0; i < sizeof l / sizeof l[0]; i++) {
        bool got = cur_left(l[i].tracked_here, l[i].known_minimized, l[i].alive, l[i].read, l[i].ax_minimized);
        if (got == l[i].want) continue;
        printf("FAIL cur_left: %s = %d; want %d\n", l[i].what, got, l[i].want);
        failed++;
    }
    if (!failed) printf("ok\n");
    return failed != 0;
}
