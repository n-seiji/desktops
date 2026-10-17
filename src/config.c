#include "jd.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ~/.config/just-desktops/config
//   border = on|off
//   hud = on|off
//   workspaces = <1-10>   (default 9; 10 uses ⌥0)
//   <bundle id> = <workspace 1-9>

typedef struct { char id[128]; int ws; } assign_t;

static assign_t *assigns;
static int nassigns;
static bool border = true;
static bool hud = true;
static int workspaces = 9;

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
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
            border = !strcmp(v, "on") || !strcmp(v, "true") || !strcmp(v, "yes");
            continue;
        }
        if (!strcmp(k, "hud")) {
            hud = !strcmp(v, "on") || !strcmp(v, "true") || !strcmp(v, "yes");
            continue;
        }
        if (!strcmp(k, "workspaces")) {
            int n = atoi(v);
            if (n >= 1 && n <= JD_MAX_WS) workspaces = n;
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
        if (!strcmp(assigns[i].id, bundle_id)) return assigns[i].ws <= workspaces ? assigns[i].ws : 0;
    return 0;
}

bool config_border(void) { return border; }

bool config_hud(void) { return hud; }

int config_workspaces(void) { return workspaces; }
