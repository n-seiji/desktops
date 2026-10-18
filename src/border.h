#pragma once
// The focus ring (no AppKit).
#include <ApplicationServices/ApplicationServices.h>

void border_init(void);
void border_show(CGRect frame, uint32_t target_wid); // ring around frame, ordered above window target_wid
void border_hide(void);
