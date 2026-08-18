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

#ifdef SDL_VIDEO_DRIVER_PS5

#include <errno.h>

#include "SDL_ps5video.h"
#include "SDL_ps5keyboard.h"
#include "SDL_ps5osmesa.h"

static void PS5_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface;

    surface = window->surface;
    SDL_FreeSurface(surface);
}

static int PS5_CreateWindowFramebuffer(_THIS, SDL_Window *window,
                                       Uint32 *format, void **pixels,
                                       int *pitch)
{
    const Uint32 surface_format = SDL_PIXELFORMAT_ABGR8888;
    SDL_Surface *surface;
    int w, h;

    /* Free the old framebuffer surface */
    // PS5_DestroyWindowFramebuffer(window);
    SDL_assert(window->surface == NULL);

    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, surface_format);
    if (!surface) {
        return -1;
    }

    /* Save the info and return! */
    window->surface = surface;
    // *format = surface_format;
    // *pixels = surface->pixels;
    // *pitch = surface->pitch;
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

    surface = window->surface;
    if (!surface) {
        return SDL_SetError("Couldn't find surface for window");
    }

    if(surface->w == device_data->surface->w &&
       surface->h == device_data->surface->h) {
        PS5_Tilemap_Blit(device_data->tmap, surface->pixels, surface->pitch / 4,
                         device_data->vbuf[idx].data,
                         idx, rects, numrects, 0, 0);
    } else {
        SDL_BlitSurface(surface, NULL, device_data->surface,
                        &(SDL_Rect){(device_data->surface->w - surface->w) / 2,
                                    (device_data->surface->h - surface->h) / 2,
                                    surface->w, surface->h});
        PS5_Tilemap_Blit(device_data->tmap,
                         device_data->surface->pixels,
                         device_data->surface->pitch / 4,
                         device_data->vbuf[idx].data,
                         idx, rects, numrects,
                         (device_data->surface->w - surface->w) / 2,
                         (device_data->surface->h - surface->h) / 2);
    }

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
    //SDL_AddDisplayMode(display, &mode);
}

static int PS5_SetDisplayMode(_THIS, SDL_VideoDisplay * display,
                              SDL_DisplayMode * mode)
{
    PS5_DeviceData *device_data = (PS5_DeviceData *)_this->driverdata;
    PS5_VideoAttr vattr = {0};
    SDL_Surface *surface;
    PS5_Tilemap *tmap;

    if(PS5_Tilemap_BufferSize(mode->w, mode->h) > device_data->memsize / 2) {
        return SDL_SetError("%dx%d does not fit in the video buffers",
                            mode->w, mode->h);
    }

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

    surface = SDL_CreateRGBSurfaceWithFormat(0, mode->w, mode->h, 32,
                                             mode->format);
    if(!surface) {
        return -1;
    }

    tmap = PS5_Tilemap_Create(mode->w, mode->h);
    if(!tmap) {
        SDL_FreeSurface(surface);
        return SDL_OutOfMemory();
    }

    SDL_FreeSurface(device_data->surface);
    PS5_Tilemap_Destroy(device_data->tmap);
    device_data->surface = surface;
    device_data->tmap = tmap;

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
    device_data->memsize = 0x4000000;
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

    device_data->surface = SDL_CreateRGBSurfaceWithFormat(0, mode.w, mode.h, 32,
                                                          mode.format);
    if (!device_data->surface) {
        return -1;
    }

    SDL_zero(display);
    display.desktop_mode = mode;
    display.current_mode = mode;

    device_data->tmap = PS5_Tilemap_Create(mode.w, mode.h);
    if(!device_data->tmap) {
        return SDL_OutOfMemory();
    }

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

    PS5_Tilemap_Destroy(device_data->tmap);
    device_data->tmap = NULL;
}

static int PS5_CreateWindow(_THIS, SDL_Window *window)
{
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

#ifdef SDL_VIDEO_OPENGL_OSMESA
    PS5_OSMesa_InitDevice(device);
#endif

    return device;
}

VideoBootStrap PS5_bootstrap = { "ps5", "Sony PS5 Video Driver",
                                 PS5_CreateDevice };

#endif /* SDL_VIDEO_DRIVER_PS5 */

/* vi: set ts=4 sw=4 expandtab: */

/* emacs: */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* indent-tabs-mode: nil */
/* End: */
