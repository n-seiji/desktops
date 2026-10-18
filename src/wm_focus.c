#include "wm.h"
#include "wm_priv.h"
#include "ax.h"
#include "border.h"
#include "config.h"
#include "menubar.h"
#include "shown.h"
#include <dispatch/dispatch.h>

// Parking, switching and focus. A window of an inactive desktop sits at the bottom-right corner of
// the main display; a desktop switch parks one set and unparks another, and a parked window that
// macOS focuses on its own is pulled to the current desktop.

static uint32_t last_focus[JD_MAX_WS + 1];
static uint32_t stale; // parked window macOS still reports as focused
static double settle_until; // until then, parked windows macOS activates on its own are not pulled

// After a desktop change macOS hands activation from one parked window's app to the next,
// about a second apart; each one extends the quiet period a user's Cmd-Tab has to wait out.
#define SETTLE_S 1.5

// ---- hide / show / focus ----

static CGPoint hide_point(void) {
    return shown_park_point(CGDisplayBounds(CGMainDisplayID()));
}

void wm_hide(win_t *w) {
    if (w->hidden) return;
    CGPoint p;
    if (!ax_value(w->el, kAXPositionAttribute, kAXValueTypeCGPoint, &p)) return;
    w->saved = p;
    ax_set_position(w->el, hide_point());
    w->hidden = true;
}

static void show(win_t *w) {
    if (!w->hidden) return;
    ax_set_position(w->el, w->saved);
    w->hidden = false;
    w->shown_at = CFAbsoluteTimeGetCurrent();
}

static void focus(win_t *w) {
    app_t *a = wm_find_app(w->pid);
    if (a) AXUIElementSetAttributeValue(a->el, kAXFrontmostAttribute, kCFBooleanTrue);
    AXUIElementSetAttributeValue(w->el, kAXMainAttribute, kCFBooleanTrue);
    AXUIElementPerformAction(w->el, kAXRaiseAction);
}

static win_t *first_in(int ws) {
    for (int i = 0; i < nwins; i++)
        if (wins[i].ws == ws) return &wins[i];
    return NULL;
}

static win_t *focus_in_current(void) {
    win_t *t = wm_find_win(last_focus[cur_ws]);
    if (!t || t->ws != cur_ws) t = first_in(cur_ws);
    if (t) focus(t);
    return t;
}

// ---- ui ----

// w is the focused window, looked up once by the caller: each lookup is a round trip to the app.
static void refresh_border(win_t *w) {
    if (!config_border()) return;
    CGPoint p;
    CGSize s;
    if (!w || w->hidden || w->ws != cur_ws ||
        w->minimized || ax_bool(w->el, CFSTR("AXFullScreen")) ||
        !ax_value(w->el, kAXPositionAttribute, kAXValueTypeCGPoint, &p) ||
        !ax_value(w->el, kAXSizeAttribute, kAXValueTypeCGSize, &s)) {
        border_hide();
        return;
    }
    border_show((CGRect){p, s}, w->wid);
}

void wm_refresh_ui(void) {
    if (!config_border() && !menubar_ready()) return;
    win_t *f = wm_focused();
    refresh_border(f);
    wm_refresh_menubar(f);
}

// ---- focus and desktop changes ----

// While settling, a parked window that takes focus is left alone, so nothing would look at it again once the
// quiet period ends. One check per quiet period is queued for then; only the newest one runs.
static unsigned recheck_gen;

static void recheck_after_settle(void) {
    unsigned gen = ++recheck_gen;
    double wait = settle_until - CFAbsoluteTimeGetCurrent() + 0.02;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (wait > 0 ? wait : 0) * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        if (gen == recheck_gen) wm_on_focus();
    });
}

void wm_on_focus(void) {
    win_t *w = wm_focused();
    if (w && w->ws != cur_ws) {
        // A parked window took focus (Cmd-Tab, Dock, launch, unhide): pull it here, unless macOS is still
        // settling after a desktop change or it is the one we just parked.
        double t = CFAbsoluteTimeGetCurrent();
        bool settling = t < settle_until;
        if (settling) {
            settle_until = t + SETTLE_S;
            recheck_after_settle();
        }
        if (settling || w->wid == stale) return;
        w->ws = cur_ws;
        show(w);
    }
    if (w) {
        stale = 0;
        last_focus[cur_ws] = w->wid;
    }
    refresh_border(w);
    wm_refresh_menubar(w);
}

// WindowServer may not list the windows just shown or focused as on screen yet: check again once it does.
static void refresh_menubar_later(void) {
    if (!menubar_ready()) return;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (FRESH_S + 0.05) * NSEC_PER_SEC), dispatch_get_main_queue(), ^{ wm_refresh_menubar(wm_focused()); });
}

void wm_switch(int ws) {
    if (ws < 1 || ws > config_workspaces() || ws == cur_ws) return;
    win_t *f = wm_focused();
    if (f && f->ws == cur_ws) last_focus[cur_ws] = f->wid;

    int old = cur_ws;
    cur_ws = ws;
    stale = f && f->ws == old ? f->wid : 0;
    for (int i = 0; i < nwins; i++)
        if (wins[i].ws == ws) show(&wins[i]);
    for (int i = 0; i < nwins; i++)
        if (wins[i].ws == old) wm_hide(&wins[i]);

    win_t *t = focus_in_current();
    double now = CFAbsoluteTimeGetCurrent();
    settle_until = now + SETTLE_S;
    wm_mark_fresh(t ? t->wid : 0, now);
    wm_refresh_ui();
    refresh_menubar_later();
}

void wm_move_focused(int ws) {
    if (ws < 1 || ws > config_workspaces()) return;
    win_t *w = wm_focused();
    if (!w || w->ws == ws) return;
    w->ws = ws;
    last_focus[ws] = w->wid;
    if (ws != cur_ws) {
        wm_hide(w);
        stale = w->wid;
        focus_in_current();
        settle_until = CFAbsoluteTimeGetCurrent() + SETTLE_S;
    }
    wm_refresh_ui();
}

void wm_focus_wid(uint32_t wid) {
    win_t *w = wm_find_win(wid);
    if (!w || w->ws != cur_ws) return;
    last_focus[cur_ws] = wid;
    focus(w);
    wm_mark_fresh(wid, CFAbsoluteTimeGetCurrent());
    wm_refresh_ui();
    refresh_menubar_later();
}

void wm_restore_all(void) {
    for (int i = 0; i < nwins; i++) show(&wins[i]);
}
