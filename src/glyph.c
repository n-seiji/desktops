#include "glyph.h"
#include <stdlib.h>
#include <string.h>

enum {
    RIM = 2,  // pixels shaved off the icon's outline
    LO = 75,  // colour distance from the tile where the glyph starts to show...
    HI = 195, // ...and where it is fully opaque
};

void glyph_cut(uint8_t *px) {
    enum { N = GLYPH_PX * GLYPH_PX, BINS = 4096 };
    static uint16_t votes[BINS];
    memset(votes, 0, sizeof votes);
    int best = 0;
    for (int i = 0; i < N; i++) {
        const uint8_t *p = px + i * 4;
        if (p[3] < 250) continue;
        int bin = (p[0] >> 4) << 8 | (p[1] >> 4) << 4 | p[2] >> 4;
        if (++votes[bin] > votes[best]) best = bin;
    }
    int bg[3] = {(best >> 8 & 15) * 17, (best >> 4 & 15) * 17, (best & 15) * 17};

    // The tile's own rim (bevel, shadow, antialiased edge) is not part of the symbol:
    // keep only pixels that are RIM pixels away from anything transparent.
    static uint8_t inner[N], tmp[N];
    for (int i = 0; i < N; i++) inner[i] = px[i * 4 + 3];
    for (int pass = 0; pass < RIM; pass++) {
        for (int y = 0; y < GLYPH_PX; y++)
            for (int x = 0; x < GLYPH_PX; x++) {
                uint8_t m = inner[y * GLYPH_PX + x];
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int u = x + dx, v = y + dy;
                        uint8_t o = u < 0 || v < 0 || u >= GLYPH_PX || v >= GLYPH_PX ? 0 : inner[v * GLYPH_PX + u];
                        if (o < m) m = o;
                    }
                tmp[y * GLYPH_PX + x] = m;
            }
        memcpy(inner, tmp, N);
    }

    for (int i = 0; i < N; i++) {
        uint8_t *p = px + i * 4;
        int a = p[3], out = 0;
        if (a) {
            int d = 0;
            for (int k = 0; k < 3; k++) {
                int v = p[k] * 255 / a; // un-premultiply
                d += abs(v - bg[k]);
            }
            int t = (d - LO) * 255 / (HI - LO); // soft threshold
            t = t < 0 ? 0 : t > 255 ? 255 : t;
            out = t * inner[i] / 255;
        }
        p[0] = p[1] = p[2] = p[3] = (uint8_t)out;
    }
}
