/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_PS5

#include <sys/event.h>

#include "../SDL_sysvideo.h"

#define PS5_SURFACE "_PS5_Surface"

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
} PS5_DeviceData;



int sceSystemServiceHideSplashScreen(void);

int sceKernelAllocateMainDirectMemory(size_t, size_t, int, intptr_t*);
int sceKernelMapDirectMemory(void**, size_t, int, int, intptr_t, size_t);

int sceKernelCreateEqueue(struct kevent **, const char *);
int sceKernelWaitEqueue(struct kevent*, struct kevent*, int, int*, uint*);

int sceVideoOutOpen(int, int, int, const void*);
void sceVideoOutClose(int);

int sceVideoOutAddFlipEvent(struct kevent*, int, void*);
int sceVideoOutSetFlipRate(int, int);
int sceVideoOutSubmitFlip(int, int, uint, int64_t);
void sceVideoOutSetBufferAttribute2(PS5_VideoAttr*, uint64_t, uint32_t, uint32_t,
				    uint32_t, uint64_t, uint32_t, uint64_t);
int sceVideoOutRegisterBuffers2(int, int, int, PS5_VideoBuf*, int, PS5_VideoAttr*,
				int, void*);


typedef struct pixel {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} pixel_t;


static void
rainbow_draw_frame(uint32_t frame_id, pixel_t *frame, size_t size) {
  float progress = fmodf((float)frame_id / 60.0f, 1.0f);
  pixel_t px = {0, 0, 0, 255};

  if(progress < 0.2f) {
    px.r = 255;
    px.g = (uint8_t)(255 * progress * 5.0f);
  } else if (progress < 0.4f) {
    px.r = (uint8_t)(255 * (0.4f - progress) * 5.0f);
    px.g = 255;
  } else if (progress < 0.6f) {
    px.g = 255;
    px.b = (uint8_t)(255 * (progress - 0.4f) * 5.0f);
  } else if (progress < 0.8f) {
    px.g = (uint8_t)(255 * (0.8f - progress) * 5.0f);
    px.b = 255;
  } else {
    px.r = (uint8_t)(255 * (progress - 0.8f) * 5.0f);
    px.b = 255;
  }

  for(int i=0; i<size; i++) {
    frame[i] = px;
  }
}

void PS5_DestroyWindowFramebuffer(_THIS, SDL_Window * window)
{
    SDL_Surface *surface;

    surface = (SDL_Surface *)SDL_SetWindowData(window, PS5_SURFACE, NULL);
    SDL_FreeSurface(surface);
}

static int PS5_CreateWindowFramebuffer(_THIS, SDL_Window * window, Uint32 * format, void ** pixels, int *pitch)
{
    SDL_Surface *surface;
    const Uint32 surface_format = SDL_PIXELFORMAT_RGB888;
    int w, h;

    PS5_DestroyWindowFramebuffer(_this, window);

    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, surface_format);
    if (!surface) {
        return -1;
    }

    SDL_SetWindowData(window, PS5_SURFACE, surface);
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;

    return 0;
}

static int PS5_UpdateWindowFramebuffer(_THIS, SDL_Window * window, const SDL_Rect * rects, int numrects)
{
    PS5_DeviceData *device_data = (PS5_DeviceData *)_this->driverdata;
    static uint32_t frame_id = 0;
    uint8_t idx = frame_id % 2;
    SDL_Surface *surface;
    struct kevent evt;
    int junk;

    int w = 3840;
    int h = 2176;

    surface = (SDL_Surface *)SDL_GetWindowData(window, PS5_SURFACE);
    if (!surface) {
        return SDL_SetError("Couldn't find surface for window");
    }

    //TODO: copy surface
    rainbow_draw_frame(frame_id, (pixel_t*)device_data->vbuf[idx].data, w*h);

    if(sceVideoOutSubmitFlip(device_data->handle, idx, 1, frame_id)) {
      return SDL_SetError("sceVideoOutSubmitFlip");
    }

    if(sceKernelWaitEqueue(device_data->evt_queue, &evt, 1, &junk, 0)) {
      return SDL_SetError("sceKernelWaitEqueue");
    }
    frame_id++;

    return 0;
}

