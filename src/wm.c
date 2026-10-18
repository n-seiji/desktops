#include "wm.h"
#include "wm_priv.h"
#include "apps.h"
#include "ax.h"
#include "config.h"
#include "menubar.h"
#include "shown.h"
#include "skylight.h"
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <string.h>

// The apps and windows the manager knows, kept in step with Accessibility notifications. Workspaces
// are emulated like AeroSpace does: windows of inactive workspaces are parked at the bottom-right
// corner of the main display, so macOS Spaces / SIP are never touched (see wm_focus.c).

win_t *wins;
int nwins;
int cur_ws = 1;

static int capwins;
static app_t *apps;
static int napps, capapps;
static AXUIElementRef sys;

static void ax_cb(AXObserverRef obs, AXUIElementRef el, CFStringRef n, void *ctx);

// ---- lookup ----

app_t *wm_find_app(pid_t pid) {
    for (int i = 0; i < napps; i++)
        if (apps[i].pid == pid) return &apps[i];
    return NULL;
}

win_t *wm_find_win(uint32_t wid) {
    if (!wid) return NULL;
    for (int i = 0; i < nwins; i++)
        if (wins[i].wid == wid) return &wins[i];
    return NULL;
}

static void remove_win_at(int i) {
    CFRelease(wins[i].el);
    memmove(&wins[i], &wins[i + 1], sizeof *wins * (--nwins - i)); // keep order: cycling relies on it
}

// ---- tracking ----

static win_t *track(app_t *a, AXUIElementRef el) {
    uint32_t wid = 0;
    if (_AXUIElementGetWindow(el, &wid) != kAXErrorSuccess || !wid) return NULL;
    win_t *w = wm_find_win(wid);
    if (w) return w;
    if (!ax_is(el, kAXRoleAttribute, kAXWindowRole)) return NULL;
    if (!ax_is(el, kAXSubroleAttribute, kAXStandardWindowSubrole) &&
        !ax_is(el, kAXSubroleAttribute, kAXDialogSubrole))
        return NULL;

    if (nwins == capwins) {
        int cap = capwins ? capwins * 2 : 32;
        win_t *n = realloc(wins, sizeof *wins * cap);
        if (!n) return NULL;
        wins = n;
        capwins = cap;
    }
    w = &wins[nwins++];
    *w = (win_t){(AXUIElementRef)CFRetain(el), a->pid, wid, a->assign ? a->assign : cur_ws, {0, 0}, false, 0, false,
                 ax_bool(el, kAXMinimizedAttribute)};

    // A window left in the corner by a previous crash: bring it back.
    CGPoint p;
    if (ax_value(el, kAXPositionAttribute, kAXValueTypeCGPoint, &p) && shown_is_parked(p, CGDisplayBounds(CGMainDisplayID()))) {
        CGRect b = CGDisplayBounds(CGMainDisplayID());
        ax_set_position(el, CGPointMake(b.origin.x + 40, b.origin.y + 60));
    }

    void *ctx = (void *)(intptr_t)a->pid;
    AXObserverAddNotification(a->obs, el, kAXUIElementDestroyedNotification, ctx);
    AXObserverAddNotification(a->obs, el, kAXWindowMiniaturizedNotification, ctx);
    AXObserverAddNotification(a->obs, el, kAXWindowDeminiaturizedNotification, ctx);

    if (w->ws != cur_ws) wm_hide(w);
    return w;
}

win_t *wm_focused(void) {
    AXUIElementRef app = NULL, win = NULL;
    win_t *w = NULL;
    // The system-wide query fails for some apps (Slack: kAXErrorCannotComplete); ask the frontmost app itself.
    if (AXUIElementCopyAttributeValue(sys, kAXFocusedApplicationAttribute, (CFTypeRef *)&app) != kAXErrorSuccess || !app) {
        app_t *a = wm_find_app(apps_frontmost());
        if (!a) return NULL;
        app = (AXUIElementRef)CFRetain(a->el);
    }
    if (AXUIElementCopyAttributeValue(app, kAXFocusedWindowAttribute, (CFTypeRef *)&win) == kAXErrorSuccess && win) {
        uint32_t wid = 0;
        if (_AXUIElementGetWindow(win, &wid) == kAXErrorSuccess) w = wm_find_win(wid);
        if (!w) {
            pid_t pid = 0;
            AXUIElementGetPid(app, &pid);
            app_t *a = wm_find_app(pid);
            if (a) w = track(a, win);
        }
        CFRelease(win);
    }
    CFRelease(app);
    return w;
}

// ---- apps ----

static void add_app(pid_t pid, int assign, bool nopreview, int attempt) {
    if (wm_find_app(pid)) return;
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
            track(wm_find_app(pid), CFArrayGetValueAtIndex(list, i));
        CFRelease(list);
    }
    if (attempt > 0) wm_on_focus();
}

void wm_add_app(pid_t pid, const char *bundle_id) {
    add_app(pid, config_assign(bundle_id), config_no_preview(bundle_id), 0);
    if (menubar_ready()) wm_refresh_menubar(wm_focused());
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
    app_t *a = wm_find_app((pid_t)(intptr_t)ctx);
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
        wm_refresh_ui();
    }
}

void wm_init(void) {
    sys = AXUIElementCreateSystemWide();
    AXUIElementSetMessagingTimeout(sys, 1.0);
}

int wm_current(void) {
    return cur_ws;
}
