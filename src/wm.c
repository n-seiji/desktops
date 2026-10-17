#include "jd.h"
#include "extern.h"
#include <dispatch/dispatch.h>
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
} win_t;

typedef struct {
    pid_t pid;
    AXUIElementRef el;
    AXObserverRef obs;
    int assign;
} app_t;

static win_t *wins;
static int nwins, capwins;
static app_t *apps;
static int napps, capapps;
static int cur = 1;
static uint32_t last_focus[JD_MAX_WS + 1];
static bool border_on = true;
static double follow_block_until;
static AXUIElementRef sys;

static void ax_cb(AXObserverRef obs, AXUIElementRef el, CFStringRef n, void *ctx);

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
    CGRect b = CGDisplayBounds(CGMainDisplayID());
    return CGPointMake(CGRectGetMaxX(b) - 1, CGRectGetMaxY(b) - 1);
}

static bool near_hide_point(CGPoint p) {
    CGPoint h = hide_point();
    return p.x >= h.x - 4 && p.y >= h.y - 64;
}

static double now(void) { return CFAbsoluteTimeGetCurrent(); }

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
}

static void focus(win_t *w) {
    app_t *a = find_app(w->pid);
    if (a) AXUIElementSetAttributeValue(a->el, kAXFrontmostAttribute, kCFBooleanTrue);
    AXUIElementSetAttributeValue(w->el, kAXMainAttribute, kCFBooleanTrue);
    AXUIElementPerformAction(w->el, kAXRaiseAction);
}

static void focus_in_current(void) {
    win_t *t = find_wid(last_focus[cur]);
    if (!t || t->ws != cur) t = first_in(cur);
    if (t) focus(t);
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
    *w = (win_t){(AXUIElementRef)CFRetain(el), a->pid, wid, a->assign ? a->assign : cur, {0, 0}, false};

    // A window left in the corner by a previous crash: bring it back.
    CGPoint p;
    if (get_value(el, kAXPositionAttribute, kAXValueTypeCGPoint, &p) && near_hide_point(p)) {
        CGRect b = CGDisplayBounds(CGMainDisplayID());
        set_pos(el, CGPointMake(b.origin.x + 40, b.origin.y + 60));
    }

    void *ctx = (void *)(intptr_t)a->pid;
    AXObserverAddNotification(a->obs, el, kAXUIElementDestroyedNotification, ctx);
    if (border_on) {
        AXObserverAddNotification(a->obs, el, kAXMovedNotification, ctx);
        AXObserverAddNotification(a->obs, el, kAXResizedNotification, ctx);
    }
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

static void refresh_border(void) {
    if (!border_on) return;
    win_t *w = focused();
    CGPoint p;
    CGSize s;
    if (!w || w->hidden || w->ws != cur ||
        get_bool(w->el, kAXMinimizedAttribute) || get_bool(w->el, CFSTR("AXFullScreen")) ||
        !get_value(w->el, kAXPositionAttribute, kAXValueTypeCGPoint, &p) ||
        !get_value(w->el, kAXSizeAttribute, kAXValueTypeCGSize, &s)) {
        border_hide();
        return;
    }
    border_show((CGRect){p, s}, w->wid);
}

// ---- apps ----

static void add_app(pid_t pid, int assign, int attempt) {
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
                           ^{ add_app(pid, assign, attempt + 1); });
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
    apps[napps++] = (app_t){pid, el, obs, assign};

    CFArrayRef list = NULL;
    if (AXUIElementCopyAttributeValue(el, kAXWindowsAttribute, (CFTypeRef *)&list) == kAXErrorSuccess && list) {
        for (CFIndex i = 0; i < CFArrayGetCount(list); i++)
            track(find_app(pid), CFArrayGetValueAtIndex(list, i));
        CFRelease(list);
    }
    if (attempt > 0) wm_on_focus();
}

void wm_add_app(pid_t pid, const char *bundle_id) {
    add_app(pid, config_assign(bundle_id), 0);
}

void wm_remove_app(pid_t pid) {
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
        refresh_border();
    }
}

// ---- public ----

void wm_init(void) {
    sys = AXUIElementCreateSystemWide();
    AXUIElementSetMessagingTimeout(sys, 1.0);
    border_on = config_border();
}

void wm_on_focus(void) {
    win_t *w = focused();
    if (w && w->ws != cur) {
        // Focus moved to a window on another workspace (Cmd-Tab, Dock, ...): follow it.
        if (now() < follow_block_until) return;
        last_focus[w->ws] = w->wid;
        wm_switch(w->ws);
        return;
    }
    if (w) last_focus[cur] = w->wid;
    refresh_border();
}

void wm_switch(int ws) {
    if (ws < 1 || ws > config_workspaces() || ws == cur) return;
    win_t *f = focused();
    if (f && f->ws == cur) last_focus[cur] = f->wid;

    int old = cur;
    cur = ws;
    for (int i = 0; i < nwins; i++)
        if (wins[i].ws == ws) show(&wins[i]);
    for (int i = 0; i < nwins; i++)
        if (wins[i].ws == old) hide(&wins[i]);

    follow_block_until = now() + 0.4;
    focus_in_current();
    refresh_border();
}

void wm_move_focused(int ws) {
    if (ws < 1 || ws > config_workspaces()) return;
    win_t *w = focused();
    if (!w || w->ws == ws) return;
    w->ws = ws;
    last_focus[ws] = w->wid;
    if (ws != cur) {
        hide(w);
        follow_block_until = now() + 0.4;
        focus_in_current();
    }
    refresh_border();
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

// Switchable windows of the current workspace in array order. *out is malloc'd with
// +1 icon/app/title per entry (title is the row text; the caller releases them); *sel is the window after the
// focused one (before it for dir < 0).
int wm_windows(int dir, sw_entry_t **out, int *sel) {
    sw_entry_t *list = malloc(sizeof *list * (nwins ? nwins : 1));
    if (!list) return 0;
    win_t *f = focused();
    int n = 0, at = -1;
    for (int i = 0; i < nwins; i++) {
        win_t *w = &wins[i];
        if (w->ws != cur || w->hidden || get_bool(w->el, kAXMinimizedAttribute)) continue;
        if (w == f) at = n;
        CFStringRef app = apps_name(w->pid), raw = title_of(w->el);
        list[n++] = (sw_entry_t){w->wid, apps_icon(w->pid, 44), app, raw ? title_clean(raw, app) : NULL};
        if (raw) CFRelease(raw);
    }
    label_entries(list, n);
    *sel = n ? (at < 0 ? (dir > 0 ? 0 : n - 1) : ((at + dir) % n + n) % n) : 0;
    *out = list;
    return n;
}

void wm_focus_wid(uint32_t wid) {
    win_t *w = find_wid(wid);
    if (!w || w->ws != cur) return;
    last_focus[cur] = wid;
    focus(w);
    refresh_border();
}

void wm_restore_all(void) {
    for (int i = 0; i < nwins; i++) show(&wins[i]);
}
