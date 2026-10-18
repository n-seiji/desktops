#pragma once
#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>
#include <stdint.h>

#define JD_VERSION "0.1.0"
#define JD_MAX_WS 10 // most desktops `keys` can bind
#define JD_MENUBAR_SLOTS 10 // menu bar glyph slots: with more windows the last slot is a "+k" count
#define JD_MENUBAR_WINS 64  // windows the menu bar looks at

void jd_quit(void); // put every parked window back and exit(0): the one clean way out

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
typedef enum { MB_OFF, MB_STRIP, MB_ICON } menubar_mode_t;
menubar_mode_t config_menubar(void); // strip: window glyphs; icon: only the app glyph; off: no item
menubar_mode_t config_parse_menubar(const char *v); // "on"/"strip", "icon"; anything else is off
void config_set_app_mode(bool app); // before config_load: the app defaults to strip and never has no item
bool config_parse_on(const char *v);
int config_workspaces(void); // number of desktops: how many keys are bound
int config_key(int i);       // virtual keycode of desktop i + 1
char config_key_char(int i); // that key as an uppercase character
int config_parse_keys(const char *list, int *codes);

// config_write.c
bool config_path(char *buf, size_t n);  // ~/.config/just-desktops/config
const char *config_sample(void);        // the commented template written for a new file
char *config_set(const char *text, const char *key, const char *value); // text with key set; caller frees
int config_write_key(const char *path, const char *key, const char *value); // 0, or -1 with errno
int config_ensure_file(const char *path); // creates the file (and directories) from the sample if missing

// main.m and app.m: the .app bundle
bool jd_app_mode(void);   // running as dev.n-seiji.just-desktops.app
void jd_restart(void);    // put every parked window back and exec this executable again
void app_wait_accessibility(void); // first-run panel; returns once Accessibility is granted
void app_set(const char *key, const char *value); // write one config key, then jd_restart
void app_open_config(void);
int app_login_state(void); // 0 off, 1 on, 2 needs approval in System Settings
void app_toggle_login(void);
void app_open_about(void);

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
void menubar_init(menubar_mode_t mode);
bool menubar_ready(void); // the glyph strip is shown (menubar_init ran in strip mode)
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
