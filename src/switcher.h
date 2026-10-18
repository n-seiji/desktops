#pragma once
// The ⌥Tab window switcher; the style comes from config (list, square or instant).

void switcher_step(int dir); // ⌥Tab (dir 1) or ⌥⇧Tab (-1)
void switcher_commit(void);  // ⌥ released: focus the selection
void switcher_cancel(void);  // Esc, or a desktop key
