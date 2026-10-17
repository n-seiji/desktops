#pragma once
// Private SkyLight / HIServices APIs (same family JankyBorders and yabai use).
#include <ApplicationServices/ApplicationServices.h>

extern AXError _AXUIElementGetWindow(AXUIElementRef element, uint32_t *wid);

extern int SLSMainConnectionID(void);
extern CGError SLSNewWindow(int cid, int type, float x, float y, CFTypeRef region, uint32_t *wid);
extern CGError SLSSetWindowTags(int cid, uint32_t wid, uint64_t *tags, int tag_size);
extern CGError SLSSetWindowShape(int cid, uint32_t wid, float x_offset, float y_offset, CFTypeRef shape);
extern CGError SLSSetWindowResolution(int cid, uint32_t wid, double resolution);
extern CGError SLSSetWindowOpacity(int cid, uint32_t wid, bool is_opaque);
extern CGError SLSReleaseWindow(int cid, uint32_t wid);
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
