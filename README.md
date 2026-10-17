# just-desktops

A tiny macOS-only daemon that does two things: switch between workspaces, and draw a white border around the focused window. Nothing else.

## Inspiration

just-desktops is a stripped-down mix of two great tools:

- [AeroSpace](https://github.com/nikitabobko/AeroSpace) by Nikita Bobko: the workspace model. Windows on inactive workspaces are parked in a corner of the screen, so macOS Spaces and SIP are never touched.
- [JankyBorders](https://github.com/FelixKratz/JankyBorders) by Felix Kratz: the border. It is drawn directly through the private SkyLight framework instead of an AppKit window.

If you want tiling, layouts, colors or more options, use those projects. just-desktops only keeps the parts above and aims for the smallest possible memory footprint.

## Features

| Key | Action |
|---|---|
| ⌥1 … ⌥9 (⌥0) | Switch to workspace 1 … 9 (10) |
| ⌥⇧1 … ⌥⇧9 (⌥⇧0) | Move the focused window to that workspace |
| ⌥Tab / ⌥⇧Tab (hold ⌥) | Open a list of the windows in this workspace and move the selection forward / backward (wraps around). Release ⌥ to switch to the selected window; Esc cancels |

- Each switcher row shows the app name when the app has one window. With several, rows show the window titles ("App 2" when a title is empty or repeated).
- The switcher panel exists only while ⌥ is held after a Tab. Nothing is held in memory while it is closed.
- ⌥Tab and ⌥⇧Tab are always captured by just-desktops, so apps never see those keys.
- Apps can be pinned to a workspace in the config file. Existing and newly opened windows go there.
- Focusing a window on another workspace (Cmd-Tab, Dock, …) switches to that workspace.
- The border is a thin white ring floating 3pt off the focused window, brighter along the top. Turn it off with `border = off` in the config; no border window is created then.

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
workspaces = 9
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
- Key bindings are fixed.
