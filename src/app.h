#pragma once
// The process lifecycle (main.m) and the .app bundle (app.m).
#include <stdbool.h>

bool jd_app_mode(void); // running as dev.n-seiji.just-desktops.app
void jd_quit(void);     // put every parked window back and exit(0): the one clean way out
void jd_restart(void);  // put every parked window back and start this executable again

void app_wait_accessibility(void); // first-run panel; returns once Accessibility is granted
void app_set(const char *key, const char *value); // write one config key, then jd_restart
void app_open_config(void);
int app_login_state(void); // 0 off, 1 on, 2 needs approval in System Settings
void app_toggle_login(void);
void app_open_about(void);
