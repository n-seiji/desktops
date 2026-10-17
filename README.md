<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <img src="assets/logo-light.svg" width="128" height="128" alt="just-desktops">
  </picture>
</p>

# just-desktops

A tiny macOS-only daemon that does two things: switch between workspaces, and draw a ring around the focused window. Nothing else.

## Inspiration

just-desktops is a stripped-down mix of two great tools:

- [AeroSpace](https://github.com/nikitabobko/AeroSpace) by Nikita Bobko: the workspace model. Windows on inactive workspaces are parked in a corner of the screen, so macOS Spaces and SIP are never touched.
- [JankyBorders](https://github.com/FelixKratz/JankyBorders) by Felix Kratz: the border. It is drawn directly through the private SkyLight framework instead of an AppKit window.

If you want tiling, layouts, colors or more options, use those projects. just-desktops only keeps the parts above and aims for the smallest possible memory footprint.

## Features

| Key | Action |
|---|---|
| ⌥1 ⌥2 ⌥3 ⌥Q ⌥W ⌥E | Switch to Desktop 1 … 6 |
| ⌥⇧ + the same key | Move the focused window to that desktop |
| ⌥Tab / ⌥⇧Tab (hold ⌥) | Open a list of the windows in this workspace and move the selection forward / backward (wraps around). Release ⌥ to switch to the selected window; Esc cancels |

- Each switcher row shows the app name when the app has one window. With several, rows show the window titles ("App 2" when a title is empty or repeated).
- The switcher panel exists only while ⌥ is held after a Tab. Nothing is held in memory while it is closed.
- ⌥Tab and ⌥⇧Tab are captured by just-desktops only while the switcher is on (the default), so apps never see those keys then. With `switcher = off` they pass through to apps and no switcher code runs.
- The switcher lists only windows that are really shown: windows of other Spaces, hidden apps or parked off screen (such as Raycast's idle window) are left out.
- The desktop keys are set with `keys` in the config (digits and letters, at most 10). Other keys pass through untouched.
- Apps can be pinned to a desktop in the config file. A pinned app's windows start on that desktop, both existing ones and ones opened later. The pin applies only when a window first appears.
- Focusing a window that belongs to another desktop (Cmd-Tab, Dock, a shortcut, unhiding the app) moves that one window to the current desktop instead of switching. Pinned apps are pulled too.
- The border is a thin ring floating 3pt off the focused window, brighter along the top. It is white in Dark mode and black in Light mode, and the switcher panel has a light and a dark look too. Both follow the system appearance, and the border updates the moment it changes. Turn it off with `border = off` in the config; no border window is created then.

## Design

- **One process.** Workspaces and the border live in the same daemon.
- **Mostly C.** No Swift runtime and no AppKit UI. The only Objective-C is `src/apps.m`, which listens for app launch, quit and activation events from NSWorkspace.
- **Border through SkyLight**, like JankyBorders, so no NSWindow is created.
- **Hotkeys through a CGEventTap** on the main run loop. No extra threads.

SkyLight is a private Apple API, so a macOS update can break the border, the same risk JankyBorders and yabai carry.

## Install

```sh
mise use -g github:n-seiji/desktops
just-desktops
```

On first launch macOS asks for Accessibility permission. Allow `just-desktops` (or your terminal, if you started it from one) under System Settings > Privacy & Security > Accessibility; the daemon starts as soon as permission is granted. On exit, every parked window is moved back.

To start at login, run this inside the repository:

```sh
mise run service     # register and start a LaunchAgent
mise run unservice   # stop and unregister it
```

Upgrading through mise changes the binary path, so run `mise run service` again and re-grant Accessibility permission afterwards.

## Configuration

`~/.config/just-desktops/config` (optional; see `config.example`):

```
border = on
switcher = on
keys = 1 2 3 q w e
com.apple.Safari = 1
com.googlecode.iterm2 = 2
```

Restart the daemon to apply changes.

## Development

Requires the Xcode Command Line Tools (`xcode-select --install`) and [mise](https://mise.jdx.dev).

```sh
mise run build   # build bin/just-desktops
mise run run     # build and run in the foreground
mise run test    # run the unit tests
mise tasks       # list all tasks
```

Pushing a `v*` tag makes GitHub Actions publish arm64 and x86_64 binaries to Releases.

## Limitations

- Windows are parked on the main display only; multiple displays are not supported yet.
- After a crash, parked windows stay in the bottom-right corner until the next launch, which moves them back on screen.
- ⌥Tab, ⌥⇧Tab and the ⌥ / ⌥⇧ modifiers are fixed (the switcher can only be turned on or off); only the desktop keys are configurable.

## License

[GPL-3.0](LICENSE).

## Support

If just-desktops saves you a few keystrokes a day, you can [buy me a coffee](https://buymeacoffee.com/sijis).
