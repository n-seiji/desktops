#pragma once
// What wm.c, wm_focus.c and wm_list.c share. Other modules use wm.h.
#include "common.h"

typedef struct {
    AXUIElementRef el;
    pid_t pid;
    uint32_t wid;
    int ws;
    CGPoint saved;
    bool hidden;
    double shown_at; // when wm_show last unparked it
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

extern win_t *wins; // in tracking order: cycling relies on it
extern int nwins;
extern int cur_ws;  // the current desktop

// wm.c: tracked apps and windows
app_t *wm_find_app(pid_t pid);
win_t *wm_find_win(uint32_t wid);
win_t *wm_focused(void); // the window macOS reports as focused, tracked now if it was not

// wm_focus.c: parking, focus and the border
void wm_hide(win_t *w);
void wm_refresh_ui(void);

// wm_list.c: window lists and the menu bar
void wm_refresh_menubar(win_t *f);
void wm_mark_fresh(uint32_t wid, double now); // the menu bar prefers wid to macOS's lagging focus for a moment
