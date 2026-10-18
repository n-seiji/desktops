#pragma once
// Desktops emulated by parking the windows of inactive desktops at the bottom-right corner.
#include "common.h"

void wm_init(void);
void wm_add_app(pid_t pid, const char *bundle_id);
void wm_remove_app(pid_t pid);
void wm_on_focus(void); // the focused window or app may have changed
void wm_switch(int ws);
void wm_move_focused(int ws);
int wm_current(void);
void wm_focus_wid(uint32_t wid);
void wm_restore_all(void); // every parked window back on screen

// The switchable windows of this desktop for the switchers, in tracking order. *out is malloc'd and
// each entry has +1 icon, app and title (the title is the row text); the caller releases them with
// sw_free_entries. *sel is the window after the focused one (before it for dir < 0).
int wm_windows(int dir, sw_entry_t **out, int *sel);
int wm_window_ids(int dir, uint32_t **out, int *sel); // wm_windows without icons and text; caller frees *out
// The windows behind the menu bar's "+k" slot, in order, as wm_windows has them.
int wm_overflow(sw_entry_t **out);
