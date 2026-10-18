#include "../src/jd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failed;

static void check_set(const char *name, const char *text, const char *key, const char *value, const char *want) {
    char *got = config_set(text, key, value);
    if (!got || strcmp(got, want)) {
        printf("FAIL %s: config_set(\"%s\", %s, %s) = \"%s\"; want \"%s\"\n", name, text, key, value, got ? got : "(null)", want);
        failed++;
    }
    free(got);
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char *buf = calloc(1, 8192);
    size_t n = fread(buf, 1, 8191, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

static void check_file(const char *name, const char *path, const char *want) {
    char *got = read_all(path);
    if (!got || strcmp(got, want)) {
        printf("FAIL %s: file = \"%s\"; want \"%s\"\n", name, got ? got : "(missing)", want);
        failed++;
    }
    free(got);
}

int main(void) {
    check_set("replace", "border = on\nswitcher = list\n", "switcher", "square", "border = on\nswitcher = square\n");
    check_set("replace without spaces", "switcher=list\n", "switcher", "off", "switcher = off\n");
    check_set("append", "border = on\n", "switcher", "square", "border = on\nswitcher = square\n");
    check_set("append to empty", "", "menubar", "icon", "menubar = icon\n");
    check_set("append without trailing newline", "border = on", "menubar", "icon", "border = on\nmenubar = icon\n");
    check_set("replace last line without trailing newline", "a = 1\nborder = on", "border", "off", "a = 1\nborder = off");
    check_set("comments kept", "# switcher = list\nswitcher = list # x\n# end\n", "switcher", "off", "# switcher = list\nswitcher = off\n# end\n");
    check_set("comment alone is not a setting", "# border = on\n", "border", "off", "# border = on\nborder = off\n");
    check_set("other keys and pins kept", "com.apple.Safari = 1\nborder = on\nkeys = 1 2\n", "border", "off", "com.apple.Safari = 1\nborder = off\nkeys = 1 2\n");
    check_set("key prefix is not the key", "border_x = on\n", "border", "off", "border_x = on\nborder = off\n");
    check_set("last duplicate is replaced", "border = on\nborder = off\n", "border", "on", "border = on\nborder = on\n");
    check_set("indented line", "  border = on\n", "border", "off", "border = off\n");
    check_set("spaces around key", "border   =   on\n", "border", "off", "border = off\n");
    check_set("blank lines kept", "\n\nborder = on\n\n", "border", "off", "\n\nborder = off\n\n");
    check_set("CR kept", "border = on\r\nkeys = 1\r\n", "border", "off", "border = off\r\nkeys = 1\r\n");

    char dir[] = "/tmp/jd_config_write_test.XXXXXX";
    if (!mkdtemp(dir)) return 1;
    char path[512], link[512], real[512], cmd[1200];
    snprintf(path, sizeof path, "%s/.config/just-desktops/config", dir);

    if (config_write_key(path, "switcher", "square") != 0) { printf("FAIL write into a missing directory\n"); failed++; }
    check_file("created", path, "switcher = square\n");
    if (config_write_key(path, "border", "off") != 0) { printf("FAIL second write\n"); failed++; }
    check_file("appended", path, "switcher = square\nborder = off\n");
    if (config_write_key(path, "switcher", "list") != 0) { printf("FAIL replace write\n"); failed++; }
    check_file("replaced", path, "switcher = list\nborder = off\n");

    snprintf(real, sizeof real, "%s/real", dir);
    snprintf(link, sizeof link, "%s/link", dir);
    FILE *f = fopen(real, "w");
    fputs("# mine\nborder = on\n", f);
    fclose(f);
    symlink(real, link);
    if (config_write_key(link, "border", "off") != 0) { printf("FAIL write through symlink\n"); failed++; }
    char target[512] = {0};
    ssize_t n = readlink(link, target, sizeof target - 1);
    if (n < 0 || strcmp(target, real)) { printf("FAIL symlink was replaced by a file\n"); failed++; }
    check_file("symlink target", real, "# mine\nborder = off\n");

    snprintf(path, sizeof path, "%s/.config2/just-desktops/config", dir);
    if (config_ensure_file(path) != 0) { printf("FAIL config_ensure_file\n"); failed++; }
    check_file("sample", path, config_sample());
    for (const char *p = config_sample(); *p;) {
        const char *e = strchr(p, '\n');
        if (!e) e = p + strlen(p);
        if (e > p && *p != '#') { printf("FAIL sample has an active line: %.*s\n", (int)(e - p), p); failed++; }
        p = *e ? e + 1 : e;
    }
    f = fopen(path, "w");
    fputs("border = off\n", f);
    fclose(f);
    config_ensure_file(path);
    check_file("existing file kept", path, "border = off\n");

    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd)) failed++;
    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
