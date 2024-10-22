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

#include <errno.h>
#include <pthread.h>

#include "SDL_ps5tilemap.inc"
#include "SDL_ps5video.h"
#include "SDL_ps5keyboard.h"

#define PS5_SURFACE "_PS5_Surface"

#define PS5_THREAD_COUNT 12


static void* PS5_DrawTileThread(void* arg) {
    const PS5_DrawChunk* chunk = (PS5_DrawChunk*)arg;

    for (int ind = chunk->src_start; ind < chunk->src_end; ind++) {
        int x = ind % chunk->frame_width;
        int y = ind / chunk->frame_width;
        int ty = y / PS5_TILE_HEIGHT;
        int tx = x / PS5_TILE_WIDTH;

        int t = (int)(PS5_TILE_SIZE * (tx + ty * ((double)chunk->frame_width /
                                                  PS5_TILE_WIDTH)));
        int i = PS5_tilemap[y % PS5_TILE_HEIGHT][x % PS5_TILE_WIDTH];
        chunk->dst[t + i] = chunk->src[ind];
    }
    return 0;
}

static void PS5_DrawPixelsAsTiles(uint32_t *src, uint32_t *dst,
                                  int frame_width, int frame_height)
{
    int chunk_size = frame_width * frame_height / PS5_THREAD_COUNT;
    PS5_DrawChunk chunks[PS5_THREAD_COUNT];
    pthread_t threads[PS5_THREAD_COUNT];

    for (int i=0; i<PS5_THREAD_COUNT; i++) {
        chunks[i].src = src;
        chunks[i].dst = dst;
        chunks[i].src_start = i * chunk_size;
        chunks[i].src_end = (i + 1) * chunk_size;
        chunks[i].frame_width = frame_width;
        chunks[i].frame_height = frame_height;

        if(i == PS5_THREAD_COUNT - 1) {
            chunks[i].src_end = frame_width * frame_height;
        }

        pthread_create(&threads[i], 0, &PS5_DrawTileThread, &chunks[i]);
    }

    for (int i=0; i<PS5_THREAD_COUNT; i++) {
        pthread_join(threads[i], 0);
    }
}

static void PS5_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface;

    surface = (SDL_Surface *)SDL_SetWindowData(window, PS5_SURFACE, NULL);
    SDL_FreeSurface(surface);
}

static int PS5_CreateWindowFramebuffer(_THIS, SDL_Window *window,
                                       Uint32 *format, void **pixels,
                                       int *pitch)
{
    const Uint32 surface_format = SDL_PIXELFORMAT_ABGR8888;
    SDL_Surface *surface;
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

static int PS5_UpdateWindowFramebuffer(_THIS, SDL_Window *window,
                                       const SDL_Rect *rects, int numrects)
{
    PS5_DeviceData *device_data = (PS5_DeviceData *)_this->driverdata;
    static uint32_t frame_id = 0;
    uint8_t idx = frame_id % 2;
    SDL_Surface *surface;
    struct kevent evt;
    int junk;

    surface = (SDL_Surface *)SDL_GetWindowData(window, PS5_SURFACE);
    if (!surface) {
        return SDL_SetError("Couldn't find surface for window");
    }

    PS5_DrawPixelsAsTiles(surface->pixels, device_data->vbuf[idx].data,
                          surface->w, surface->h);

    if (sceVideoOutSubmitFlip(device_data->handle, idx, 1, frame_id)) {
        return SDL_SetError("sceVideoOutSubmitFlip: %s", strerror(errno));
    }

    if (sceKernelWaitEqueue(device_data->evt_queue, &evt, 1, &junk, 0)) {
        return SDL_SetError("sceKernelWaitEqueue: %s", strerror(errno));
    }
    frame_id++;

    return 0;
}

static void PS5_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    SDL_DisplayMode mode;

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.w = 3840;
    mode.h = 2160;
    mode.refresh_rate = 60;

    SDL_AddDisplayMode(display, &display->current_mode);
    SDL_AddDisplayMode(display, &mode);
}

static int PS5_SetDisplayMode(_THIS, SDL_VideoDisplay * display,
                              SDL_DisplayMode * mode)
{
    PS5_DeviceData *device_data = (PS5_DeviceData *)_this->driverdata;
    PS5_VideoAttr vattr = {0};

    if(device_data->evt_queue) {
        sceVideoOutDeleteFlipEvent(device_data->evt_queue, device_data->handle);
        sceKernelDeleteEqueue(device_data->evt_queue);
    }

    if(device_data->handle >= 0) {
        sceVideoOutClose(device_data->handle);
    }
    device_data->handle = sceVideoOutOpen(0xff, 0, 0, NULL);

    if (sceKernelCreateEqueue(&device_data->evt_queue, "flip queue")) {
        return SDL_SetError("sceKernelCreateEqueue: %s", strerror(errno));
    }
    if (sceVideoOutAddFlipEvent(device_data->evt_queue, device_data->handle, 0)) {
        return SDL_SetError("sceVideoOutAddFlipEvent: %s", strerror(errno));
    }
    if (sceVideoOutSetFlipRate(device_data->handle, 0)) {
        return SDL_SetError("sceVideoOutSetFlipRate: %s", strerror(errno));
    }

    sceVideoOutSetBufferAttribute2(&vattr, 0x8000000022000000UL, 0,
                                   mode->w, mode->h, 0, 0, 0);

    if (sceVideoOutRegisterBuffers2(device_data->handle, 0, 0,
                                    device_data->vbuf, 2, &vattr, 0, NULL)) {
        return SDL_SetError("sceVideoOutRegisterBuffers2: %s", strerror(errno));
    }

    return 0;
}

