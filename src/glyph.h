#pragma once
// Turns an app icon into the white line-art mask the menu bar draws.
#include <stdint.h>

enum { GLYPH_PX = 40 }; // the mask's side in pixels

// In place on a GLYPH_PX x GLYPH_PX bitmap of premultiplied RGBA bytes: the tile behind the symbol
// disappears. The tile is the icon's most common colour (4 bits per channel); a pixel's alpha is
// how far its colour is from that background, so only the symbol's strokes and shapes stay. The
// tile's own rim (bevel, shadow, antialiased edge) is cut away. Every channel ends up as that alpha.
void glyph_cut(uint8_t *px);
