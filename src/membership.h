#pragma once
// The desktop of each window and the current desktop, kept in a small text file across a restart.
#include "common.h"
#include <stddef.h>

#define MEMB_MAX_AGE_S 60 // a file older than this is from some other session
#define MEMB_SKEW_S 5     // a file this far ahead of the clock is from a clock adjustment, not another session

typedef struct {
    pid_t pid;
    uint32_t wid;
    int ws;
} memb_win_t;

// Writes "v1 <now> <cur>" and one "<pid> <wid> <ws>" line per window into buf, whole lines only.
// Returns the length written, without the terminating NUL.
size_t memb_format(char *buf, size_t cap, long now, int cur, const memb_win_t *w, int n);
// Reads that text. Returns the number of windows stored in out (at most cap), or -1 when the text is
// empty, has no valid header or is older than MEMB_MAX_AGE_S or newer than MEMB_SKEW_S ahead. Lines that do not parse are skipped.
// Desktops above nws become nws; *cur_out is set only on success.
int memb_parse(const char *text, long now, int nws, int *cur_out, memb_win_t *out, int cap);
// The desktop of a new window: its pin wins (config_assign values), then the restored one (0: none), then cur.
int memb_pick(int assign, int restored, int cur);
