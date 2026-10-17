#import <AppKit/AppKit.h>
#include "jd.h"

// The only Objective-C in the project: NSWorkspace app launch / quit / activate events.

static void add(NSRunningApplication *app) {
    if (app.activationPolicy != NSApplicationActivationPolicyRegular) return;
    wm_add_app(app.processIdentifier, app.bundleIdentifier.UTF8String);
}

@interface JDAppObserver : NSObject
@end

@implementation JDAppObserver
- (void)launched:(NSNotification *)n {
    add(n.userInfo[NSWorkspaceApplicationKey]);
}
- (void)terminated:(NSNotification *)n {
    NSRunningApplication *app = n.userInfo[NSWorkspaceApplicationKey];
    wm_remove_app(app.processIdentifier);
    wm_on_focus();
}
- (void)activated:(NSNotification *)n {
    (void)n;
    wm_on_focus();
}
@end

static JDAppObserver *observer;

void apps_init(void) {
    observer = [JDAppObserver new];
    NSNotificationCenter *nc = NSWorkspace.sharedWorkspace.notificationCenter;
    [nc addObserver:observer selector:@selector(launched:) name:NSWorkspaceDidLaunchApplicationNotification object:nil];
    [nc addObserver:observer selector:@selector(terminated:) name:NSWorkspaceDidTerminateApplicationNotification object:nil];
    [nc addObserver:observer selector:@selector(activated:) name:NSWorkspaceDidActivateApplicationNotification object:nil];
    for (NSRunningApplication *app in NSWorkspace.sharedWorkspace.runningApplications) add(app);
}

pid_t apps_frontmost(void) {
    return NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier;
}
