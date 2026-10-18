#include "jd.h"

// The key is only set while the system is in Dark mode, and it follows Auto too.
bool appearance_dark(void) {
    CFPropertyListRef v = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);
    if (!v) return false;
    bool dark = CFGetTypeID(v) == CFStringGetTypeID() && CFStringCompare(v, CFSTR("Dark"), 0) == kCFCompareEqualTo;
    CFRelease(v);
    return dark;
}
