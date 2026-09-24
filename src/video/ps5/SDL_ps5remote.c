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

#ifdef SDL_VIDEO_DRIVER_PS5

#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_stdinc.h"

#include "../../events/SDL_keyboard_c.h"
#include "../../joystick/ps5/SDL_ps5joystick.h"

#include "SDL_ps5remote.h"


static const struct
{
    Uint32 button;
    SDL_Scancode scancode;
} remote_btn_map[] = {
    { PS5_PAD_BUTTON_UP,      SDL_SCANCODE_UP },
    { PS5_PAD_BUTTON_DOWN,    SDL_SCANCODE_DOWN },
    { PS5_PAD_BUTTON_LEFT,    SDL_SCANCODE_LEFT },
    { PS5_PAD_BUTTON_RIGHT,   SDL_SCANCODE_RIGHT },
    { PS5_PAD_BUTTON_CROSS,   SDL_SCANCODE_RETURN },  // typically labeled "ok"
    { PS5_PAD_BUTTON_CIRCLE,  SDL_SCANCODE_ESCAPE },  // typically labeled "back"
    { PS5_PAD_BUTTON_OPTIONS, SDL_SCANCODE_MENU },
};

/* All other remote keys are reported as a CEC key code in the pad state. */
static const SDL_Scancode remote_key_map[] = {
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
    SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8,
    SDL_SCANCODE_9, SDL_SCANCODE_0,
    SDL_SCANCODE_MINUS,             // labeled as "11", on some Japanese RCs
    SDL_SCANCODE_EQUALS,            // labeled as "12", on some Japanese RCs
    SDL_SCANCODE_RETURN,
    SDL_SCANCODE_UNKNOWN,           // unknown (14)
    SDL_SCANCODE_ESCAPE,            // typically labeled "back"
    SDL_SCANCODE_UNKNOWN,           // unknown (16)
    SDL_SCANCODE_UNKNOWN,           // unknown (17)
    SDL_SCANCODE_MENU,
    SDL_SCANCODE_UNKNOWN,           // unknown (19)
    SDL_SCANCODE_AUDIOPREV,
    SDL_SCANCODE_AUDIONEXT,
    SDL_SCANCODE_AUDIOPLAY,
    SDL_SCANCODE_AUDIOREWIND,
    SDL_SCANCODE_AUDIOFASTFORWARD,
    SDL_SCANCODE_AUDIOSTOP,
    SDL_SCANCODE_PAUSE,             // typically labeled "pause"
    SDL_SCANCODE_APPLICATION,       // context menu
    SDL_SCANCODE_UNKNOWN,           // unknown (28)
    SDL_SCANCODE_UNKNOWN,           // unknown (29)
    SDL_SCANCODE_F1,                // typically labeled "subtitle"
    SDL_SCANCODE_F2,                // typically labeled "audio"
    SDL_SCANCODE_F3,                // typically labeled "camera"
    SDL_SCANCODE_F4,                // typically labeled "display"
    SDL_SCANCODE_UNKNOWN,           // unknown (34)
    SDL_SCANCODE_UNKNOWN,           // unknown (35)
    SDL_SCANCODE_F8,                // blue
    SDL_SCANCODE_F5,                // red
    SDL_SCANCODE_F6,                // green
    SDL_SCANCODE_F7,                // yellow
    SDL_SCANCODE_PERIOD,
    SDL_SCANCODE_PAGEUP,            // typically labeled "p+"
    SDL_SCANCODE_PAGEDOWN,          // typically labeled "p-"
    SDL_SCANCODE_BACKSPACE,         // typically labeled "prev"
    SDL_SCANCODE_F9,                // typically labeled "guide"
    SDL_SCANCODE_SPACE,             // typically labeled "play/pause"
};

/* Index into PS5_PadData.unknown holding the CEC key code. */
#define PS5_REMOTE_KEY_OFFSET 3

static int g_remote_handle = -1;

static Uint8 g_remote_keys[SDL_NUM_SCANCODES];

static void PS5_Remote_SetKeys(const Uint8 *keys)
{
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        if (keys[i] == g_remote_keys[i]) {
            continue;
        }
        SDL_SendKeyboardKey(keys[i] ? SDL_PRESSED : SDL_RELEASED,
                            (SDL_Scancode)i);
        g_remote_keys[i] = keys[i];
    }
}

int PS5_Remote_Init(void)
{
    int handle;
    int err;

    SDL_zeroa(g_remote_keys);

    err = sceUserServiceInitialize(0);
    if (err != 0 && err != 0x80960003) {
        return SDL_SetError("sceUserServiceInitialize: 0x%08x", err);
    }

    err = scePadInit();
    if (err != 0) {
        return SDL_SetError("scePadInit: 0x%08x", err);
    }

    handle = scePadOpen(PS5_USER_ID_SYSTEM, PS5_PAD_PORT_TYPE_REMOTE_CONTROL,
                        0, NULL);
    if ((unsigned)handle == PS5_PAD_ERROR_ALREADY_OPENED) {
        handle = scePadGetHandle(PS5_USER_ID_SYSTEM,
                                 PS5_PAD_PORT_TYPE_REMOTE_CONTROL, 0);
    }
    if (handle <= 0) {
        return SDL_SetError("scePadOpen: 0x%08x", handle);
    }

    g_remote_handle = handle;

    return 0;
}

void PS5_Remote_PumpEvents(void)
{
    Uint8 keys[SDL_NUM_SCANCODES];
    PS5_PadData pad = { 0 };
    Uint8 code;

    if (g_remote_handle <= 0) {
        return;
    }

    SDL_zeroa(keys);

    // On read failure or disconnect, release anything still held down
    if (scePadReadState(g_remote_handle, &pad) == 0 && pad.connected) {
        for (int i = 0; i < SDL_arraysize(remote_btn_map); i++) {
            if (pad.buttons & remote_btn_map[i].button) {
                keys[remote_btn_map[i].scancode] = 1;
            }
        }

        code = pad.unknown[PS5_REMOTE_KEY_OFFSET];
        if (code < SDL_arraysize(remote_key_map) &&
            remote_key_map[code] != SDL_SCANCODE_UNKNOWN) {
            keys[remote_key_map[code]] = 1;
        }
    }

    PS5_Remote_SetKeys(keys);
}

void PS5_Remote_Quit(void)
{
    Uint8 keys[SDL_NUM_SCANCODES];

    SDL_zeroa(keys);
    PS5_Remote_SetKeys(keys);

    if (g_remote_handle > 0) {
        scePadClose(g_remote_handle);
        g_remote_handle = -1;
    }
}

#endif /* SDL_VIDEO_DRIVER_PS5 */

/* vi: set ts=4 sw=4 expandtab: */