static int PS5_VideoInit(_THIS)
{
    PS5_DeviceData *device_data = (PS5_DeviceData *)_this->driverdata;
    SDL_DisplayMode mode;
    PS5_VideoAttr vattr;

    int memsize = 0x20000000;
    int memalign = 0x20000;
    intptr_t paddr = 0;
    void* vaddr = 0;

    memset(device_data->vbuf, 0, sizeof(device_data->vbuf));
    memset(&vattr, 0, sizeof(vattr));

    sceSystemServiceHideSplashScreen();

    if(sceKernelAllocateMainDirectMemory(memsize, memalign, 3, &paddr)) {
        return SDL_SetError("sceKernelAllocateMainDirectMemory");
    }

    if(sceKernelMapDirectMemory(&vaddr, memsize, 0x33, 0, paddr, memalign)) {
      perror("sceKernelMapDirectMemory");
        return SDL_SetError("sceKernelMapDirectMemory");
    }

    device_data->handle = sceVideoOutOpen(0xff, 0, 0, NULL);
    if(device_data->handle < 0) {
        return SDL_SetError("sceVideoOutOpen");
    }

    if(sceKernelCreateEqueue(&device_data->evt_queue, "flip queue")) {
        return SDL_SetError("sceKernelCreateEqueue");
    }

    if(sceVideoOutAddFlipEvent(device_data->evt_queue, device_data->handle, NULL)) {
        return SDL_SetError("sceVideoOutAddFlipEvent");
    }
    if(sceVideoOutSetFlipRate(device_data->handle, 0)) {
        return SDL_SetError("sceVideoOutSetFlipRate");
    }
    sceVideoOutSetBufferAttribute2(&vattr, 0x8000000022000000UL, 0,
				   3840, 2160, 0, 0, 0);

    device_data->vbuf[0].data = vaddr;
    device_data->vbuf[1].data = vaddr + (memsize / 2);
    if(sceVideoOutRegisterBuffers2(device_data->handle, 0, 0, device_data->vbuf, 2, &vattr, 0, NULL)) {
      return SDL_SetError("sceVideoOutRegisterBuffers2");
    }

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGBA8888;
    mode.w = 3840;
    mode.h = 2176;
    mode.refresh_rate = 60;
    mode.driverdata = NULL;

    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }

    SDL_AddDisplayMode(&_this->displays[0], &mode);

    return 0;
}

static void PS5_VideoQuit(_THIS)
{
    PS5_DeviceData *device_data = (PS5_DeviceData *)_this->driverdata;

    if(device_data->handle != 0) {
        sceVideoOutClose(device_data->handle);
	device_data->handle = 0;
    }
}



static void PS5_DestroyDevice(SDL_VideoDevice *device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

static void PS5_PumpEvents(_THIS)
{
}

static SDL_VideoDevice *PS5_CreateDevice(void)
{
    SDL_VideoDevice *device;

    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    device->driverdata = SDL_calloc(1, sizeof(PS5_DeviceData));
    if (device->driverdata == NULL) {
        SDL_free(device);
        SDL_OutOfMemory();
        return NULL;
    }

    device->VideoInit = PS5_VideoInit;
    device->VideoQuit = PS5_VideoQuit;
    device->PumpEvents = PS5_PumpEvents;
    device->CreateWindowFramebuffer = PS5_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = PS5_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = PS5_DestroyWindowFramebuffer;
    device->free = PS5_DestroyDevice;

    return device;
}

VideoBootStrap PS5_bootstrap = { "PS5", "Sony PS5 Video Driver", PS5_CreateDevice };

#endif /* SDL_VIDEO_DRIVER_PS5 */
