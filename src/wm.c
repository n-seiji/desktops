#include "jd.h"
#include "extern.h"
#include "order.h"
#include "select.h"
#include "shown.h"
#include <dispatch/dispatch.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Workspaces are emulated like AeroSpace does: windows of inactive
// workspaces are parked at the bottom-right corner of the main display,
// so macOS Spaces / SIP are never touched.

typedef struct {
    AXUIElementRef el;
    pid_t pid;
    uint32_t wid;
    int ws;
    CGPoint saved;
    bool hidden;
    double shown_at; // when show() last unparked it
    bool was_listed; // WindowServer listed it the last time we looked
    bool minimized;  // kept from the (de)miniaturize notifications: asking a hung app costs a timeout per window
} win_t;

typedef struct {
    pid_t pid;
    AXUIElementRef el;
    AXObserverRef obs;
    int assign;
    bool nopreview;
} app_t;

static win_t *wins;
static int nwins, capwins;
static app_t *apps;
static int napps, capapps;
static int cur = 1;
static uint32_t last_focus[JD_MAX_WS + 1];
static bool border_on = true;
static uint32_t stale; // parked window macOS still reports as focused
static double fresh_until; // until then, the menu bar prefers the window we just focused...
static uint32_t fresh_wid; // ...to the one macOS reports as focused, which lags behind
static double settle_until; // until then, parked windows macOS activates on its own are not pulled
static AXUIElementRef sys;

// After a desktop change macOS hands activation from one parked window's app to the next,
// about a second apart; each one extends the quiet period a user's Cmd-Tab has to wait out.
#define SETTLE_S 1.5

static void ax_cb(AXObserverRef obs, AXUIElementRef el, CFStringRef n, void *ctx);
static void refresh_menubar(win_t *f);

// ---- AX helpers ----

static bool get_value(AXUIElementRef el, CFStringRef attr, AXValueType type, void *out) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(el, attr, &v) != kAXErrorSuccess || !v) return false;
    bool ok = CFGetTypeID(v) == AXValueGetTypeID() && AXValueGetValue(v, type, out);
    CFRelease(v);
    return ok;
}

static bool get_bool(AXUIElementRef el, CFStringRef attr) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(el, attr, &v) != kAXErrorSuccess || !v) return false;
    bool r = CFGetTypeID(v) == CFBooleanGetTypeID() && CFBooleanGetValue(v);
    CFRelease(v);
    return r;
}

static bool attr_is(AXUIElementRef el, CFStringRef attr, CFStringRef want) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(el, attr, &v) != kAXErrorSuccess || !v) return false;
    bool r = CFGetTypeID(v) == CFStringGetTypeID() && CFEqual(v, want);
    CFRelease(v);
    return r;
}

static void set_pos(AXUIElementRef el, CGPoint p) {
    AXValueRef v = AXValueCreate(kAXValueTypeCGPoint, &p);
    AXUIElementSetAttributeValue(el, kAXPositionAttribute, v);
    CFRelease(v);
}

static CGPoint hide_point(void) {
    return shown_park_point(CGDisplayBounds(CGMainDisplayID()));
}

// ---- lookup ----

static app_t *find_app(pid_t pid) {
    for (int i = 0; i < napps; i++)
        if (apps[i].pid == pid) return &apps[i];
    return NULL;
}

static win_t *find_wid(uint32_t wid) {
    if (!wid) return NULL;
    for (int i = 0; i < nwins; i++)
        if (wins[i].wid == wid) return &wins[i];
    return NULL;
}

static win_t *first_in(int ws) {
    for (int i = 0; i < nwins; i++)
        if (wins[i].ws == ws) return &wins[i];
    return NULL;
}

static void remove_win_at(int i) {
    CFRelease(wins[i].el);
    memmove(&wins[i], &wins[i + 1], sizeof *wins * (--nwins - i)); // keep order: cycling relies on it
}

// ---- hide / show / focus ----

