#pragma once
#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>

#define JD_VERSION "0.1.0"
#define JD_MAX_WS 10 // ⌥1-9 and ⌥0

// config.c
void config_load(void);
int config_assign(const char *bundle_id);
bool config_border(void);
int config_workspaces(void);

// border.c
void border_init(void);
void border_show(CGRect frame, uint32_t target_wid);
void border_hide(void);

// hotkey.c
bool hotkey_init(void);

// wm.c
void wm_init(void);
void wm_add_app(pid_t pid, const char *bundle_id);
void wm_remove_app(pid_t pid);
void wm_on_focus(void);
void wm_switch(int ws);
void wm_move_focused(int ws);
void wm_cycle(int dir);
void wm_restore_all(void);

// apps.m
void apps_init(void);
pid_t apps_frontmost(void);
