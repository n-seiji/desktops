#include "jd.h"
#include <dispatch/dispatch.h>

// Fixed hotkeys:
//   ⌥1-9,0   switch workspace (up to `workspaces` in config)
//   ⌥⇧1-9,0  move focused window to workspace
//   ⌥Tab / ⌥⇧Tab  focus next / previous window in the workspace

static const int64_t digit_keys[JD_MAX_WS] = {0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1A, 0x1C, 0x19, 0x1D};

#define KEY_TAB 0x30

static CFMachPortRef tap;

static CGEventRef on_key(CGEventTapProxy proxy, CGEventType type, CGEventRef ev, void *ctx) {
    (void)proxy; (void)ctx;
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        CGEventTapEnable(tap, true);
        return ev;
    }
    if (type != kCGEventKeyDown) return ev;

    CGEventFlags mods = CGEventGetFlags(ev) & (kCGEventFlagMaskAlternate | kCGEventFlagMaskShift |
                                              kCGEventFlagMaskCommand | kCGEventFlagMaskControl);
    bool alt = mods == kCGEventFlagMaskAlternate;
    bool alt_shift = mods == (kCGEventFlagMaskAlternate | kCGEventFlagMaskShift);
    if (!alt && !alt_shift) return ev;

    int64_t key = CGEventGetIntegerValueField(ev, kCGKeyboardEventKeycode);
    bool repeat = CGEventGetIntegerValueField(ev, kCGKeyboardEventAutorepeat) != 0;

    if (key == KEY_TAB) {
        int dir = alt ? 1 : -1;
        if (!repeat) dispatch_async(dispatch_get_main_queue(), ^{ wm_cycle(dir); });
        return NULL;
    }
    for (int i = 0; i < config_workspaces(); i++) {
        if (key != digit_keys[i]) continue;
        int ws = i + 1;
        if (!repeat) {
            if (alt) dispatch_async(dispatch_get_main_queue(), ^{ wm_switch(ws); });
            else dispatch_async(dispatch_get_main_queue(), ^{ wm_move_focused(ws); });
        }
        return NULL;
    }
    return ev;
}

bool hotkey_init(void) {
    tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                           CGEventMaskBit(kCGEventKeyDown), on_key, NULL);
    if (!tap) return false;
    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(NULL, tap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopDefaultMode);
    CFRelease(src);
    CGEventTapEnable(tap, true);
    return true;
}
