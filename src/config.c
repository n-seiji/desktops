#include "jd.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ~/.config/just-desktops/config
//   border = on|off
//   switcher = list|square|off   (⌥Tab window switcher; on = list)
//   menubar = on|off      (menu bar item: current desktop and its apps)
//   keys = 1 2 3 q w e    (digits and letters; the n-th key is Desktop n)
//   <bundle id> = <desktop number>

typedef struct { char id[128]; int ws; } assign_t;

static assign_t *assigns;
static int nassigns;
static bool border = true;
static sw_style_t switcher = SW_LIST;
static bool menubar = false;
static int keys[JD_MAX_WS] = {0x12, 0x13, 0x14, 0x0C, 0x0D, 0x0E}; // 1 2 3 q w e
static int nkeys = 6;

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

// "off" (or false/no) turns it off; "on" and anything unknown give the default list style.
sw_style_t config_parse_switcher(const char *v) {
    if (!strcmp(v, "off") || !strcmp(v, "false") || !strcmp(v, "no")) return SW_OFF;
    if (!strcmp(v, "square")) return SW_SQUARE;
    return SW_LIST;
}

void config_load(void) {
    const char *home = getenv("HOME");
    char path[1024];
    snprintf(path, sizeof path, "%s/.config/just-desktops/config", home ? home : "");
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof line, f)) {
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
        if (!strcmp(k, "menubar")) {
            menubar = config_parse_on(v);
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
        int ws = atoi(v);
        if (ws < 1 || ws > JD_MAX_WS) continue;
        assign_t *n = realloc(assigns, sizeof *assigns * (nassigns + 1));
        if (!n) break;
        assigns = n;
        snprintf(assigns[nassigns].id, sizeof assigns[nassigns].id, "%s", k);
        assigns[nassigns++].ws = ws;
    }
    fclose(f);
}

int config_assign(const char *bundle_id) {
    if (!bundle_id) return 0;
    for (int i = 0; i < nassigns; i++)
        if (!strcmp(assigns[i].id, bundle_id)) return assigns[i].ws <= nkeys ? assigns[i].ws : 0;
    return 0;
}

bool config_border(void) { return border; }

sw_style_t config_switcher_style(void) { return switcher; }

bool config_menubar(void) { return menubar; }

int config_workspaces(void) { return nkeys; }

int config_key(int i) { return keys[i]; }

char config_key_char(int i) { return toupper((unsigned char)layout[keys[i]]); }
