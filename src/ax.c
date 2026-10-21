#include "ax.h"

bool ax_value(AXUIElementRef el, CFStringRef attr, AXValueType type, void *out) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(el, attr, &v) != kAXErrorSuccess || !v) return false;
    bool ok = CFGetTypeID(v) == AXValueGetTypeID() && AXValueGetValue(v, type, out);
    CFRelease(v);
    return ok;
}

bool ax_bool(AXUIElementRef el, CFStringRef attr) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(el, attr, &v) != kAXErrorSuccess || !v) return false;
    bool r = CFGetTypeID(v) == CFBooleanGetTypeID() && CFBooleanGetValue(v);
    CFRelease(v);
    return r;
}

bool ax_is(AXUIElementRef el, CFStringRef attr, CFStringRef want) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(el, attr, &v) != kAXErrorSuccess || !v) return false;
    bool r = CFGetTypeID(v) == CFStringGetTypeID() && CFEqual(v, want);
    CFRelease(v);
    return r;
}

CFStringRef ax_title(AXUIElementRef el) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(el, kAXTitleAttribute, &v) != kAXErrorSuccess || !v) return NULL;
    if (CFGetTypeID(v) == CFStringGetTypeID()) return v;
    CFRelease(v);
    return NULL;
}

AXError ax_set_position(AXUIElementRef el, CGPoint p) {
    AXValueRef v = AXValueCreate(kAXValueTypeCGPoint, &p);
    AXError err = AXUIElementSetAttributeValue(el, kAXPositionAttribute, v);
    CFRelease(v);
    return err;
}