static void hide(win_t *w) {
    if (w->hidden) return;
    CGPoint p;
    if (!get_value(w->el, kAXPositionAttribute, kAXValueTypeCGPoint, &p)) return;
    w->saved = p;
    set_pos(w->el, hide_point());
    w->hidden = true;
}

static void show(win_t *w) {
    if (!w->hidden) return;
    set_pos(w->el, w->saved);
    w->hidden = false;
    w->shown_at = CFAbsoluteTimeGetCurrent();
}

static void focus(win_t *w) {
    app_t *a = find_app(w->pid);
    if (a) AXUIElementSetAttributeValue(a->el, kAXFrontmostAttribute, kCFBooleanTrue);
    AXUIElementSetAttributeValue(w->el, kAXMainAttribute, kCFBooleanTrue);
    AXUIElementPerformAction(w->el, kAXRaiseAction);
}

static win_t *focus_in_current(void) {
    win_t *t = find_wid(last_focus[cur]);
    if (!t || t->ws != cur) t = first_in(cur);
    if (t) focus(t);
    return t;
}

// ---- tracking ----

static win_t *track(app_t *a, AXUIElementRef el) {
    uint32_t wid = 0;
    if (_AXUIElementGetWindow(el, &wid) != kAXErrorSuccess || !wid) return NULL;
    win_t *w = find_wid(wid);
    if (w) return w;
    if (!attr_is(el, kAXRoleAttribute, kAXWindowRole)) return NULL;
    if (!attr_is(el, kAXSubroleAttribute, kAXStandardWindowSubrole) &&
        !attr_is(el, kAXSubroleAttribute, kAXDialogSubrole))
        return NULL;

    if (nwins == capwins) {
        int cap = capwins ? capwins * 2 : 32;
        win_t *n = realloc(wins, sizeof *wins * cap);
        if (!n) return NULL;
        wins = n;
        capwins = cap;
    }
    w = &wins[nwins++];
    *w = (win_t){(AXUIElementRef)CFRetain(el), a->pid, wid, a->assign ? a->assign : cur, {0, 0}, false, 0, false,
                 get_bool(el, kAXMinimizedAttribute)};

    // A window left in the corner by a previous crash: bring it back.
    CGPoint p;
    if (get_value(el, kAXPositionAttribute, kAXValueTypeCGPoint, &p) && shown_is_parked(p, CGDisplayBounds(CGMainDisplayID()))) {
        CGRect b = CGDisplayBounds(CGMainDisplayID());
        set_pos(el, CGPointMake(b.origin.x + 40, b.origin.y + 60));
    }

    void *ctx = (void *)(intptr_t)a->pid;
    AXObserverAddNotification(a->obs, el, kAXUIElementDestroyedNotification, ctx);
    AXObserverAddNotification(a->obs, el, kAXWindowMiniaturizedNotification, ctx);
    AXObserverAddNotification(a->obs, el, kAXWindowDeminiaturizedNotification, ctx);

    if (w->ws != cur) hide(w);
    return w;
}

static win_t *focused(void) {
    AXUIElementRef app = NULL, win = NULL;
    win_t *w = NULL;
    // The system-wide query fails for some apps (Slack: kAXErrorCannotComplete); ask the frontmost app itself.
    if (AXUIElementCopyAttributeValue(sys, kAXFocusedApplicationAttribute, (CFTypeRef *)&app) != kAXErrorSuccess || !app) {
        app_t *a = find_app(apps_frontmost());
        if (!a) return NULL;
        app = (AXUIElementRef)CFRetain(a->el);
    }
    if (AXUIElementCopyAttributeValue(app, kAXFocusedWindowAttribute, (CFTypeRef *)&win) == kAXErrorSuccess && win) {
        uint32_t wid = 0;
        if (_AXUIElementGetWindow(win, &wid) == kAXErrorSuccess) w = find_wid(wid);
        if (!w) {
            pid_t pid = 0;
            AXUIElementGetPid(app, &pid);
            app_t *a = find_app(pid);
            if (a) w = track(a, win);
        }
        CFRelease(win);
    }
    CFRelease(app);
    return w;
}

