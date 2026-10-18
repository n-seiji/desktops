# Menu bar

An optional menu bar item shows one glyph per window on this desktop, and the current desktop's key in a box. Turn it on with `menubar = on`. It is off by default.

![The menu bar item](images/menubar.png)

## The strip

- One monochrome glyph per window. Two Finder windows show two Finder glyphs.
- Glyphs are grouped by app. An app's windows stay in a fixed order, so changing focus never reshuffles them.
- The current desktop's key sits at the right end in an outlined box. The item grows and shrinks to the left, so the key stays put.
- The focused window's glyph (only that one, not every window of its app) is cut out of a filled tile.
- The glyphs are template images, so macOS tints them for light and dark menu bars.
- It redraws only when the desktop, the focused window or the set of windows changes.

## Clicking

- Click a glyph to focus exactly that window.
- Clicking the key box or a gap between glyphs does nothing.
- Right-click or control-click the item for a menu with Quit. The item has no permanent menu, so left clicks are never swallowed.

## The `+k` slot

The item shows at most 10 slots. With more than 10 windows it shows the first 9 and a `+k` slot that counts the rest.

Click `+k` to open a small panel under it. The panel lists the hidden windows in the same order, each as the app's icon and the window title (cleaned the same way as in the switcher). Click a row to focus that window.

- Down/Up or Tab/Shift+Tab move the highlight (wrapping).
- Return focuses the highlighted window.
- Esc, or a click anywhere else, closes the panel without changing focus.
- The panel closes by itself when the set of windows changes.
- It follows the system's light or dark appearance and shows at most 14 rows at a time. The mouse wheel scrolls longer lists.

## Memory

AppKit's status item costs about 5MB. With `menubar = off` none of it is loaded.

The panel is created on click and freed on close. Opening it once costs about 3MB of resident memory that stays after it closes (window and text machinery loaded on first use). Repeated opens do not add more.
