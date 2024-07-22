/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_ps5video_h_
#define SDL_ps5video_h_

#include <sys/event.h>

#include "../SDL_sysvideo.h"

typedef struct PS5_VideoBuf {
    void *data;
    uint64_t junk0[3];
} PS5_VideoBuf;


typedef struct PS5_VideoAttr {
    uint8_t junk0[80];
} PS5_VideoAttr;

typedef struct PS5_DeviceData
{
    int handle;
    PS5_VideoBuf vbuf[2];
    struct kevent *evt_queue;
    intptr_t paddr;
    size_t memsize;
} PS5_DeviceData;

typedef struct PS5_DrawChunk {
    uint32_t *src;
    uint32_t *dst;

    uint16_t frame_width;
    uint16_t frame_height;

    size_t src_start;
    size_t src_end;
} PS5_DrawChunk;


int sceSystemServiceHideSplashScreen(void);

int sceKernelAllocateMainDirectMemory(size_t, size_t, int, intptr_t*);
int sceKernelMapDirectMemory(void**, size_t, int, int, intptr_t, size_t);
int sceKernelReleaseDirectMemory(intptr_t, size_t);

int sceKernelCreateEqueue(struct kevent **, const char *);
int sceKernelWaitEqueue(struct kevent*, struct kevent*, int, int*, uint*);
int sceKernelDeleteEqueue(struct kevent*);

int sceVideoOutOpen(int, int, int, const void*);
void sceVideoOutClose(int);

int sceVideoOutAddFlipEvent(struct kevent*, int, void*);
int sceVideoOutSetFlipRate(int, int);
int sceVideoOutSubmitFlip(int, int, uint32_t, int64_t);
int sceVideoOutDeleteFlipEvent(struct kevent*, int);

void sceVideoOutSetBufferAttribute2(PS5_VideoAttr*, uint64_t, uint32_t, uint32_t,
                                    uint32_t, uint64_t, uint32_t, uint64_t);
int sceVideoOutRegisterBuffers2(int, int, int, PS5_VideoBuf*, int, PS5_VideoAttr*,
                                int, void*);

#endif /* SDL_ps5video_h_ */

/* vi: set ts=4 sw=4 expandtab: */

/* emacs: */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* indent-tabs-mode: nil */
/* End: */
