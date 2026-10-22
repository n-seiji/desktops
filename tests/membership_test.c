#include "../src/membership.h"
#include "../src/config.h"
#include <stdio.h>
#include <string.h>

static int failed;

static void check(const char *what, long got, long want) {
    if (got == want) return;
    printf("FAIL %s = %ld; want %ld\n", what, got, want);
    failed++;
}

static void win(const char *what, const memb_win_t *got, pid_t pid, uint32_t wid, int ws) {
    if (got->pid == pid && got->wid == wid && got->ws == ws) return;
    printf("FAIL %s = {%d %u %d}; want {%d %u %d}\n", what, got->pid, got->wid, got->ws, pid, wid, ws);
    failed++;
}

int main(void) {
    memb_win_t out[8];
    int cur = 0;

    {
        char buf[256];
        memb_win_t in[] = {{100, 7, 3}, {200, 9, 1}};
        memb_format(buf, sizeof buf, 1000, 2, in, 2);
        check("round trip count", memb_parse(buf, 1010, 6, &cur, out, 8), 2);
        check("round trip cur_ws", cur, 2);
        win("round trip first", &out[0], 100, 7, 3);
        win("round trip second", &out[1], 200, 9, 1);
    }
    {
        char buf[30];
        memb_win_t in[] = {{100, 7, 3}, {200, 9, 1}, {300, 11, 2}};
        size_t len = memb_format(buf, sizeof buf, 1000, 2, in, 3);
        check("format cuts at a line end", len > 0 && buf[len - 1] == '\n' && buf[len] == 0, 1);
        int n = memb_parse(buf, 1000, 6, &cur, out, 8);
        check("a cut file length", (long)len, 26);
        check("a cut file keeps the whole lines", n, 2);
    }
    {
        char buf[8] = "x";
        memb_win_t in[] = {{100, 7, 3}};
        check("cap smaller than the header", (long)memb_format(buf, 5, 1000, 2, in, 1), 0);
        check("cap smaller than the header, terminated", buf[0], 0);
        check("nothing written is not a file", memb_parse(buf, 1000, 6, &cur, out, 8), -1);
        check("cap of zero", (long)memb_format(buf, 0, 1000, 2, in, 1), 0);
    }
    check("empty text", memb_parse("", 1000, 6, &cur, out, 8), -1);
    check("null text", memb_parse(NULL, 1000, 6, &cur, out, 8), -1);
    check("header only", memb_parse("v1 1000 3\n", 1000, 6, &cur, out, 8), 0);
    check("header only cur_ws", cur, 3);
    check("header without newline", memb_parse("v1 1000 3", 1000, 6, &cur, out, 8), -1);
    check("wrong version", memb_parse("v2 1000 3\n1 2 3\n", 1000, 6, &cur, out, 8), -1);
    check("broken header", memb_parse("v1 x 3\n1 2 3\n", 1000, 6, &cur, out, 8), -1);
    check("header with extra field", memb_parse("v1 1000 3 4\n1 2 3\n", 1000, 6, &cur, out, 8), -1);
    check("desktop 0 in the header", memb_parse("v1 1000 0\n", 1000, 6, &cur, out, 8), -1);

    check("exactly at the age limit", memb_parse("v1 1000 3\n", 1000 + MEMB_MAX_AGE_S, 6, &cur, out, 8), 0);
    check("stale file", memb_parse("v1 1000 3\n1 2 3\n", 1000 + MEMB_MAX_AGE_S + 1, 6, &cur, out, 8), -1);
    check("a little clock skew is tolerated", memb_parse("v1 1000 3\n1 2 3\n", 1000 - MEMB_SKEW_S, 6, &cur, out, 8), 1);
    check("file from the future", memb_parse("v1 1000 3\n1 2 3\n", 1000 - MEMB_SKEW_S - 1, 6, &cur, out, 8), -1);
    check("largest timestamp", memb_parse("v1 9223372036854775807 3\n1 2 3\n", 1000, 6, &cur, out, 8), -1);
    check("smallest timestamp", memb_parse("v1 -9223372036854775808 3\n1 2 3\n", 1000, 6, &cur, out, 8), -1);
    check("timestamp out of range", memb_parse("v1 99999999999999999999 3\n1 2 3\n", 1000, 6, &cur, out, 8), -1);

    cur = 99;
    check("stale file leaves cur_ws alone", memb_parse("v1 1 3\n", 1000, 6, &cur, out, 8), -1);
    check("stale cur_ws", cur, 99);

    {
        const char *t = "v1 1000 2\n10 1 1\ngarbage\n10 2\n10 3 2 9\n-5 4 1\n10 0 1\n10 5 0\n10 6 99999999999999999999\n\n11 7 2\n";
        check("broken lines are skipped", memb_parse(t, 1000, 6, &cur, out, 8), 2);
        win("line before the broken ones", &out[0], 10, 1, 1);
        win("line after the broken ones", &out[1], 11, 7, 2);
    }
    {
        check("line without a final newline", memb_parse("v1 1000 2\n10 1 1\n11 2 3", 1000, 6, &cur, out, 8), 2);
        win("last line", &out[1], 11, 2, 3);
    }
    {
        check("desktops above the count are clamped", memb_parse("v1 1000 9\n10 1 7\n10 2 3\n", 1000, 4, &cur, out, 8), 2);
        check("clamped cur_ws", cur, 4);
        win("clamped window", &out[0], 10, 1, 4);
        win("unclamped window", &out[1], 10, 2, 3);
    }
    check("pid above INT32_MAX is skipped", memb_parse("v1 1000 1\n2147483648 1 1\n2147483647 2 1\n", 1000, 4, &cur, out, 8), 1);
    win("pid at INT32_MAX", &out[0], 2147483647, 2, 1);
    check("more lines than room", memb_parse("v1 1000 1\n1 1 1\n2 2 1\n3 3 1\n", 1000, 4, &cur, out, 2), 2);

    check("a pin wins over the restored desktop", memb_pick(2, 3, 1), 2);
    check("every desktop wins over the restored desktop", memb_pick(CONFIG_EVERY, 3, 1), WS_EVERY);
    check("restored desktop without a pin", memb_pick(0, 3, 1), 3);
    check("nothing restored", memb_pick(0, 0, 4), 4);

    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