// f is the focused window, looked up once by the caller: each lookup is a round trip to the app.
static void refresh_border(win_t *w) {
    if (!border_on) return;
    CGPoint p;
    CGSize s;
    if (!w || w->hidden || w->ws != cur ||
        w->minimized || get_bool(w->el, CFSTR("AXFullScreen")) ||
        !get_value(w->el, kAXPositionAttribute, kAXValueTypeCGPoint, &p) ||
        !get_value(w->el, kAXSizeAttribute, kAXValueTypeCGSize, &s)) {
        border_hide();
        return;
    }
    border_show((CGRect){p, s}, w->wid);
}

static void refresh_ui(void) {
    if (!border_on && !menubar_ready()) return;
    win_t *f = focused();
    refresh_border(f);
    refresh_menubar(f);
}

// ---- apps ----

static void add_app(pid_t pid, int assign, bool nopreview, int attempt) {
    if (find_app(pid)) return;
    AXObserverRef obs = NULL;
    if (AXObserverCreate(pid, ax_cb, &obs) != kAXErrorSuccess) return;
    AXUIElementRef el = AXUIElementCreateApplication(pid);
    AXUIElementSetMessagingTimeout(el, 1.0);
    void *ctx = (void *)(intptr_t)pid;

    AXError err = AXObserverAddNotification(obs, el, kAXWindowCreatedNotification, ctx);
    if (err != kAXErrorSuccess && err != kAXErrorNotificationAlreadyRegistered) {
        CFRelease(obs);
        CFRelease(el);
        // Freshly launched apps are often not ready for AX yet.
        if (err == kAXErrorCannotComplete && attempt < 20)
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC), dispatch_get_main_queue(),
                           ^{ add_app(pid, assign, nopreview, attempt + 1); });
        return;
    }
    AXObserverAddNotification(obs, el, kAXFocusedWindowChangedNotification, ctx);
    AXObserverAddNotification(obs, el, kAXMainWindowChangedNotification, ctx);
    CFRunLoopAddSource(CFRunLoopGetMain(), AXObserverGetRunLoopSource(obs), kCFRunLoopDefaultMode);

    if (napps == capapps) {
        int cap = capapps ? capapps * 2 : 32;
        app_t *n = realloc(apps, sizeof *apps * cap);
        if (!n) return;
        apps = n;
        capapps = cap;
    }
    apps[napps++] = (app_t){pid, el, obs, assign, nopreview};

    CFArrayRef list = NULL;
    if (AXUIElementCopyAttributeValue(el, kAXWindowsAttribute, (CFTypeRef *)&list) == kAXErrorSuccess && list) {
        for (CFIndex i = 0; i < CFArrayGetCount(list); i++)
            track(find_app(pid), CFArrayGetValueAtIndex(list, i));
        CFRelease(list);
    }
    if (attempt > 0) wm_on_focus();
}

void wm_add_app(pid_t pid, const char *bundle_id) {
    add_app(pid, config_assign(bundle_id), config_no_preview(bundle_id), 0);
    if (menubar_ready()) refresh_menubar(focused());
}

void wm_remove_app(pid_t pid) {
    menubar_forget(pid);
    for (int i = nwins - 1; i >= 0; i--)
        if (wins[i].pid == pid) remove_win_at(i);
    for (int i = 0; i < napps; i++) {
        if (apps[i].pid != pid) continue;
        CFRunLoopRemoveSource(CFRunLoopGetMain(), AXObserverGetRunLoopSource(apps[i].obs), kCFRunLoopDefaultMode);
        CFRelease(apps[i].obs);
        CFRelease(apps[i].el);
        apps[i] = apps[--napps];
        return;
    }
}

