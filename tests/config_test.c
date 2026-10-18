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

static void check_on(const char *v, bool want) {
    if (config_parse_on(v) != want) {
        printf("FAIL config_parse_on(\"%s\") = %d; want %d\n", v, !want, want);
        failed++;
    }
}

static void check_switcher(const char *v, sw_style_t want) {
    sw_style_t got = config_parse_switcher(v);
    if (got != want) {
        printf("FAIL config_parse_switcher(\"%s\") = %d; want %d\n", v, got, want);
        failed++;
    }
}

static void check_key_char(int i, char want) {
    char got = config_key_char(i);
    if (got != want) {
        printf("FAIL config_key_char(%d) = '%c'; want '%c'\n", i, got, want);
        failed++;
    }
}

int main(void) {
    check_key_char(0, '1');
    check_key_char(2, '3');
    check_key_char(3, 'Q');
    check_key_char(5, 'E');
    check_on("on", true);
    check_on("true", true);
    check_on("yes", true);
    check_on("off", false);
    check_on("", false);
    check_on("ON", false);
    check_switcher("list", SW_LIST);
    check_switcher("square", SW_SQUARE);
    check_switcher("peek", SW_LIST);
    check_switcher("off", SW_OFF);
    check_switcher("on", SW_LIST);
    check_switcher("garbage", SW_LIST);
    check_switcher("", SW_LIST);
    if (config_switcher_style() != SW_LIST) {
        printf("FAIL default config_switcher_style() = %d; want %d\n", config_switcher_style(), SW_LIST);
        failed++;
    }
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
