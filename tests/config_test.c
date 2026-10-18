#include "../src/jd.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

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

static void check_menubar(const char *v, menubar_mode_t want) {
    menubar_mode_t got = config_parse_menubar(v);
    if (got != want) {
        printf("FAIL config_parse_menubar(\"%s\") = %d; want %d\n", v, got, want);
        failed++;
    }
}

static void check_modifier(const char *v, CGEventFlags want) {
    CGEventFlags got = config_parse_modifier(v);
    if (got != want) {
        printf("FAIL config_parse_modifier(\"%s\") = %llx; want %llx\n", v, (unsigned long long)got, (unsigned long long)want);
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

static void check_no_preview(const char *id, bool want) {
    if (config_no_preview(id) != want) {
        printf("FAIL config_no_preview(\"%s\") = %d; want %d\n", id, !want, want);
        failed++;
    }
}

int main(void) {
    if (config_modifier() != kCGEventFlagMaskAlternate) {
        printf("FAIL default config_modifier() = %llx; want option\n", (unsigned long long)config_modifier());
        failed++;
    }
    check_no_preview("com.1password.1password", true);
    check_no_preview("COM.Apple.Passwords", true);
    check_no_preview("com.apple.keychainaccess", true);
    check_no_preview("org.keepassxc.keepassxc", true);
    check_no_preview("com.apple.Safari", false);
    check_no_preview("", false);
    check_no_preview(NULL, false);

    char dir[] = "/tmp/jd_config_test.XXXXXX";
    if (!mkdtemp(dir)) return 1;
    char path[256];
    snprintf(path, sizeof path, "%s/.config", dir);
    mkdir(path, 0700);
    snprintf(path, sizeof path, "%s/.config/just-desktops", dir);
    mkdir(path, 0700);
    snprintf(path, sizeof path, "%s/.config/just-desktops/config", dir);
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fputs("no_preview = com.example.vault  com.example.notes\nno_preview=Com.Example.Other\nmodifier = CMD\ncom.example.pinned = 2\n", f);
    fputs("no_preview =", f); // a line far over 512 bytes: every id on it counts, the last one too
    for (int i = 0; i < 400; i++) fprintf(f, " com.example.long%d", i);
    fprintf(f, " com.example.%0150d com.example.after\n", 7); // one id too long for the buffer, then one more
    fputs("modifier = ctrl\n", f); // read after the long line: it must not have been split
    fclose(f);
    setenv("HOME", dir, 1);
    config_load();
    unlink(path);
    snprintf(path, sizeof path, "%s/.config/just-desktops", dir);
    rmdir(path);
    snprintf(path, sizeof path, "%s/.config", dir);
    rmdir(path);
    rmdir(dir);
    if (config_modifier() != kCGEventFlagMaskControl) {
        printf("FAIL config_modifier() after a long no_preview line and \"modifier = ctrl\" = %llx; want control\n", (unsigned long long)config_modifier());
        failed++;
    }
    check_no_preview("com.example.long0", true);
    check_no_preview("com.example.long399", true);
    check_no_preview("com.example.after", true);
    check_no_preview("com.example.long400", false);
    check_no_preview("com.example.vault", true);
    check_no_preview("com.example.notes", true);
    check_no_preview("com.example.other", true);
    check_no_preview("com.example.pinned", false);
    check_no_preview("com.example", false);
    check_no_preview("com.1password.1password", true);
    if (config_assign("com.example.pinned") != 2) {
        printf("FAIL no_preview line disturbed pins\n");
        failed++;
    }

    check_key_char(0, '1');
    check_key_char(2, '3');
    check_key_char(3, 'Q');
    check_key_char(5, 'E');
    check_modifier("opt", kCGEventFlagMaskAlternate);
    check_modifier("option", kCGEventFlagMaskAlternate);
    check_modifier("alt", kCGEventFlagMaskAlternate);
    check_modifier("OPT", kCGEventFlagMaskAlternate);
    check_modifier("cmd", kCGEventFlagMaskCommand);
    check_modifier("command", kCGEventFlagMaskCommand);
    check_modifier("Command", kCGEventFlagMaskCommand);
    check_modifier("ctrl", kCGEventFlagMaskControl);
    check_modifier("control", kCGEventFlagMaskControl);
    check_modifier("CTRL", kCGEventFlagMaskControl);
    check_modifier("hyper", kCGEventFlagMaskAlternate);
    check_modifier("garbage", kCGEventFlagMaskAlternate);
    check_modifier("", kCGEventFlagMaskAlternate);
    check_menubar("on", MB_STRIP);
    check_menubar("strip", MB_STRIP);
    check_menubar("true", MB_STRIP);
    check_menubar("icon", MB_ICON);
    check_menubar("off", MB_OFF);
    check_menubar("garbage", MB_OFF);
    check_menubar("", MB_OFF);
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
    // menubar default and the app's rule that there is always an item
    static const struct { bool app; const char *file; menubar_mode_t want; } mb[] = {
        {false, NULL, MB_OFF}, {false, "menubar = on\n", MB_STRIP}, {false, "menubar = icon\n", MB_ICON},
        {false, "menubar = off\n", MB_OFF}, {true, NULL, MB_STRIP}, {true, "menubar = off\n", MB_ICON},
        {true, "menubar = icon\n", MB_ICON}, {true, "menubar = strip\n", MB_STRIP},
    };
    for (size_t i = 0; i < sizeof mb / sizeof *mb; i++) {
        char home[] = "/tmp/jd_config_mb.XXXXXX", cmd[256];
        if (!mkdtemp(home)) return 1;
        snprintf(path, sizeof path, "%s/.config/just-desktops", home);
        snprintf(cmd, sizeof cmd, "mkdir -p '%s'", path);
        if (system(cmd)) return 1;
        if (mb[i].file) {
            snprintf(path, sizeof path, "%s/.config/just-desktops/config", home);
            f = fopen(path, "w");
            fputs(mb[i].file, f);
            fclose(f);
        }
        setenv("HOME", home, 1);
        config_set_app_mode(mb[i].app);
        config_load();
        if (config_menubar() != mb[i].want) {
            printf("FAIL app=%d menubar file %s: config_menubar() = %d; want %d\n", mb[i].app, mb[i].file ? mb[i].file : "(none)", config_menubar(), mb[i].want);
            failed++;
        }
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", home);
        if (system(cmd)) failed++;
    }
    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
