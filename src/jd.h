#pragma once
#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>
#include <stdint.h>

#define JD_VERSION "0.1.0"
#define JD_MAX_WS 10 // most desktops `keys` can bind
#define JD_MENUBAR_SLOTS 10 // menu bar glyph slots: with more windows the last slot is a "+k" count
#define JD_MENUBAR_WINS 64  // windows the menu bar looks at

// config.c
void config_load(void);
int config_assign(const char *bundle_id);
bool config_no_preview(const char *bundle_id); // the square switcher never screenshots this app's windows
bool config_border(void);
typedef enum { SW_OFF, SW_LIST, SW_SQUARE } sw_style_t;
sw_style_t config_switcher_style(void); // off: ⌥Tab cycles instantly with no panel
sw_style_t config_parse_switcher(const char *v); // "list"/"on", "square", "off" (instant, no panel); anything else is list
CGEventFlags config_modifier(void); // base modifier flag of every hotkey: option (default), command or control
CGEventFlags config_parse_modifier(const char *v); // opt|option|alt, cmd|command, ctrl|control (any case); anything else is option
bool config_menubar(void);  // the menu bar item is shown
bool config_parse_on(const char *v);
int config_workspaces(void); // number of desktops: how many keys are bound
int config_key(int i);       // virtual keycode of desktop i + 1
char config_key_char(int i); // that key as an uppercase character
int config_parse_keys(const char *list, int *codes);

// appearance.c
bool appearance_dark(void); // the system is in Dark mode

// border.c
void border_init(void);
void border_show(CGRect frame, uint32_t target_wid);
void border_hide(void);

// switcher.c
typedef struct {
    uint32_t wid;
    pid_t pid;
    CGImageRef icon;
    CFStringRef app, title;
    bool nopreview; // never capture this window (password managers)
} sw_entry_t;
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
void menubar_init(void);
bool menubar_ready(void); // menubar_init has run: the item exists
// The current desktop ws, the n pids of the apps on it in switcher order, and the focused pid.
// Redraws only when one of them changed.
void menubar_update(int ws, const pid_t *pids, const uint32_t *wids, int n, int over, uint32_t over_hash, uint32_t front);
void menubar_forget(pid_t pid); // drop an app's cached glyph

// title.c
// +1 title without its " - <app>" tail, or NULL when only the app name is left.
CFStringRef title_clean(CFStringRef title, CFStringRef app);
// One app's n windows: +1 row text per window in out. The app name when n == 1, else the
// cleaned title (NULL in titles means none), "<app> <i>" when missing, " <i>" added to repeats.
void title_labels(CFStringRef app, const CFStringRef *titles, int n, CFStringRef *out);

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
