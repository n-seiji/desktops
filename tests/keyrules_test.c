#include "../src/keyrules.h"
#include <stdio.h>

static int failed;

#define ALT kCGEventFlagMaskAlternate
#define SHIFT kCGEventFlagMaskShift
#define CMD kCGEventFlagMaskCommand
#define CTRL kCGEventFlagMaskControl

static void mods(const char *what, CGEventFlags flags, CGEventFlags base, mods_t want) {
    mods_t got = keyrules_mods(flags, base);
    if (got == want) return;
    printf("FAIL keyrules_mods(%s) = %d; want %d\n", what, got, want);
    failed++;
}

static void allowed(const char *what, bool repeat, bool pending, uint64_t now_ms, uint64_t last_ms, bool want) {
    bool got = keyrules_step_allowed(repeat, pending, now_ms * 1000000ull, last_ms * 1000000ull);
    if (got == want) return;
    printf("FAIL keyrules_step_allowed(%s) = %d; want %d\n", what, got, want);
    failed++;
}

int main(void) {
    mods("alt", ALT, ALT, MODS_BASE);
    mods("alt+shift", ALT | SHIFT, ALT, MODS_BASE_SHIFT);
    mods("nothing", 0, ALT, MODS_OTHER);
    mods("shift alone", SHIFT, ALT, MODS_OTHER);
    mods("alt+cmd", ALT | CMD, ALT, MODS_OTHER);
    mods("alt+shift+ctrl", ALT | SHIFT | CTRL, ALT, MODS_OTHER);
    mods("cmd instead of alt", CMD, ALT, MODS_OTHER);
    mods("alt with caps lock and fn", ALT | kCGEventFlagMaskAlphaShift | kCGEventFlagMaskSecondaryFn, ALT, MODS_BASE);
    mods("cmd base: cmd", CMD, CMD, MODS_BASE);
    mods("cmd base: cmd+shift", CMD | SHIFT, CMD, MODS_BASE_SHIFT);
    mods("cmd base: alt", ALT, CMD, MODS_OTHER);
    mods("ctrl base: ctrl+alt", CTRL | ALT, CTRL, MODS_OTHER);

    allowed("first press", false, false, 1000, 0, true);
    allowed("first press while a step is pending", false, true, 1000, 999, true);
    allowed("repeat with a step pending", true, true, 1000, 0, false);
    allowed("repeat right after a step", true, false, 1030, 1000, false);
    allowed("repeat 119ms after a step", true, false, 1119, 1000, false);
    allowed("repeat 120ms after a step", true, false, 1120, 1000, true);
    allowed("repeat long after a step", true, false, 5000, 1000, true);

    puts(failed ? "FAILED" : "ok");
    return failed != 0;
}