static void ax_cb(AXObserverRef obs, AXUIElementRef el, CFStringRef n, void *ctx) {
    (void)obs;
    app_t *a = find_app((pid_t)(intptr_t)ctx);
    if (!a) return;
    if (CFEqual(n, kAXWindowCreatedNotification)) {
        track(a, el);
        wm_on_focus();
    } else if (CFEqual(n, kAXUIElementDestroyedNotification)) {
        for (int i = 0; i < nwins; i++)
            if (CFEqual(wins[i].el, el)) { remove_win_at(i); break; }
        wm_on_focus();
    } else if (CFEqual(n, kAXFocusedWindowChangedNotification) || CFEqual(n, kAXMainWindowChangedNotification)) {
        wm_on_focus();
    } else {
        bool mini = CFEqual(n, kAXWindowMiniaturizedNotification);
        for (int i = 0; i < nwins; i++)
            if (CFEqual(wins[i].el, el)) { wins[i].minimized = mini; break; }
        refresh_ui();
    }
}

// ---- public ----

void wm_init(void) {
    sys = AXUIElementCreateSystemWide();
    AXUIElementSetMessagingTimeout(sys, 1.0);
    border_on = config_border();
}

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
    win_t *w = focused();
    if (w && w->ws != cur) {
        // A parked window took focus (Cmd-Tab, Dock, launch, unhide): pull it here, unless macOS is still
        // settling after a desktop change or it is the one we just parked.
        double t = CFAbsoluteTimeGetCurrent();
        bool settling = t < settle_until;
        if (settling) {
            settle_until = t + SETTLE_S;
            recheck_after_settle();
        }
        if (settling || w->wid == stale) return;
        w->ws = cur;
        show(w);
    }
    if (w) {
        stale = 0;
        last_focus[cur] = w->wid;
    }
    refresh_border(w);
    refresh_menubar(w);
}

void wm_switch(int ws) {
    if (ws < 1 || ws > config_workspaces() || ws == cur) return;
    win_t *f = focused();
    if (f && f->ws == cur) last_focus[cur] = f->wid;

    int old = cur;
    cur = ws;
    stale = f && f->ws == old ? f->wid : 0;
    for (int i = 0; i < nwins; i++)
        if (wins[i].ws == ws) show(&wins[i]);
    for (int i = 0; i < nwins; i++)
        if (wins[i].ws == old) hide(&wins[i]);

    win_t *t = focus_in_current();
    double now = CFAbsoluteTimeGetCurrent();
    settle_until = now + SETTLE_S;
    fresh_until = now + FRESH_S;
    fresh_wid = t ? t->wid : 0;
    refresh_ui();
    // WindowServer may not list the windows just shown as on screen yet: check again once it does.
    if (menubar_ready())
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (FRESH_S + 0.05) * NSEC_PER_SEC), dispatch_get_main_queue(), ^{ refresh_menubar(focused()); });
}

void wm_move_focused(int ws) {
    if (ws < 1 || ws > config_workspaces()) return;
    win_t *w = focused();
    if (!w || w->ws == ws) return;
    w->ws = ws;
    last_focus[ws] = w->wid;
    if (ws != cur) {
        hide(w);
        stale = w->wid;
        focus_in_current();
        settle_until = CFAbsoluteTimeGetCurrent() + SETTLE_S;
    }
    refresh_ui();
}

static CFStringRef title_of(AXUIElementRef el) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(el, kAXTitleAttribute, &v) != kAXErrorSuccess || !v) return NULL;
    if (CFGetTypeID(v) == CFStringGetTypeID()) return v;
    CFRelease(v);
    return NULL;
}

int wm_current(void) {
    return cur;
}

// Replaces each entry's title with its row text: see title_labels.
static void label_entries(sw_entry_t *list, int n) {
    int *at = malloc(sizeof *at * n);
    CFStringRef *titles = malloc(sizeof *titles * n * 2), *labels = titles + n;
    if (at && titles) {
        for (int i = 0; i < n; i++) {
            bool first = list[i].app != NULL;
            for (int j = 0; j < i && first; j++) first = !list[j].app || !CFEqual(list[j].app, list[i].app);
            if (!first) continue;
            int k = 0;
            for (int j = i; j < n; j++) {
                if (!list[j].app || !CFEqual(list[j].app, list[i].app)) continue;
                at[k] = j;
                titles[k++] = list[j].title;
            }
            title_labels(list[i].app, titles, k, labels);
            for (int m = 0; m < k; m++) {
                if (list[at[m]].title) CFRelease(list[at[m]].title);
                list[at[m]].title = labels[m];
            }
        }
    }
    free(at);
    free(titles);
}

