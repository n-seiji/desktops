#pragma once
// Typed reads and writes of Accessibility attributes. A missing attribute, an error or a value of
// another type reads as false / NULL.
#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>

bool ax_value(AXUIElementRef el, CFStringRef attr, AXValueType type, void *out); // a CGPoint, CGSize...
bool ax_bool(AXUIElementRef el, CFStringRef attr);
bool ax_is(AXUIElementRef el, CFStringRef attr, CFStringRef want); // the string attribute equals want
CFStringRef ax_title(AXUIElementRef el); // +1 AXTitle, or NULL
AXError ax_set_position(AXUIElementRef el, CGPoint p);
