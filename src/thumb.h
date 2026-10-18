#pragma once
// Window thumbnails for the square switcher (ScreenCaptureKit, used only when it is chosen).
#include <ApplicationServices/ApplicationServices.h>

void thumb_request_access(void); // asks for Screen Recording access once, when it is not granted
bool thumb_ready(void);          // access is granted and this macOS can take window screenshots
void thumb_begin(void);          // a switcher opened: look up the shareable windows (async)
// Screenshots window wid, at most 1152x660 px. done runs on the main queue with the image (valid
// during the call; retain it) or NULL, unless thumb_end ran first.
void thumb_capture(uint32_t wid, void (^done)(CGImageRef img));
void thumb_end(void);            // the switcher closed: drop everything held
