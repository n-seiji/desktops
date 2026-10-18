#pragma once
// Writing the config file: the app's settings menu changes one line at a time.
#include <stdbool.h>
#include <stddef.h>

bool config_path(char *buf, size_t n);  // ~/.config/just-desktops/config
const char *config_sample(void);        // the commented template written for a new file
char *config_set(const char *text, const char *key, const char *value); // text with key set; caller frees
int config_write_key(const char *path, const char *key, const char *value); // 0, or -1 with errno
int config_ensure_file(const char *path); // creates the file (and directories) from the sample if missing
