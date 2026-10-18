#import <AppKit/AppKit.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <sys/file.h>
#include <unistd.h>
#include "jd.h"

extern char **environ;
static dispatch_source_t signal_sources[3];

// The app is identified by its bundle id. JD_APP_MODE=1 turns the app mode on for the bare binary:
// a test hook for the app's menu and settings without building a bundle; it is not a user setting.
bool jd_app_mode(void) {
    static int app = -1;
    if (app < 0) {
        CFBundleRef b = CFBundleGetMainBundle();
        CFStringRef id = b ? CFBundleGetIdentifier(b) : NULL;
        app = (id && CFEqual(id, CFSTR("dev.n-seiji.just-desktops"))) || getenv("JD_APP_MODE") != NULL;
    }
    return app;
}

void jd_quit(void) {
    wm_restore_all();
    border_hide();
    exit(0);
}

// Windows come back on screen, then a helper starts this program again once this process is gone
// and has released the lock, and this process exits. (exec in place is not an option: the new
// image cannot reach LaunchServices, so it sees no running apps and draws an empty strip.) A bundle
// is opened through LaunchServices, a bare binary is run directly.
void jd_restart(void) {
    wm_restore_all();
    border_hide();
    char path[PATH_MAX], pid[16];
    uint32_t n = sizeof path;
    if (_NSGetExecutablePath(path, &n) == 0) {
        char *bundle = strstr(path, ".app/Contents/MacOS/");
        if (bundle) bundle[4] = 0; // path is now the .app
        snprintf(pid, sizeof pid, "%d", getpid());
        char *args[] = {"sh", "-c",
                        "while kill -0 \"$1\" 2>/dev/null; do sleep 0.1; done; "
                        "if [ -d \"$2\" ]; then exec /usr/bin/open \"$2\"; else exec \"$2\"; fi",
                        "sh", pid, path, NULL};
        posix_spawnattr_t attr;
        posix_spawnattr_init(&attr);
        posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);
        int err = posix_spawn(NULL, "/bin/sh", NULL, &attr, args, environ);
        if (err == 0) exit(0);
        errno = err;
    }
    fprintf(stderr, "just-desktops: restart failed: %s\n", strerror(errno));
    exit(1);
}

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
    // close-on-exec: the restart helper must not inherit the lock, or the new instance could not take it
    if (lock >= 0) fcntl(lock, F_SETFD, FD_CLOEXEC);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "just-desktops: already running\n");
        return 0; // not a failure: launchd's KeepAlive would respawn a nonzero exit forever
    }

    @autoreleasepool {
        // Installed before the Accessibility wait so a signal there exits cleanly too.
        int sigs[3] = {SIGINT, SIGTERM, SIGHUP};
        for (int i = 0; i < 3; i++) {
            signal(sigs[i], SIG_IGN);
            signal_sources[i] = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, sigs[i], 0, dispatch_get_main_queue());
            dispatch_source_set_event_handler(signal_sources[i], ^{ jd_quit(); });
            dispatch_resume(signal_sources[i]);
        }

        NSDictionary *opts = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt: @YES};
        if (!AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)opts)) {
            fprintf(stderr, "just-desktops: waiting for Accessibility permission\n");
            if (jd_app_mode()) {
                app_wait_accessibility();
            } else {
                // Poll on the run loop, not sleep(): signals keep working while we wait.
                while (!AXIsProcessTrusted()) {
                    @autoreleasepool {
                        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, false);
                    }
                }
            }
        }
        config_set_app_mode(jd_app_mode());
        config_load();
        if (config_switcher_style() >= SW_SQUARE) thumb_request_access();
        wm_init();
        apps_init();
        if (config_menubar() != MB_OFF) menubar_init(config_menubar());
        // After AppKit is up: edge windows made before it stay ordered out, except the first.
        if (config_border()) border_init();
        if (!hotkey_init()) {
            fprintf(stderr, "just-desktops: could not create keyboard event tap\n");
            wm_restore_all();
            return 1;
        }

        wm_on_focus();
    }

    // The status item needs AppKit's own event loop to draw and take clicks.
    if (config_menubar() != MB_OFF) [NSApp run];

    // Drain autoreleased objects after every event so memory stays flat.
    for (;;) {
        @autoreleasepool {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 3600, true);
        }
    }
}
