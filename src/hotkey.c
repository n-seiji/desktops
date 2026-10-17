#include "jd.h"
#include <dispatch/dispatch.h>

// Fixed hotkeys:
//   ⌥1-9,0   switch workspace (up to `workspaces` in config)
//   ⌥⇧1-9,0  move focused window to workspace
//   ⌥Tab / ⌥⇧Tab  open the window switcher / move the selection; releasing ⌥ commits, Esc cancels

static const int64_t digit_keys[JD_MAX_WS] = {0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1A, 0x1C, 0x19, 0x1D};

#define KEY_TAB 0x30
#define KEY_ESC 0x35

static CFMachPortRef tap;
static bool armed; // switcher open (or about to be): ⌥ is held since a Tab

static void commit(void) {
    armed = false;
    dispatch_async(dispatch_get_main_queue(), ^{ switcher_commit(); });
}

static CGEventRef on_key(CGEventTapProxy proxy, CGEventType type, CGEventRef ev, void *ctx) {
    (void)proxy; (void)ctx;
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        CGEventTapEnable(tap, true);
        // The ⌥ release may have been missed while the tap was off.
        if (armed && !(CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState) & kCGEventFlagMaskAlternate)) commit();
        return ev;
    }
    if (type == kCGEventFlagsChanged) {
        if (armed && !(CGEventGetFlags(ev) & kCGEventFlagMaskAlternate)) commit();
        return ev;
    }
    if (type != kCGEventKeyDown) return ev;
    if (armed && CGEventGetIntegerValueField(ev, kCGKeyboardEventKeycode) == KEY_ESC) {
        armed = false;
        dispatch_async(dispatch_get_main_queue(), ^{ switcher_cancel(); });
        return NULL;
    }

    CGEventFlags mods = CGEventGetFlags(ev) & (kCGEventFlagMaskAlternate | kCGEventFlagMaskShift |
                                              kCGEventFlagMaskCommand | kCGEventFlagMaskControl);
    bool alt = mods == kCGEventFlagMaskAlternate;
    bool alt_shift = mods == (kCGEventFlagMaskAlternate | kCGEventFlagMaskShift);
    if (!alt && !alt_shift) return ev;

    int64_t key = CGEventGetIntegerValueField(ev, kCGKeyboardEventKeycode);
    bool repeat = CGEventGetIntegerValueField(ev, kCGKeyboardEventAutorepeat) != 0;

    if (key == KEY_TAB) {
        int dir = alt ? 1 : -1;
        armed = true;
        dispatch_async(dispatch_get_main_queue(), ^{ switcher_step(dir); });
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
                           CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventFlagsChanged), on_key, NULL);
    if (!tap) return false;
    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(NULL, tap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopDefaultMode);
    CFRelease(src);
    CGEventTapEnable(tap, true);
    return true;
}
