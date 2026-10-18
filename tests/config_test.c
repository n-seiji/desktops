#include "../src/jd.h"
#include <stdio.h>

static int failed;

static void check(const char *list, int n, const int *want) {
    int got[JD_MAX_WS + 4];
    int c = config_parse_keys(list, got);
    bool ok = c == n;
    for (int i = 0; ok && i < n; i++) ok = got[i] == want[i];
    if (!ok) {
        printf("FAIL config_parse_keys(\"%s\") returned %d; want %d\n", list, c, n);
        failed++;
    }
}

int main(void) {
    check("1 2 3 q w e", 6, (int[]){0x12, 0x13, 0x14, 0x0C, 0x0D, 0x0E});
    check("0 9", 2, (int[]){0x1D, 0x19});
    check("  a   j\tz ", 3, (int[]){0x00, 0x26, 0x06});
    check("Q W", 2, (int[]){0x0C, 0x0D});
    check("1 tab 2", 2, (int[]){0x12, 0x13});
    check("1 12 - 2", 2, (int[]){0x12, 0x13});
    check("1 1 2", 2, (int[]){0x12, 0x13});
    check("", 0, NULL);
    check("tab esc", 0, NULL);
    check("1 2 3 4 5 6 7 8 9 0 q w", 10, (int[]){0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1A, 0x1C, 0x19, 0x1D});
    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
