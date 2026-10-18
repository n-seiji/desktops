# Architecture

just-desktops is one process on the main run loop. It emulates desktops by moving windows, so it never touches macOS Spaces. This page is the map for reading the code in `src/`.

## Modules

| File | Job |
| --- | --- |
| `main.m` | Start up, single-instance lock, signals, `jd_quit` and `jd_restart`. |
| `app.m` | The `.app` bundle only: first-run Accessibility panel, login item, About, settings writes. |
| `config.c`, `config_write.c` | Read the config file once; write one key back for the app's settings menu. |
| `apps.m` | NSWorkspace launch, quit and activate events; app names and icons. |
| `wm.c` | The apps and windows we track, and the Accessibility notifications that update them. |
| `wm_focus.c` | Park and unpark windows, switch desktops, move a window, pull windows macOS focuses on its own. |
| `wm_list.c` | The window lists the switchers and the menu bar draw from. |
| `ax.c` | Typed reads of Accessibility attributes. |
| `hotkey.c` | The keyboard event tap: desktop keys, ⌥Tab, Esc. |
| `switcher.c`, `switcher_card.c`, `swkit.c` | List and instant switcher, square switcher, the SkyLight drawing helpers they share. |
| `thumb.m` | Window screenshots for the square switcher (ScreenCaptureKit). |
| `border.c` | The focus ring: four thin SkyLight windows. |
| `menubar.m` | The status item: strip drawing, click handling. |
| `menubar_glyph.m`, `menubar_overflow.m`, `menubar_menu.m` | App glyph masks, the "+k" panel, the right-click menu. |

Pure logic, with a unit test each in `tests/`: `select.c` (selection stepping), `keyrules.c` (modifier match, repeat throttle), `strip.c` (strip geometry and hit test), `order.c` (menu bar order and overflow split), `shown.c` (visibility, grace period, parking point), `cardmath.c` (easing and card layout), `border_geom.c` (ring rects), `glyph.c` (icon to mask), `title.c` (row text).

Headers: one per module, and `common.h` for the limits and `sw_entry_t` several modules share. `wm_priv.h` and `menubar_priv.h` are for the files of that module only.

## Data flow

Events come in from three places: Accessibility observers (window created, destroyed, focused, minimized), NSWorkspace (app launched, quit, activated) and the keyboard tap. `wm*.c` is the only owner of state: which windows exist, their desktop, whether they are parked, which desktop is current.

After every change `wm_refresh_ui` reads the focused window once, then calls `border_show` or `border_hide` and `menubar_update`. The switchers pull a fresh list from `wm_windows` when they open and call `wm_focus_wid` to commit. Nothing outside `wm*.c` writes window state.

## Threading

There is one thread. The event tap, the Accessibility observers, the SkyLight notification port, the animation timers and the signal handlers all run on the main run loop. The tap callback only decides and then hops to the main queue with `dispatch_async`; the work happens there. With the menu bar on, `[NSApp run]` is that loop; without it, a plain `CFRunLoopRunInMode` loop with an autorelease pool per event. ScreenCaptureKit completions are delivered on the main queue.

## Memory rules

Idle footprint is the product, so these are rules, not tuning.

- No AppKit unless the menu bar is on. The border, both panel switchers and all hotkeys use SkyLight and CoreGraphics directly.
- Allocate on demand, free on close. A switcher's windows, contexts, fonts, icons and entries exist only while ⌥ is held. Closing calls `malloc_zone_pressure_relief`.
- Draw large surfaces once. Flushing a SkyLight context again keeps extra backing surfaces alive, so the list highlight and the square cards are separate windows that move, and the cards are drawn at 1x.
- Thin windows over big ones. The ring is four thin windows because a window's backing store is its full rect.
- Timers run only while something moves.
- No NSPopover (on macOS 26 each one keeps a 5MB surface after closing); the "+k" panel is one hand-drawn view.
- ScreenCaptureKit is linked weakly and touched only for the square style.
- Glyph masks are cached per app and dropped when the app quits.

## Private APIs

All declarations are in `skylight.h`. SkyLight (window creation, tags, shapes, ordering, level, blur, move and resize notifications, corner radii) is used by `border.c`, `swkit.c`, `switcher.c` and `switcher_card.c`. The one HIServices call, `_AXUIElementGetWindow` (Accessibility element to window id), is used by `wm.c`. A macOS update can break these; everything else is public API.
