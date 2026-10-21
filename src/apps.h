#pragma once
// What NSWorkspace knows about running apps; the only Objective-C the window manager core talks to.
#include <ApplicationServices/ApplicationServices.h>

void apps_init(void); // starts the launch / quit / activate events and adds the running apps to wm
pid_t apps_frontmost(void);
CGImageRef apps_icon(pid_t pid, int px); // +1, a px x px copy of the app's icon, or NULL
CFStringRef apps_name(pid_t pid);        // +1 localized name, or NULL
bool apps_alive(pid_t pid);              // running and not terminating
