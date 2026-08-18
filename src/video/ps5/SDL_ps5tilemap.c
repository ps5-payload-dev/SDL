/*
  Simple DirectMedia Layer
  Copyright (C) 2026 John Törnblom <john.tornblom@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PS5

#include <immintrin.h>

#include "../SDL_sysvideo.h"

#define PS5_TILE_WIDTH  512
#define PS5_TILE_HEIGHT 128
#define PS5_TILE_SIZE   (PS5_TILE_WIDTH * PS5_TILE_HEIGHT)
#define PS5_TILE_BAND   8

typedef struct PS5_Tilemap
{
    Uint32 width;
    Uint32 height;
    Uint32 *colx;
    Uint32 yoff[PS5_TILE_HEIGHT];
    SDL_Rect pending[2];
    SDL_bool full[2];
} PS5_Tilemap;

static Uint32 PS5_TileOffset(Uint32 x, Uint32 y)
{
    return (((x) & 1u) << 0 |
            ((x >>  1) & 1u) << 1 |
            ((y >>  0) & 1u) << 2 |
            ((y >>  1) & 1u) << 3 |
            ((y >>  2) & 1u) << 4 |
            ((x >>  2) & 1u) << 5 |
            (((x >> 3) ^ (y >> 3)) & 1u) << 6 |
            (((x >> 4) ^ (y >> 4)) & 1u) << 7 |
            (((x >> 6) ^ (y >> 5)) & 1u) << 8 |
            (((x >> 5) ^ (y >> 6)) & 1u) << 9 |
            ((y >>  3) & 1u) << 10 |
            ((x >>  4) & 1u) << 11 |
            ((y >>  6) & 1u) << 12 |
            ((x >>  6) & 1u) << 13 |
            ((x >>  7) & 1u) << 14 |
            ((x >>  8) & 1u) << 15);
}

static Uint32 PS5_TilePixel(Uint32 x, Uint32 y, Uint32 width)
{
    return ((x / PS5_TILE_WIDTH) * PS5_TILE_SIZE +
            (y / PS5_TILE_HEIGHT) * (PS5_TILE_HEIGHT * width) +
            ((PS5_TileOffset((x % PS5_TILE_WIDTH), 0) ^
              PS5_TileOffset(0, (y % PS5_TILE_HEIGHT)))));
}

__attribute__((target("avx2")))
static void PS5_TileArea(const PS5_Tilemap *tmap,
                         const Uint32 *src, Uint32 pitch, Uint32 *dst,
                         Uint32 x0, Uint32 x1, Uint32 y0, Uint32 y1)
{
    const Uint32 *colx = tmap->colx;
    const Uint32 width = tmap->width;
    const Uint32 quads = x1 / 4;
    const Uint32 *p;
    const Uint32 *s;
    Uint32 rows;
    Uint32 base;
    Uint32 yo;
    Uint32 *d;
    Uint32 *o;
    __m256i v;
    Uint32 x;
    Uint32 y;
    Uint32 k;
    Uint32 g;

    for(y = y0; y < y1; y += PS5_TILE_BAND) {
        base = (y / PS5_TILE_HEIGHT) * (PS5_TILE_HEIGHT * width);
        yo = tmap->yoff[y % PS5_TILE_HEIGHT];
        s = src + y * pitch;
        d = dst + base;
        rows = SDL_min(PS5_TILE_BAND, y1 - y);

        for(g = x0 / 4; g < quads; g++) {
            p = s + (g << 2);
            o = d + (colx[g] ^ yo);

            for(k = 0; k + 1 < rows; k += 2) {
                v = _mm256_inserti128_si256(_mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)p)),
                                            _mm_loadu_si128((const __m128i *)(p + pitch)), 1);
                _mm256_stream_si256((__m256i *)o, v);
                p += 2 * pitch;
                o += 8;
            }

            if (rows & 1) {
                _mm_stream_si128((__m128i *)o, _mm_loadu_si128((const __m128i *)p));
            }
        }

        for(k=0; k<rows; k++) {
            p = s + k * pitch;
            for(x=quads*4; x<x1; x++) {
                dst[PS5_TilePixel(x, y + k, width)] = p[x];
            }
        }
    }
    _mm_sfence();
}

static void PS5_Tilemap_RectUnion(SDL_Rect *acc, const SDL_Rect *r)
{
    int x0;
    int x1;
    int y0;
    int y1;

    if(SDL_RectEmpty(r)) {
        return;
    }
    if(SDL_RectEmpty(acc)) {
        *acc = *r;
        return;
    }

    x0 = SDL_min(acc->x, r->x);
    y0 = SDL_min(acc->y, r->y);
    x1 = SDL_max(acc->x + acc->w, r->x + r->w);
    y1 = SDL_max(acc->y + acc->h, r->y + r->h);

    acc->x = x0;
    acc->y = y0;
    acc->w = x1 - x0;
    acc->h = y1 - y0;
}

PS5_Tilemap* PS5_Tilemap_Create(Uint32 width, Uint32 height)
{
    PS5_Tilemap* tmap;
    Uint32 x;
    Uint32 y;

    tmap = (PS5_Tilemap*)SDL_malloc(sizeof(PS5_Tilemap));
    if(!tmap) {
        return NULL;
    }

    tmap->colx = (Uint32*)SDL_malloc(sizeof(Uint32) * (width / 4 + 1));
    if(!tmap->colx) {
        SDL_free(tmap);
        return NULL;
    }

    for(x = 0; x < width; x += 4) {
        tmap->colx[x / 4] = (x / PS5_TILE_WIDTH) * PS5_TILE_SIZE +
            PS5_TileOffset((x % PS5_TILE_WIDTH), 0);
    }
    for(y = 0; y < PS5_TILE_HEIGHT; y++) {
        tmap->yoff[y] = PS5_TileOffset(0, y);
    }

    tmap->width = width;
    tmap->height = height;
    SDL_zero(tmap->pending);
    tmap->full[0] = tmap->full[1] = SDL_TRUE;

    return tmap;
}

void PS5_Tilemap_Blit(PS5_Tilemap *tmap,
                      const void *pixels, int pitch, void *tiled,
                      int buf_idx, const SDL_Rect *rects, int numrects,
                      int off_x, int off_y)
{
    const SDL_Rect screen = {0, 0, tmap->width, tmap->height};
    SDL_Rect damage;
    SDL_Rect area;
    SDL_Rect r;
    int i;

    SDL_zero(damage);
    if(rects && numrects > 0) {
        for(i = 0; i < numrects; i++) {
            r = rects[i];
            r.x += off_x;
            r.y += off_y;
            if(SDL_IntersectRect(&r, &screen, &r)) {
                PS5_Tilemap_RectUnion(&damage, &r);
            }
        }
    } else {
        damage = screen;
    }

    if(tmap->full[buf_idx]) {
        area = screen;
    } else {
        area = tmap->pending[buf_idx];
        PS5_Tilemap_RectUnion(&area, &damage);
    }

    tmap->full[buf_idx] = SDL_FALSE;
    SDL_zero(tmap->pending[buf_idx]);
    PS5_Tilemap_RectUnion(&tmap->pending[buf_idx ^ 1], &damage);

    if(SDL_RectEmpty(&area)) {
        return;
    }

    area.w += area.x & 3;
    area.x &= ~3;
    area.w = SDL_min((area.w + 3) & ~3, tmap->width - area.x);

    area.h += area.y & (PS5_TILE_BAND - 1);
    area.y &= ~(PS5_TILE_BAND - 1);
    area.h = SDL_min((area.h + PS5_TILE_BAND - 1) & ~(PS5_TILE_BAND - 1),
                     tmap->height - area.y);

    PS5_TileArea(tmap, (const Uint32*)pixels, pitch, (Uint32*)tiled,
                 area.x, area.x + area.w, area.y, area.y + area.h);
}


void PS5_Tilemap_Destroy(PS5_Tilemap *tmap)
{
    if(!tmap) {
        return;
    }

    SDL_free(tmap->colx);
    SDL_free(tmap);
}


#endif /* SDL_VIDEO_DRIVER_PS5 */

/* vi: set ts=4 sw=4 expandtab: */

/* emacs: */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* indent-tabs-mode: nil */
/* End: */
