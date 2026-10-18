#include "config.h"
#include "config_write.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// ~/.config/just-desktops/config
//   border = on|off
//   switcher = list|square|off   (⌥Tab window switcher; on = list; off = switch at once, no panel)
//   modifier = opt|cmd|ctrl   (base modifier of every hotkey, default opt; aliases option, alt, command, control)
//   menubar = on|icon|off (menu bar item: on/strip = windows and the desktop key, icon = app glyph only; the app treats off as icon)
//   keys = 1 2 3 q w e    (digits and letters; the n-th key is Desktop n)
//   no_preview = <bundle id> [<bundle id> ...]   (apps the square switcher never screenshots; repeatable)
//   <bundle id> = <desktop number>

typedef struct { char id[128]; int ws; } assign_t;

static assign_t *assigns;
static int nassigns;
typedef struct { char id[128]; } nopreview_t;
static nopreview_t *extra; // no_preview entries from the config file, on top of the built-in list
static int nextra;
static bool border = true;
static sw_style_t switcher = SW_LIST;
static menubar_mode_t menubar = MB_OFF;
static bool app_mode;
static CGEventFlags modifier = kCGEventFlagMaskAlternate;
static int keys[JD_MAX_WS] = {0x12, 0x13, 0x14, 0x0C, 0x0D, 0x0E}; // 1 2 3 q w e
static int nkeys = 6;

// Password managers: their windows are never captured for the square switcher (ids compared case-insensitively).
static const char *const secret_apps[] = {
    "com.1password.1password", "com.agilebits.onepassword7", "com.agilebits.onepassword-osx", "com.agilebits.onepassword4",
    "com.bitwarden.desktop", "com.dashlane.dashlanephonefinal", "com.lastpass.lastpassmacdesktop",
    "org.keepassxc.keepassxc", "com.keepassium.ios", "com.markmcguill.strongbox.mac",
    "com.markmcguill.strongbox.mac.mas", "in.sinew.Enpass-Desktop", "io.enpass.Enpass", "com.nordsec.nordpass",
    "me.proton.pass.electron", "com.callpod.keepermac.lite", "com.apple.Passwords", "com.apple.keychainaccess",
    "com.hicknhacksoftware.MacPass", "com.buttercup.desktop",
};

// ANSI virtual keycode -> character; a space marks a keycode that is not bindable.
static const char layout[] = "asdfhgzxcv bqwerty123465 97 80 ou ip lj k    nm";

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

// "on", "true" and "yes" are on; anything else is off.
bool config_parse_on(const char *v) {
    return !strcmp(v, "on") || !strcmp(v, "true") || !strcmp(v, "yes");
}

// Parses space-separated single characters into virtual keycodes; unknown tokens and
// repeats are skipped. Returns the number written to codes (at most JD_MAX_WS).
int config_parse_keys(const char *list, int *codes) {
    int n = 0;
    for (const char *p = list; *p && n < JD_MAX_WS;) {
        if (isspace((unsigned char)*p)) { p++; continue; }
        const char *e = p;
        while (*e && !isspace((unsigned char)*e)) e++;
        const char *at = e - p == 1 ? strchr(layout, tolower((unsigned char)*p)) : NULL;
        int code = at ? (int)(at - layout) : -1;
        for (int i = 0; i < n && code >= 0; i++)
            if (codes[i] == code) code = -1;
        if (code >= 0) codes[n++] = code;
        p = e;
    }
    return n;
}

// "off" (or false/no) is the instant mode without a panel; "on" and anything unknown give the default list style.
sw_style_t config_parse_switcher(const char *v) {
    if (!strcmp(v, "off") || !strcmp(v, "false") || !strcmp(v, "no")) return SW_OFF;
    if (!strcmp(v, "square")) return SW_SQUARE;
    return SW_LIST;
}

