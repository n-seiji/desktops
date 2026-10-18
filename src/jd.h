#pragma once
#include "common.h"
#include "config.h"

void jd_quit(void); // put every parked window back and exit(0): the one clean way out

// main.m and app.m: the .app bundle
bool jd_app_mode(void);   // running as dev.n-seiji.just-desktops.app
void jd_restart(void);    // put every parked window back and exec this executable again
void app_wait_accessibility(void); // first-run panel; returns once Accessibility is granted
void app_set(const char *key, const char *value); // write one config key, then jd_restart
void app_open_config(void);
int app_login_state(void); // 0 off, 1 on, 2 needs approval in System Settings
void app_toggle_login(void);
void app_open_about(void);

// border.c
void border_init(void);
void border_show(CGRect frame, uint32_t target_wid);
void border_hide(void);

// switcher.c
void switcher_step(int dir);
void switcher_commit(void);
void switcher_cancel(void);

// switcher_card.c: the square style
void card_step(int dir);
void card_commit(void);
void card_cancel(void);

// thumb.m: window thumbnails for the card switchers (ScreenCaptureKit, used only when one is chosen)
void thumb_request_access(void); // asks for Screen Recording access once, when it is not granted
bool thumb_ready(void);          // access is granted and this macOS can take window screenshots
void thumb_begin(void);          // a switcher opened: look up the shareable windows (async)
// Screenshots window wid, at most 1152x660 px. done runs on the main queue with the image (valid
// during the call; retain it) or NULL, unless thumb_end ran first.
void thumb_capture(uint32_t wid, void (^done)(CGImageRef img));
void thumb_end(void);            // the switcher closed: drop everything held

// menubar.m
void menubar_init(menubar_mode_t mode);
bool menubar_ready(void); // the glyph strip is shown (menubar_init ran in strip mode)
// The current desktop ws, the n pids of the apps on it in switcher order, and the focused pid.
// Redraws only when one of them changed.
void menubar_update(int ws, const pid_t *pids, const uint32_t *wids, int n, int over, uint32_t over_hash, uint32_t front);
void menubar_forget(pid_t pid); // drop an app's cached glyph

// hotkey.c
bool hotkey_init(void);

// wm.c
void wm_init(void);
void wm_add_app(pid_t pid, const char *bundle_id);
void wm_remove_app(pid_t pid);
void wm_on_focus(void);
void wm_switch(int ws);
void wm_move_focused(int ws);
int wm_current(void);
int wm_windows(int dir, sw_entry_t **out, int *sel);
int wm_overflow(sw_entry_t **out); // the windows behind the menu bar's "+k" slot, in order; caller releases icon/app/title and frees
int wm_window_ids(int dir, uint32_t **out, int *sel); // wm_windows without icons and text; caller frees *out
void wm_focus_wid(uint32_t wid);
void wm_restore_all(void);

// apps.m
void apps_init(void);
pid_t apps_frontmost(void);
CGImageRef apps_icon(pid_t pid, int px);
CFStringRef apps_name(pid_t pid);
