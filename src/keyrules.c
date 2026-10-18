#include "keyrules.h"

mods_t keyrules_mods(CGEventFlags flags, CGEventFlags base) {
    CGEventFlags mods = flags & (kCGEventFlagMaskAlternate | kCGEventFlagMaskShift |
                                 kCGEventFlagMaskCommand | kCGEventFlagMaskControl);
    if (mods == base) return MODS_BASE;
    if (mods == (base | kCGEventFlagMaskShift)) return MODS_BASE_SHIFT;
    return MODS_OTHER;
}

bool keyrules_step_allowed(bool repeat, bool step_pending, uint64_t now_ns, uint64_t last_step_ns) {
    return !(repeat && (step_pending || now_ns - last_step_ns < KEYRULES_REPEAT_MS * 1000000ull));
}