// Case-insensitive. Unknown values give option.
CGEventFlags config_parse_modifier(const char *v) {
    if (!strcasecmp(v, "cmd") || !strcasecmp(v, "command")) return kCGEventFlagMaskCommand;
    if (!strcasecmp(v, "ctrl") || !strcasecmp(v, "control")) return kCGEventFlagMaskControl;
    return kCGEventFlagMaskAlternate;
}

menubar_mode_t config_parse_menubar(const char *v) {
    if (!strcmp(v, "icon")) return MB_ICON;
    return config_parse_on(v) || !strcmp(v, "strip") ? MB_STRIP : MB_OFF;
}

void config_set_app_mode(bool app) { app_mode = app; }

void config_load(void) {
    menubar = app_mode ? MB_STRIP : MB_OFF;
    char path[1024];
    config_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (!f) return;

    // Lines have no length limit: a long no_preview list must not lose its tail, because every id lost is an app that gets screenshotted.
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, f) >= 0) {
        char *s = trim(line);
        if (!*s || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);

        if (!strcmp(k, "border")) {
            border = config_parse_on(v);
            continue;
        }
        if (!strcmp(k, "switcher")) {
            switcher = config_parse_switcher(v);
            continue;
        }
        if (!strcmp(k, "modifier")) {
            modifier = config_parse_modifier(v);
            continue;
        }
        if (!strcmp(k, "menubar")) {
            menubar = config_parse_menubar(v);
            continue;
        }
        if (!strcmp(k, "keys")) {
            int parsed[JD_MAX_WS];
            int n = config_parse_keys(v, parsed);
            if (n) {
                memcpy(keys, parsed, sizeof parsed);
                nkeys = n;
            }
            continue;
        }
        if (!strcmp(k, "no_preview")) {
            for (char *tok = strtok(v, " \t"); tok; tok = strtok(NULL, " \t")) {
                if (strlen(tok) >= sizeof extra->id) {
                    fprintf(stderr, "just-desktops: no_preview id longer than %zu bytes ignored: %.20s...\n", sizeof extra->id - 1, tok);
                    continue;
                }
                nopreview_t *n = realloc(extra, sizeof *extra * (nextra + 1));
                if (!n) break;
                extra = n;
                snprintf(extra[nextra++].id, sizeof extra->id, "%s", tok);
            }
            continue;
        }
        int ws = atoi(v);
        if (ws < 1 || ws > JD_MAX_WS) continue;
        if (strlen(k) >= sizeof assigns->id) {
            fprintf(stderr, "just-desktops: bundle id longer than %zu bytes ignored: %.20s...\n", sizeof assigns->id - 1, k);
            continue;
        }
        assign_t *n = realloc(assigns, sizeof *assigns * (nassigns + 1));
        if (!n) break;
        assigns = n;
        snprintf(assigns[nassigns].id, sizeof assigns[nassigns].id, "%s", k);
        assigns[nassigns++].ws = ws;
    }
    free(line);
    fclose(f);
    if (app_mode && menubar == MB_OFF) menubar = MB_ICON;
}

int config_assign(const char *bundle_id) {
    if (!bundle_id) return 0;
    for (int i = 0; i < nassigns; i++)
        if (!strcmp(assigns[i].id, bundle_id)) return assigns[i].ws <= nkeys ? assigns[i].ws : 0;
    return 0;
}

bool config_no_preview(const char *bundle_id) {
    if (!bundle_id) return false;
    for (size_t i = 0; i < sizeof secret_apps / sizeof *secret_apps; i++)
        if (!strcasecmp(secret_apps[i], bundle_id)) return true;
    for (int i = 0; i < nextra; i++)
        if (!strcasecmp(extra[i].id, bundle_id)) return true;
    return false;
}

bool config_border(void) { return border; }

sw_style_t config_switcher_style(void) { return switcher; }

menubar_mode_t config_menubar(void) { return menubar; }

CGEventFlags config_modifier(void) { return modifier; }

int config_workspaces(void) { return nkeys; }

int config_key(int i) { return keys[i]; }

char config_key_char(int i) { return toupper((unsigned char)layout[keys[i]]); }
