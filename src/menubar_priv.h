#pragma once
// What the menu bar's files share. Other modules use menubar.h.
#import <AppKit/AppKit.h>
#include "config.h"

// menubar_glyph.m
CGImageRef menubar_mask(pid_t pid); // the app's glyph mask (white on clear), cached; NULL when it has no icon

// menubar_overflow.m: the "+k" panel
void menubar_overflow_open(NSButton *b, NSRect slot); // slot: the "+k" slot in the button's coordinates
void menubar_overflow_close(void);
bool menubar_overflow_is_open(void);
bool menubar_overflow_just_closed(void); // a click on the item closes the panel on mouse down, before its action runs

// menubar_menu.m: the right-click menu
NSMenu *menubar_menu(menubar_mode_t mode); // built on every call
