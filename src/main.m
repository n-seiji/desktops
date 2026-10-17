#import <AppKit/AppKit.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>
#include "jd.h"

static dispatch_source_t signal_sources[3];

int main(int argc, char **argv) {
    if (argc > 1) {
        if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) {
            puts(JD_VERSION);
            return 0;
        }
        fprintf(stderr, "usage: just-desktops [--version]\n");
        return 1;
    }

    int lock = open("/tmp/just-desktops.lock", O_CREAT | O_RDWR, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "just-desktops: already running\n");
        return 1;
    }

    @autoreleasepool {
        NSDictionary *opts = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt: @YES};
        if (!AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)opts)) {
            fprintf(stderr, "just-desktops: waiting for Accessibility permission\n");
            while (!AXIsProcessTrusted()) sleep(2);
        }
        config_load();
        if (config_border()) border_init();
        wm_init();
        apps_init();
        if (config_menubar()) menubar_init();
        if (!hotkey_init()) {
            fprintf(stderr, "just-desktops: could not create keyboard event tap\n");
            wm_restore_all();
            return 1;
        }

        // Put every parked window back before exiting.
        int sigs[3] = {SIGINT, SIGTERM, SIGHUP};
        for (int i = 0; i < 3; i++) {
            signal(sigs[i], SIG_IGN);
            signal_sources[i] = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, sigs[i], 0, dispatch_get_main_queue());
            dispatch_source_set_event_handler(signal_sources[i], ^{
                wm_restore_all();
                border_hide();
                exit(0);
            });
            dispatch_resume(signal_sources[i]);
        }
        wm_on_focus();
    }

    // The status item needs AppKit's own event loop to draw and take clicks.
    if (config_menubar()) [NSApp run];

    // Drain autoreleased objects after every event so memory stays flat.
    for (;;) {
        @autoreleasepool {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 3600, true);
        }
    }
}