static int PS5_VideoInit(_THIS)
{
    PS5_DeviceData *device_data = (PS5_DeviceData *)_this->driverdata;
    SDL_VideoDisplay display;
    SDL_DisplayMode mode;
    PS5_VideoAttr vattr;
    void *vaddr = 0;

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.w = 1920;
    mode.h = 1080;
    mode.refresh_rate = 60;

    memset(device_data->vbuf, 0, sizeof(device_data->vbuf));
    memset(&vattr, 0, sizeof(vattr));

    sceSystemServiceHideSplashScreen();
    device_data->handle = sceVideoOutOpen(0xff, 0, 0, NULL);
    if (device_data->handle < 0) {
        return SDL_SetError("sceVideoOutOpen: %s", strerror(errno));
    }
    device_data->memsize = 0x20000000;
    if (sceKernelAllocateMainDirectMemory(device_data->memsize, 0x20000, 3,
                                          &device_data->paddr)) {
        return SDL_SetError("sceKernelAllocateMainDirectMemory: %s",
                            strerror(errno));
    }

    if (sceKernelMapDirectMemory(&vaddr, device_data->memsize, 0x33, 0,
                                 device_data->paddr, 0x20000)) {
        return SDL_SetError("sceKernelMapDirectMemory: %s", strerror(errno));
    }

    device_data->vbuf[0].data = vaddr;
    device_data->vbuf[1].data = vaddr + (device_data->memsize / 2);

    if (sceKernelCreateEqueue(&device_data->evt_queue, "flip queue")) {
        return SDL_SetError("sceKernelCreateEqueue: %s", strerror(errno));
    }

    if (sceVideoOutAddFlipEvent(device_data->evt_queue, device_data->handle, 0)) {
        return SDL_SetError("sceVideoOutAddFlipEvent: %s", strerror(errno));
    }
    if (sceVideoOutSetFlipRate(device_data->handle, 0)) {
        return SDL_SetError("sceVideoOutSetFlipRate: %s", strerror(errno));
    }

    sceVideoOutSetBufferAttribute2(&vattr, 0x8000000022000000UL, 0,
                                   mode.w, mode.h, 0, 0, 0);

    if (sceVideoOutRegisterBuffers2(device_data->handle, 0, 0,
                                    device_data->vbuf, 2, &vattr, 0, NULL)) {
        return SDL_SetError("sceVideoOutRegisterBuffers2: %s", strerror(errno));
    }

    SDL_zero(display);
    display.desktop_mode = mode;
    display.current_mode = mode;

    SDL_AddVideoDisplay(&display, SDL_FALSE);

    return 0;
}

static void PS5_VideoQuit(_THIS)
{
    PS5_DeviceData *device_data = (PS5_DeviceData *)_this->driverdata;

    if (device_data->handle != 0) {
        sceVideoOutClose(device_data->handle);
        device_data->handle = 0;
    }

    if (device_data->paddr) {
        sceKernelReleaseDirectMemory(device_data->paddr, device_data->memsize);
        device_data->paddr = 0;
        device_data->memsize = 0;
    }
    if (device_data->evt_queue) {
        sceKernelDeleteEqueue(device_data->evt_queue);
    }
}

static int PS5_CreateWindow(_THIS, SDL_Window *window)
{
    if (window) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    }

    return 0;
}

static void PS5_DestroyDevice(SDL_VideoDevice *device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

static void PS5_DestroyWindow(_THIS, SDL_Window *window)
{
}

static void PS5_PumpEvents(_THIS)
{
    PS5_Keyboard_PumpEvents();
}

static SDL_VideoDevice *PS5_CreateDevice(void)
{
    SDL_VideoDevice *device;

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
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

    PS5_Keyboard_Init();
    PS5_Keyboard_Open();

    device->VideoInit = PS5_VideoInit;
    device->VideoQuit = PS5_VideoQuit;
    device->GetDisplayModes = PS5_GetDisplayModes;
    device->SetDisplayMode = PS5_SetDisplayMode;
    device->PumpEvents = PS5_PumpEvents;
    device->CreateSDLWindow = PS5_CreateWindow;
    device->DestroyWindow = PS5_DestroyWindow;
    device->CreateWindowFramebuffer = PS5_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = PS5_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = PS5_DestroyWindowFramebuffer;
    device->HasScreenKeyboardSupport = PS5_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = PS5_ShowScreenKeyboard;
    device->HideScreenKeyboard = PS5_HideScreenKeyboard;
    device->IsScreenKeyboardShown = PS5_IsScreenKeyboardShown;
    device->free = PS5_DestroyDevice;

    return device;
}

VideoBootStrap PS5_bootstrap = { "PS5", "Sony PS5 Video Driver",
                                 PS5_CreateDevice };

#endif /* SDL_VIDEO_DRIVER_PS5 */

/* vi: set ts=4 sw=4 expandtab: */

/* emacs: */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* indent-tabs-mode: nil */
/* End: */
