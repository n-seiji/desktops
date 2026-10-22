#include "membership.h"
#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Exactly n whitespace-separated integers and nothing else.
static bool fields(const char *s, int n, long *v) {
    for (int i = 0; i < n; i++) {
        char *end;
        errno = 0;
        v[i] = strtol(s, &end, 10);
        if (end == s || errno) return false;
        s = end;
    }
    while (*s == ' ' || *s == '\r') s++;
    return *s == 0;
}

size_t memb_format(char *buf, size_t cap, long now, int cur, const memb_win_t *w, int n) {
    size_t len = 0;
    char line[64];
    snprintf(line, sizeof line, "v1 %ld %d\n", now, cur);
    for (int i = -1; i < n; i++) {
        if (i >= 0) snprintf(line, sizeof line, "%d %u %d\n", (int)w[i].pid, w[i].wid, w[i].ws);
        size_t l = strlen(line);
        if (len + l >= cap) break;
        memcpy(buf + len, line, l);
        len += l;
    }
    if (cap) buf[len] = 0;
    return len;
}

int memb_parse(const char *text, long now, int nws, int *cur_out, memb_win_t *out, int cap) {
    if (!text || strncmp(text, "v1 ", 3)) return -1;
    const char *nl = strchr(text, '\n');
    char line[64];
    long v[3];
    if (!nl || (size_t)(nl - text) - 3 >= sizeof line) return -1;
    memcpy(line, text + 3, nl - text - 3);
    line[nl - text - 3] = 0;
    if (!fields(line, 2, v) || v[0] < now - MEMB_MAX_AGE_S || v[0] > now + MEMB_SKEW_S || v[1] < 1) return -1;
    int hdr = v[1] > nws ? nws : (int)v[1];

    int n = 0;
    for (const char *p = nl + 1; *p && n < cap;) {
        nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len < sizeof line) {
            memcpy(line, p, len);
            line[len] = 0;
            if (fields(line, 3, v) && v[0] > 0 && v[0] <= INT32_MAX && v[1] > 0 && v[1] <= UINT32_MAX && v[2] >= 1)
                out[n++] = (memb_win_t){(pid_t)v[0], (uint32_t)v[1], v[2] > nws ? nws : (int)v[2]};
        }
        p += len + (nl ? 1 : 0);
    }
    *cur_out = hdr;
    return n;
}

int memb_pick(int assign, int restored, int cur) {
    if (assign == CONFIG_EVERY) return WS_EVERY;
    if (assign) return assign;
    return restored ? restored : cur;
}
