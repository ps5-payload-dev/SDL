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

#include "SDL_ps5osmesa.h"

#ifdef SDL_VIDEO_OPENGL_OSMESA

#include <SDL2/SDL_opengl.h>
#include <dlfcn.h>

#ifndef OSMESA_Y_UP
#define OSMESA_Y_UP 0x11
#endif

static void* osmesa_lib = NULL;
static void* osmesa_context = NULL;

static void* (*OSMesaGetProcAddress)(const char*) = 0;
static void* (*OSMesaCreateContext)(GLenum, void*) = 0;
static void(*OSMesaPixelStore)(GLint, GLint) = 0;
static void (*OSMesaDestroyContext)(void*) = 0;
static GLboolean (*OSMesaMakeCurrent)(void* , void*, GLenum, GLsizei, GLsizei) = 0;
static GLboolean (*OSMesaGetColorBuffer)(void*, GLint*, GLint*, GLint*, void**) = 0;

static void (*OSMesaFlush)(void) = 0;

static int OSMesa_LoadLibrary(_THIS, const char *path)
{
    osmesa_lib = dlopen("libOSMesa.so.8", RTLD_LAZY);
    if (!osmesa_lib) {
        return SDL_SetError("%s", dlerror());
    }

    OSMesaGetProcAddress = dlsym(osmesa_lib, "OSMesaGetProcAddress");
    if (!OSMesaGetProcAddress) {
        return SDL_SetError("%s", dlerror());
    }

    OSMesaCreateContext = dlsym(osmesa_lib, "OSMesaCreateContext");
    if (!OSMesaCreateContext) {
        return SDL_SetError("%s", dlerror());
    }

    OSMesaDestroyContext = dlsym(osmesa_lib, "OSMesaDestroyContext");
    if (!OSMesaDestroyContext) {
        return SDL_SetError("%s", dlerror());
    }

    OSMesaGetColorBuffer = dlsym(osmesa_lib, "OSMesaGetColorBuffer");
    if (!OSMesaGetColorBuffer) {
        return SDL_SetError("%s", dlerror());
    }

    OSMesaMakeCurrent = dlsym(osmesa_lib, "OSMesaMakeCurrent");
    if (!OSMesaMakeCurrent) {
        return SDL_SetError("%s", dlerror());
    }

    OSMesaPixelStore = dlsym(osmesa_lib, "OSMesaPixelStore");
    if (!OSMesaPixelStore) {
        return SDL_SetError("%s", dlerror());
    }

    OSMesaFlush = OSMesaGetProcAddress("glFlush");
    if (!OSMesaFlush) {
        return SDL_SetError("%s", dlerror());
    }

    return 0;
}

static void OSMesa_UnloadLibrary(_THIS)
{
    if (osmesa_lib) {
        dlclose(osmesa_lib);
    }

    osmesa_lib = 0;
    OSMesaGetProcAddress = 0;
    OSMesaCreateContext = 0;
    OSMesaDestroyContext = 0;
    OSMesaGetColorBuffer = 0;
    OSMesaMakeCurrent = 0;
    OSMesaPixelStore = 0;
}

static void* OSMesa_GetProcAddress(_THIS, const char *proc)
{
    return OSMesaGetProcAddress(proc);
}

static int OSMesa_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
    SDL_Surface* surface = SDL_GetWindowSurface(window);

    if (!surface) {
        return SDL_SetError("Couldn't find surface for window");
    }
    if (!OSMesaMakeCurrent(context, surface->pixels, GL_UNSIGNED_BYTE, surface->w, surface->h)) {
        return SDL_SetError("Failed to make context current");
    }

    OSMesaPixelStore(OSMESA_Y_UP, 0);

    osmesa_context = context;

    return 0;
}

static SDL_GLContext OSMesa_CreateContext(_THIS, SDL_Window * window)
{
    void* ctx;

    ctx = OSMesaCreateContext(GL_RGBA, NULL);
    if (!ctx) {
        SDL_SetError("Failed to create context");
        return NULL;
    }

    if (OSMesa_MakeCurrent(_this, window, ctx)) {
        return NULL;
    }

    return (SDL_GLContext)ctx;
}

static int OSMesa_SetSwapInterval(_THIS, int interval)
{
    return 0;
}

static int OSMesa_GetSwapInterval(_THIS)
{
    return 0;
}

static int OSMesa_SwapWindow(_THIS, SDL_Window *window)
{
    SDL_Surface *surface;

    OSMesaFlush();

    surface = SDL_GetWindowSurface(window);
    if (!surface) {
        return SDL_SetError("Failed to get SDL window surface");
    }

    if (SDL_UpdateWindowSurface(window) != 0) {
        return SDL_SetError("Failed to update window surface");
    }

    return 0;
}

static void OSMesa_DeleteContext(_THIS, SDL_GLContext context)
{
    if (context) {
        OSMesaDestroyContext(context);
    }
}

int PS5_OSMesa_InitDevice(SDL_VideoDevice* device)
{
    device->GL_LoadLibrary = OSMesa_LoadLibrary;
    device->GL_GetProcAddress = OSMesa_GetProcAddress;
    device->GL_UnloadLibrary = OSMesa_UnloadLibrary;
    device->GL_CreateContext = OSMesa_CreateContext;
    device->GL_MakeCurrent = OSMesa_MakeCurrent;
    device->GL_SetSwapInterval = OSMesa_SetSwapInterval;
    device->GL_GetSwapInterval = OSMesa_GetSwapInterval;
    device->GL_SwapWindow = OSMesa_SwapWindow;
    device->GL_DeleteContext = OSMesa_DeleteContext;

    return 0;
}

#endif
