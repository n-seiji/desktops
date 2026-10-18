#pragma once
// Row text of a window in the switchers.
#include <CoreFoundation/CoreFoundation.h>

// +1 title without its " - <app>" tail, or NULL when only the app name is left.
CFStringRef title_clean(CFStringRef title, CFStringRef app);
// One app's n windows: +1 row text per window in out. The app name when n == 1, else the
// cleaned title (NULL in titles means none), "<app> <i>" when missing, " <i>" added to repeats.
void title_labels(CFStringRef app, const CFStringRef *titles, int n, CFStringRef *out);
