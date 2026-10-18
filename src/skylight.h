#pragma once
// Every private API the project calls, in one place (the same family JankyBorders and yabai use).
// Window creation, drawing, ordering and notifications go through SkyLight; the AX element to window
// id lookup is HIServices, listed here with the rest because it is private too.
#include <ApplicationServices/ApplicationServices.h>

extern AXError _AXUIElementGetWindow(AXUIElementRef element, uint32_t *wid);

extern int SLSMainConnectionID(void);
extern CGError SLSNewWindow(int cid, int type, float x, float y, CFTypeRef region, uint32_t *wid);
extern CGError SLSSetWindowTags(int cid, uint32_t wid, uint64_t *tags, int tag_size);
extern CGError SLSSetWindowShape(int cid, uint32_t wid, float x_offset, float y_offset, CFTypeRef shape);
extern CGError SLSSetWindowResolution(int cid, uint32_t wid, double resolution);
extern CGError SLSSetWindowOpacity(int cid, uint32_t wid, bool is_opaque);
extern CGError SLSReleaseWindow(int cid, uint32_t wid);
extern CGError SLSSetWindowTransform(int cid, uint32_t wid, CGAffineTransform t);
extern CGError SLSSetWindowAlpha(int cid, uint32_t wid, float alpha);
extern CGError SLSSetWindowLevel(int cid, uint32_t wid, int level);
extern CGError SLSGetWindowLevel(int cid, uint32_t wid, int *level);
extern CGError SLSOrderWindow(int cid, uint32_t wid, int mode, uint32_t relative_to);
extern CGError SLSMoveWindow(int cid, uint32_t wid, CGPoint *point);
extern CGError SLSDisableUpdate(int cid);
extern CGError SLSReenableUpdate(int cid);
extern CGContextRef SLWindowContextCreate(int cid, uint32_t wid, CFDictionaryRef options);
extern CGError CGSNewRegionWithRect(CGRect *rect, CFTypeRef *region);
extern CGError SLSSetWindowBackgroundBlurRadius(int cid, uint32_t wid, int radius);
extern CGError SLSGetWindowBounds(int cid, uint32_t wid, CGRect *frame);
extern CGError SLSRegisterNotifyProc(void *handler, uint32_t event, void *context);
extern CGError SLSRequestNotificationsForWindows(int cid, uint32_t *window_list, int window_count);
extern CGError SLSGetEventPort(int cid, mach_port_t *port);
extern CGEventRef SLEventCreateNextEvent(int cid);
extern void _CFMachPortSetOptions(CFMachPortRef port, int options);
extern CFTypeRef SLSWindowQueryWindows(int cid, CFArrayRef windows, int count);
extern CFTypeRef SLSWindowQueryResultCopyWindows(CFTypeRef query);
extern bool SLSWindowIteratorAdvance(CFTypeRef iterator);
// SLSWindowIteratorGetCornerRadii (four CFNumbers, in points) is looked up at run time: older macOS does not export it.
typedef CFArrayRef (*corner_radii_fn)(CFTypeRef iterator);