// WindowServer's view of the windows that are really shown right now: on screen, not
// transparent, and with a visible part on a display. NULL when the list is unavailable.
static CFArrayRef shown_windows(void) {
    return CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
                                      kCGNullWindowID);
}

static bool is_shown(CFArrayRef shown, uint32_t wid) {
    if (!shown) return true;
    CGDirectDisplayID ds[8];
    uint32_t nd = 0;
    CGGetActiveDisplayList(8, ds, &nd);
    CGRect bounds[8];
    for (CFIndex i = 0; i < CFArrayGetCount(shown); i++) {
        CFDictionaryRef d = CFArrayGetValueAtIndex(shown, i);
        int num = 0;
        CFNumberRef n = CFDictionaryGetValue(d, kCGWindowNumber);
        if (!n || !CFNumberGetValue(n, kCFNumberIntType, &num) || (uint32_t)num != wid) continue;
        double alpha = 1;
        n = CFDictionaryGetValue(d, kCGWindowAlpha);
        if (n) CFNumberGetValue(n, kCFNumberDoubleType, &alpha);
        CGRect r;
        CFDictionaryRef b = CFDictionaryGetValue(d, kCGWindowBounds);
        if (alpha <= 0 || !b || !CGRectMakeWithDictionaryRepresentation(b, &r)) return false;
        for (uint32_t k = 0; k < nd; k++) bounds[k] = CGDisplayBounds(ds[k]);
        return shown_visible(alpha, r, bounds, nd);
    }
    return false;
}

// Switchable windows of the current workspace in array order, skipping any WindowServer
// does not show (hidden app, other Space, alpha 0, parked off screen). *out is malloc'd with
// +1 icon/app/title per entry (title is the row text; the caller releases them); *sel is the window after the
// focused one (before it for dir < 0).
static bool switchable(win_t *w, CFArrayRef shown) {
    return w->ws == cur && !w->hidden && !w->minimized && is_shown(shown, w->wid);
}

int wm_windows(int dir, sw_entry_t **out, int *sel) {
    sw_entry_t *list = malloc(sizeof *list * (nwins ? nwins : 1));
    if (!list) return 0;
    win_t *f = focused();
    int n = 0, at = -1;
    CFArrayRef shown = shown_windows();
    for (int i = 0; i < nwins; i++) {
        win_t *w = &wins[i];
        if (!switchable(w, shown)) continue;
        if (w == f) at = n;
        CFStringRef app = apps_name(w->pid), raw = title_of(w->el);
        app_t *own = find_app(w->pid);
        list[n++] = (sw_entry_t){w->wid, w->pid, apps_icon(w->pid, 56), app, raw ? title_clean(raw, app) : NULL,
                                 own && own->nopreview};
        if (raw) CFRelease(raw);
    }
    if (shown) CFRelease(shown);
    label_entries(list, n);
    *sel = sel_start(at, dir, n);
    *out = list;
    return n;
}

// The same windows in the same order as wm_windows, as bare ids: no icons, names or titles.
// *out is malloc'd; *sel as in wm_windows.
int wm_window_ids(int dir, uint32_t **out, int *sel) {
    uint32_t *ids = malloc(sizeof *ids * (nwins ? nwins : 1));
    if (!ids) return 0;
    win_t *f = focused();
    int n = 0, at = -1;
    CFArrayRef shown = shown_windows();
    for (int i = 0; i < nwins; i++) {
        if (!switchable(&wins[i], shown)) continue;
        if (&wins[i] == f) at = n;
        ids[n++] = wins[i].wid;
    }
    if (shown) CFRelease(shown);
    *sel = sel_start(at, dir, n);
    *out = ids;
    return n;
}

