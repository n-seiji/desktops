# Workspaces

just-desktops keeps its own desktops. Windows of inactive desktops are parked in a corner of the main display, so macOS Spaces and SIP are never touched. This page covers pins, apps on no desktop, pulling windows, the border and the modifier.

## Desktops and keys

- The desktop keys are set with `keys` in the config: digits and letters, at most 10. The n-th key is Desktop n. Other keys pass through untouched.
- On exit, every parked window is moved back. After a crash, parked windows stay in the bottom-right corner until the next launch. Windows of `none` apps are not brought back; see [Apps on no desktop](#apps-on-no-desktop).
- Windows are parked on the main display only.

## Pulling windows

Focusing a window that belongs to another desktop (Cmd-Tab, Dock, a shortcut, unhiding the app) moves that one window to the current desktop. It does not switch desktops. Apps on no desktop are left out (see [Apps on no desktop](#apps-on-no-desktop)). Pinned apps are pulled too: a pinned app's newly focused window comes to the current desktop, not to its pinned one.

## Pins

Pin an app to a desktop with `<bundle id> = <n>` in the config. Find a bundle id with `osascript -e 'id of app "Safari"'`.

```
com.apple.Safari = 1
com.googlecode.iterm2 = 2
```

- A window of a pinned app is placed on its desktop when it first appears, and it is not focused. This covers windows that exist when just-desktops starts and windows opened later.
- A window that is focused when it appears, or that you focus later, is pulled to the current desktop like any other. A pin never keeps away a window you are using.
- The pin is applied once, when the window first appears.
- A pin to a desktop number higher than the number of `keys` is ignored.

## Apps on no desktop

Set `<bundle id> = none` in the config for an app you call up with a shortcut, such as a quick terminal or a password manager.

```
com.mitchellh.ghostty = none
```

- The app's windows are never parked. Every window of the app is visible on every desktop.
- They are not in ⌥Tab or the menu bar, and they get no border.
- Focusing them neither pulls them nor switches desktops.
- ⌥⇧ + key does nothing while one of them has focus.
- ⌥ + another desktop's key switches desktops and focuses that desktop's last window, if it has one; a quick terminal that hides on focus loss closes then. The current desktop's key does nothing.
- ⌥Tab while one of them has focus starts at the edge window: the leftmost, or the rightmost with ⇧.
- The setting is per app, so all of an app's windows are affected, not only the quick one.
- Changes apply on restart. If a window of an app that was tracked before is left in the corner after a crash, remove the `= none` line, restart to bring it back, then add the line again.

## Border

The border is a thin ring floating 3pt off the focused window, brighter along the top.

- Its corners follow the window's own corner radius as WindowServer reports it (16pt for most windows, up to 26pt for newer styles). Without a reported radius it uses 16pt.
- It is white in Dark mode and black in Light mode. It follows the system appearance and updates the moment that changes. The switcher panel has a light and a dark look too.
- It is drawn through the private SkyLight framework, so no NSWindow is created.
- `border = off` creates no border window.

## Modifier

`⌥` in these docs is the base modifier. Option is the default. Set `modifier = cmd` or `modifier = ctrl` for Command or Control. `option`, `alt`, `command` and `control` are accepted too, in any case. An unknown value gives Option.

Every hotkey follows it: the desktop keys, moving a window, the switcher, releasing it to commit, Esc and the desktop keys while the panel is open. The Shift variants stay.

A key fires only when exactly the base modifier (plus Shift) is down. With `modifier = cmd`, holding Option or Control in addition lets the key through untouched. There is no Hyper key.

Command and Control collide with system shortcuts, and just-desktops swallows these keys system wide while it runs.

- With `cmd`, Cmd-Tab (the macOS app switcher) and Cmd-digit (app shortcuts such as browser tabs 1 to 9) stop working.
- With `ctrl`, Ctrl-Tab and Ctrl-digit stop working. These are browser and terminal tab shortcuts, and Mission Control's desktop shortcuts if you enabled them.

just-desktops does not work around this. Choose a modifier whose combinations you do not need.
