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
#include <stdbool.h>
#include <stdint.h>

#include "SDL_error.h"
#include "SDL_events.h"

#include "../../events/SDL_keyboard_c.h"

#include "SDL_ps5keyboard.h"

typedef struct keyboard_state
{
    uint64_t junk0[2];
    uint8_t available;
    uint32_t junk1[2];
    uint32_t modifiers;
    uint16_t scankey[16];
    uint64_t junk3[4];
} keyboard_state_t;

int sceKeyboardInit(void);
int sceKeyboardOpen(int, int, int, void *);
int sceKeyboardReadState(int, keyboard_state_t *);
int sceKeyboardClose(int);

int sceUserServiceInitialize(void *);
int sceUserServiceGetForegroundUser(int *);

static int g_handle;
static keyboard_state_t g_prev;

int PS5_Keyboard_Open(void)
{
    int user_id;
    long junk;
    int err;

    err = sceUserServiceGetForegroundUser(&user_id);
    if (err != 0) {
        return SDL_SetError("sceUserServiceGetForegroundUser: 0x%08x", err);
    }

    g_handle = sceKeyboardOpen(user_id, 0, 0, &junk);
    if (g_handle <= 0) {
        return SDL_SetError("sceKeyboardOpen: 0x%08x", g_handle);
    }

    return 0;
}

int PS5_Keyboard_Close(void)
{
    int err;

    if (g_handle <= 0) {
        return 0;
    }

    err = sceKeyboardClose(g_handle);
    if (err != 0) {
        return SDL_SetError("sceKeyboardClose: 0x%08x", err);
    }

    return 0;
}

int PS5_Keyboard_Init(void)
{
    int err;

    err = sceUserServiceInitialize(0);
    if (err != 0 && err != 0x80960003) {
        return SDL_SetError("sceUserServiceInitialize: 0x%08x", err);
    }

    err = sceKeyboardInit();
    if (err != 0) {
        return SDL_SetError("scePadInit: 0x%08x", err);
    }

    return 0;
}

int PS5_Keyboard_PumpEvents(void)
{
    keyboard_state_t curr;
    uint32_t diff;
    int err;

    if (g_handle <= 0) {
        return 0;
    }

    err = sceKeyboardReadState(g_handle, &curr);
    if (err != 0) {
        return SDL_SetError("sceKeyboardReadState: 0x%08x", err);
    }

    if (!curr.available) {
        return 0;
    }

    diff = g_prev.modifiers ^ curr.modifiers;
    if (diff) {
        if (diff & 0x01) {
            if (g_prev.modifiers & 0x01) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LCTRL);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LCTRL);
            }
        }
        if (diff & 0x02) {
            if (g_prev.modifiers & 0x02) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LSHIFT);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LSHIFT);
            }
        }
        if (diff & 0x04) {
            if (g_prev.modifiers & 0x04) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LALT);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LALT);
            }
        }
        if (diff & 0x08) {
            if (g_prev.modifiers & 0x08) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LGUI);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LGUI);
            }
        }
        if (diff & 0x10) {
            if (g_prev.modifiers & 0x10) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RCTRL);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RCTRL);
            }
        }
        if (diff & 0x20) {
            if (g_prev.modifiers & 0x20) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RSHIFT);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RSHIFT);
            }
        }
        if (diff & 0x40) {
            if (g_prev.modifiers & 0x40) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RALT);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RALT);
            }
        }
        if (diff & 0x80) {
            if (g_prev.modifiers & 0x80) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RGUI);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RGUI);
            }
        }
    }

    for (int i = 0; i < 16; i++) {
        if (g_prev.scankey[i] == curr.scankey[i]) {
            continue;
        }

        if (g_prev.scankey[i] == 0) {
            SDL_SendKeyboardKey(SDL_PRESSED, curr.scankey[i]);
        } else {
            SDL_SendKeyboardKey(SDL_RELEASED, g_prev.scankey[i]);
        }
    }

    memcpy(&g_prev, &curr, sizeof(curr));

    return 0;
}

#endif /* SDL_VIDEO_DRIVER_PS5 */

/* vi: set ts=4 sw=4 expandtab: */
