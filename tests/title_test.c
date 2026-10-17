#include "../src/jd.h"
#include <stdio.h>
#include <string.h>

static int failed;

static void check(const char *title, const char *app, const char *want) {
    CFStringRef t = CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8);
    CFStringRef a = CFStringCreateWithCString(NULL, app, kCFStringEncodingUTF8);
    CFStringRef r = title_clean(t, a);
    char got[256] = "";
    if (r) CFStringGetCString(r, got, sizeof got, kCFStringEncodingUTF8);
    bool ok = want ? r && !strcmp(got, want) : !r;
    if (!ok) {
        printf("FAIL title_clean(\"%s\", \"%s\") = %s%s%s; want %s\n", title, app, r ? "\"" : "", r ? got : "NULL", r ? "\"" : "", want ? want : "NULL");
        failed++;
    }
    if (r) CFRelease(r);
    CFRelease(t);
    CFRelease(a);
}

int main(void) {
    check("just-desktops ⌥Tab スイッチャー案 - Google Chrome - Seiji", "Google Chrome", "just-desktops ⌥Tab スイッチャー案");
    check("product-sre（チャンネル） - miive - Slack", "Slack", "product-sre（チャンネル） - miive");
    check("Claude", "Claude", NULL);
    check("Release Notes: 1.142.0 — sijis", "Code", "Release Notes: 1.142.0 — sijis");
    check("Seijis-MacBook-Air.local: ------", "Ghostty", "Seijis-MacBook-Air.local: ------");
    check("Inbox – Mail – Gmail", "gmail", "Inbox – Mail");
    check("Docs — Notes — Notes", "Notes", "Docs");
    check("Slack - Slack", "Slack", NULL);
    check("  padded  - Slack", "Slack", "padded");
    check("", "Slack", NULL);
    check("  ", "Slack", NULL);
    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