// The switchable windows of this desktop for the menu bar, grouped by app in order of each
// app's first window, an app's windows in tracking order, so focus changes never reorder.
// Returns the count; pids and wids hold up to JD_MENUBAR_WINS.
static int menubar_order(pid_t *pids, uint32_t *wids) {
    double now = CFAbsoluteTimeGetCurrent();
    pid_t vpid[JD_MENUBAR_WINS];
    uint32_t vwid[JD_MENUBAR_WINS];
    int nv = 0;
    CFArrayRef shown = shown_windows();
    for (int i = 0; i < nwins && nv < JD_MENUBAR_WINS; i++) {
        win_t *w = &wins[i];
        if (w->ws != cur || w->hidden || w->minimized) continue;
        if (!shown_grace(is_shown(shown, w->wid), &w->was_listed, now, w->shown_at)) continue;
        vpid[nv] = w->pid;
        vwid[nv++] = w->wid;
    }
    if (shown) CFRelease(shown);
    order_group(vpid, vwid, nv, pids, wids);
    return nv;
}

static void refresh_menubar(win_t *f) {
    if (!menubar_ready()) return; // apps_init runs before menubar_init: the first draw is wm_on_focus in main
    double now = CFAbsoluteTimeGetCurrent();
    uint32_t front = shown_front(now, fresh_until, fresh_wid, f ? f->wid : 0);
    pid_t pids[JD_MENUBAR_WINS];
    uint32_t wids[JD_MENUBAR_WINS];
    int n = menubar_order(pids, wids), shown_n, over;
    order_split(n, JD_MENUBAR_SLOTS, &shown_n, &over);
    menubar_update(cur, pids, wids, shown_n, over, order_hash(wids + shown_n, over), front);
}

// The windows behind the menu bar's "+k" slot, in menu bar order, with +1 icon/app/title
// as wm_windows has. *out is malloc'd; the caller releases and frees. Titles are labelled
// across all of the desktop's windows, so an app with one hidden window and others shown
// still gets its title.
int wm_overflow(sw_entry_t **out) {
    pid_t pids[JD_MENUBAR_WINS];
    uint32_t wids[JD_MENUBAR_WINS];
    int n = menubar_order(pids, wids), skip, over;
    order_split(n, JD_MENUBAR_SLOTS, &skip, &over);
    *out = NULL;
    if (!over) return 0;
    sw_entry_t *list = calloc(n, sizeof *list);
    if (!list) return 0;
    for (int i = 0; i < n; i++) {
        win_t *w = find_wid(wids[i]);
        CFStringRef app = apps_name(pids[i]), raw = w ? title_of(w->el) : NULL;
        list[i] = (sw_entry_t){wids[i], pids[i], NULL, app, raw && app ? title_clean(raw, app) : NULL, false};
        if (raw) CFRelease(raw);
    }
    label_entries(list, n);
    for (int i = 0; i < skip; i++) {
        if (list[i].app) CFRelease(list[i].app);
        if (list[i].title) CFRelease(list[i].title);
    }
    memmove(list, list + skip, sizeof *list * (n - skip));
    for (int i = 0; i < n - skip; i++) list[i].icon = apps_icon(list[i].pid, 36);
    *out = list;
    return over;
}

void wm_focus_wid(uint32_t wid) {
    win_t *w = find_wid(wid);
    if (!w || w->ws != cur) return;
    last_focus[cur] = wid;
    focus(w);
    fresh_until = CFAbsoluteTimeGetCurrent() + FRESH_S;
    fresh_wid = wid;
    refresh_ui();
    if (!menubar_ready()) return;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (FRESH_S + 0.05) * NSEC_PER_SEC), dispatch_get_main_queue(), ^{ refresh_menubar(focused()); });
}

void wm_restore_all(void) {
    for (int i = 0; i < nwins; i++) show(&wins[i]);
}
