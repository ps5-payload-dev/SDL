/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_JOYSTICK_PS5

#include <errno.h>

#include "SDL_error.h"
#include "SDL_events.h"

#include "../SDL_sysjoystick.h"
#include "../../events/SDL_keyboard_c.h"


#include "SDL_ps5joystick.h"

#define PS5_MAX_USERS 4

#define PS5_REMOTE_INDEX PS5_MAX_USERS
#define PS5_MAX_DEVICES  (PS5_MAX_USERS + 1)

#define PS5_PAD_AXIS_LX 0
#define PS5_PAD_AXIS_LY 1
#define PS5_PAD_AXIS_RX 2
#define PS5_PAD_AXIS_RY 3
#define PS5_PAD_AXIS_L2 4
#define PS5_PAD_AXIS_R2 5

#define PS5_PAD_GUID    "0300d0424c050000e60c000011810000"
#define PS5_REMOTE_GUID "030000004c050000cec0000011810000"

typedef struct PS5_PadContext
{
    int user_id;
    char name[255];
    int handle;
    SDL_JoystickGUID global_id;
    SDL_JoystickID instance_id;
    PS5_PadData pad;
} PS5_PadContext;

static PS5_PadContext pad_ctx[PS5_MAX_DEVICES];
static SDL_JoystickID instance_counter = 0;

#define PS5_IS_REMOTE(ctx) ((ctx) == &pad_ctx[PS5_REMOTE_INDEX])

// Map analog inputs from [0, 255] to [-32768, 32767]
static int analog_map[256] = {
    -32767, -32759, -32742, -32712, -32671, -32618, -32553, -32478, -32392,
    -32296, -32188, -32071, -31944, -31806, -31659, -31502, -31336, -31161,
    -30978, -30785, -30584, -30374, -30157, -29931, -29697, -29457, -29208,
    -28952, -28690, -28421, -28144, -27862, -27573, -27278, -26978, -26672,
    -26359, -26043, -25721, -25393, -25062, -24725, -24385, -24040, -23691,
    -23339, -22983, -22624, -22262, -21896, -21528, -21157, -20784, -20409,
    -20031, -19652, -19271, -18889, -18505, -18121, -17736, -17350, -16963,
    -16576, -16189, -15802, -15415, -15029, -14643, -14259, -13876, -13493,
    -13113, -12733, -12356, -11981, -11607, -11236, -10868, -10503, -10140,
    -9781, -9425, -9073, -8725, -8379, -8039, -7703, -7371, -7044, -6722,
    -6405, -6093, -5787, -5486, -5192, -4903, -4620, -4344, -4075, -3812,
    -3557, -3308, -3068, -2834, -2608, -2390, -2181, -1980, -1787, -1604,
    -1429, -1263, -1106, -959, -821, -694, -577, -469, -373, -287, -212,
    -147, -94, -53, -23, -6, 0, 0, 6, 23, 53, 94, 147, 212, 287, 373, 469,
    577, 694, 821, 959, 1106, 1263, 1429, 1604, 1787, 1980, 2181, 2390, 2608,
    2834, 3068, 3308, 3557, 3812, 4075, 4344, 4620, 4903, 5192, 5486, 5787,
    6093, 6405, 6722, 7044, 7371, 7703, 8039, 8379, 8725, 9073, 9425, 9781,
    10140, 10503, 10868, 11236, 11607, 11981, 12356, 12733, 13113, 13493,
    13876, 14259, 14643, 15029, 15415, 15802, 16189, 16576, 16963, 17350,
    17736, 18121, 18505, 18889, 19271, 19652, 20031, 20409, 20784, 21157,
    21528, 21896, 22262, 22624, 22983, 23339, 23691, 24040, 24385, 24725,
    25062, 25393, 25721, 26043, 26359, 26672, 26978, 27278, 27573, 27862,
    28144, 28421, 28690, 28952, 29208, 29457, 29697, 29931, 30157, 30374,
    30584, 30785, 30978, 31161, 31336, 31502, 31659, 31806, 31944, 32071,
    32188, 32296, 32392, 32478, 32553, 32618, 32671, 32712, 32742, 32759,
    32767
};

static const unsigned int btn_map[] = {
    PS5_PAD_BUTTON_CROSS,     // a:b0
    PS5_PAD_BUTTON_CIRCLE,    // b:b1
    PS5_PAD_BUTTON_SQUARE,    // x:b2
    PS5_PAD_BUTTON_TRIANGLE,  // y:b3
    PS5_PAD_BUTTON_TOUCH_PAD, // touchpad:b4
    -1,                       // back:b5
    PS5_PAD_BUTTON_OPTIONS,   // start:b6
    PS5_PAD_BUTTON_L3,        // leftstick:b7
    PS5_PAD_BUTTON_R3,        // rightstick:b8
    PS5_PAD_BUTTON_L1,        // leftshoulder:b9
    PS5_PAD_BUTTON_R1,        // rightshoulder:b10
    PS5_PAD_BUTTON_UP,        // dpup:b11
    PS5_PAD_BUTTON_DOWN,      // dpdown:b12
    PS5_PAD_BUTTON_LEFT,      // dpleft:b13
    PS5_PAD_BUTTON_RIGHT,     // dpright:b14
    PS5_PAD_BUTTON_L2,        // lefttrigger:b15
    PS5_PAD_BUTTON_R2         // righttrigger:b16
};


static const SDL_Scancode remote_map[] = {
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


static PS5_PadContext *PS5_JoystickGetDevice(int device_index)
{
    int n = 0;

    if (device_index < 0) {
        return NULL;
    }

    for (int i = 0; i < SDL_arraysize(pad_ctx); i++) {
        if (pad_ctx[i].instance_id == -1) {
            continue;
        }
        if (n == device_index) {
            return &pad_ctx[i];
        }
        n++;
    }

    return NULL;
}

static SDL_JoystickID PS5_JoystickGetDeviceInstanceID(int device_index)
{
    PS5_PadContext *ctx = PS5_JoystickGetDevice(device_index);

    return ctx ? ctx->instance_id : -1;
}

static PS5_PadContext* PS5_JoystickGetPadContext(SDL_Joystick *joystick) {
    SDL_JoystickID instance_id = SDL_JoystickInstanceID(joystick);

    for (int i = 0; i < SDL_arraysize(pad_ctx); i++) {
        if (instance_id == pad_ctx[i].instance_id) {
            return &pad_ctx[i];
        }
    }

    return 0;
}

static const char *PS5_JoystickGetDeviceName(int device_index)
{
    PS5_PadContext *ctx = PS5_JoystickGetDevice(device_index);

    return ctx ? ctx->name : NULL;
}

static SDL_bool PS5_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    PS5_PadContext *ctx = PS5_JoystickGetDevice(device_index);

    if (!ctx) {
        return SDL_FALSE;
    }

    out->a.kind = EMappingKind_Button;
    out->a.target = 0;
    out->b.kind = EMappingKind_Button;
    out->b.target = 1;
    out->x.kind = EMappingKind_Button;
    out->x.target = 2;
    out->y.kind = EMappingKind_Button;
    out->y.target = 3;
    out->touchpad.kind = EMappingKind_Button;
    out->touchpad.target = 4;
    // out->back.kind = EMappingKind_Button;
    // out->back.target = 5;
    out->start.kind = EMappingKind_Button;
    out->start.target = 6;
    out->leftstick.kind = EMappingKind_Button;
    out->leftstick.target = 7;
    out->rightstick.kind = EMappingKind_Button;
    out->rightstick.target = 8;
    out->leftshoulder.kind = EMappingKind_Button;
    out->leftshoulder.target = 9;
    out->rightshoulder.kind = EMappingKind_Button;
    out->rightshoulder.target = 10;
    out->dpup.kind = EMappingKind_Button;
    out->dpup.target = 11;
    out->dpdown.kind = EMappingKind_Button;
    out->dpdown.target = 12;
    out->dpleft.kind = EMappingKind_Button;
    out->dpleft.target = 13;
    out->dpright.kind = EMappingKind_Button;
    out->dpright.target = 14;
    out->leftx.kind = EMappingKind_Axis;
    out->leftx.target = PS5_PAD_AXIS_LX;
    out->lefty.kind = EMappingKind_Axis;
    out->lefty.target = PS5_PAD_AXIS_LY;
    out->rightx.kind = EMappingKind_Axis;
    out->rightx.target = PS5_PAD_AXIS_RX;
    out->righty.kind = EMappingKind_Axis;
    out->righty.target = PS5_PAD_AXIS_RY;
    out->lefttrigger.kind = EMappingKind_Axis;
    out->lefttrigger.target = PS5_PAD_AXIS_L2;
    out->righttrigger.kind = EMappingKind_Axis;
    out->righttrigger.target = PS5_PAD_AXIS_R2;

    return SDL_TRUE;
}


static void PS5_JoystickUpdateRemote(Uint8 prev, Uint8 curr) {
    if (prev > 45 || curr > 45) {
        return;
    }

    if (!prev && curr && remote_map[curr] != SDL_SCANCODE_UNKNOWN) {
        SDL_SendKeyboardKey(SDL_PRESSED, remote_map[curr]);
    }
    if (prev && !curr && remote_map[prev] != SDL_SCANCODE_UNKNOWN) {
        SDL_SendKeyboardKey(SDL_RELEASED, remote_map[prev]);
    }
}


static void PS5_JoystickUpdate(SDL_Joystick *joystick)
{
    PS5_PadContext *ctx = PS5_JoystickGetPadContext(joystick);
    PS5_PadData pad = { 0 };
    Uint32 btn_change = 0;
    Uint8 hat = 0;
    int err = 0;

    if (!ctx) {
        SDL_SetError("PS5_JoystickUpdate: instance not connected");
        return;
    }

    err = scePadReadState(ctx->handle, &pad);
    if(err) {
        SDL_SetError("scePadReadState: 0x%08x", err);
        return;
    }

    // Axes
    if (ctx->pad.leftStick.x != pad.leftStick.x) {
        SDL_PrivateJoystickAxis(joystick, PS5_PAD_AXIS_LX,
                                analog_map[pad.leftStick.x]);
    }
    if (ctx->pad.leftStick.y != pad.leftStick.y) {
        SDL_PrivateJoystickAxis(joystick, PS5_PAD_AXIS_LY,
                                analog_map[pad.leftStick.y]);
    }
    if (ctx->pad.rightStick.x != pad.rightStick.x) {
        SDL_PrivateJoystickAxis(joystick, PS5_PAD_AXIS_RX,
                                analog_map[pad.rightStick.x]);
    }
    if (ctx->pad.rightStick.y != pad.rightStick.y) {
        SDL_PrivateJoystickAxis(joystick, PS5_PAD_AXIS_RY,
                                analog_map[pad.rightStick.y]);
    }
    if (ctx->pad.analogButtons.l2 != pad.analogButtons.l2) {
        SDL_PrivateJoystickAxis(joystick, PS5_PAD_AXIS_L2,
                                analog_map[pad.analogButtons.l2]);
    }
    if (ctx->pad.analogButtons.r2 != pad.analogButtons.r2) {
        SDL_PrivateJoystickAxis(joystick, PS5_PAD_AXIS_R2,
                                analog_map[pad.analogButtons.r2]);
    }

    // Buttons
    btn_change = ctx->pad.buttons ^ pad.buttons;
    if (btn_change) {
        for (int i = 0; i < SDL_arraysize(btn_map); i++) {
            if (btn_map[i] == -1) {
                continue;
            }
            if (btn_change & btn_map[i]) {
                if (pad.buttons & btn_map[i]) {
                    SDL_PrivateJoystickButton(joystick, i, SDL_PRESSED);
                } else {
                    SDL_PrivateJoystickButton(joystick, i, SDL_RELEASED);
                }
            }
        }

        if (pad.buttons & PS5_PAD_BUTTON_UP) {
            hat |= SDL_HAT_UP;
        }
        if (pad.buttons & PS5_PAD_BUTTON_DOWN) {
            hat |= SDL_HAT_DOWN;
        }
        if (pad.buttons & PS5_PAD_BUTTON_LEFT) {
            hat |= SDL_HAT_LEFT;
        }
        if (pad.buttons & PS5_PAD_BUTTON_RIGHT) {
            hat |= SDL_HAT_RIGHT;
        }

        SDL_PrivateJoystickHat(joystick, 0, hat);
    }

    if(SDL_memcmp(&ctx->pad.unknown, &pad.unknown, sizeof(pad.unknown))) {
      if(PS5_IS_REMOTE(ctx)) {
	PS5_JoystickUpdateRemote(ctx->pad.unknown[3], pad.unknown[3]);
      }
    }

    SDL_memcpy(&ctx->pad, &pad, sizeof(pad));
}

static SDL_JoystickGUID PS5_JoystickGetDeviceGUID(int device_index)
{
    PS5_PadContext *ctx = PS5_JoystickGetDevice(device_index);
    SDL_JoystickGUID guid = { 0 };

    return ctx ? ctx->global_id : guid;
}

static void PS5_JoystickDetectRemote(void)
{
    PS5_PadContext *ctx = &pad_ctx[PS5_REMOTE_INDEX];
    PS5_PadData pad = { 0 };

    if (ctx->handle < 0 || scePadReadState(ctx->handle, &pad) != 0) {
        return;
    }

    if (pad.connected && ctx->instance_id == -1) {
        ctx->instance_id = instance_counter++;
        SDL_zero(ctx->pad);
        SDL_PrivateJoystickAdded(ctx->instance_id);

    } else if (!pad.connected && ctx->instance_id != -1) {
        SDL_PrivateJoystickRemoved(ctx->instance_id);
        ctx->instance_id = -1;
    }
}

static void PS5_JoystickDetect(void)
{
    int user_ids[PS5_MAX_USERS];

    PS5_JoystickDetectRemote();

    if (sceUserServiceGetLoginUserIdList(user_ids) != 0) {
        SDL_SetError("sceUserServiceGetLoginUserIdList: %s", strerror(errno));
        return;
    }

    for (int i = 0; i < PS5_MAX_USERS; i++) {
        if (user_ids[i] == pad_ctx[i].user_id) {
            continue;
        }

        if (user_ids[i] != -1) {
            pad_ctx[i].instance_id = instance_counter++;
            pad_ctx[i].user_id = user_ids[i];
            SDL_zero(pad_ctx[i].pad);
            if (sceUserServiceGetUserName(user_ids[i], pad_ctx[i].name,
                                          sizeof(pad_ctx[i].name))) {
                sprintf(pad_ctx[i].name, "%08x", pad_ctx[i].user_id);
            }
            SDL_PrivateJoystickAdded(pad_ctx[i].instance_id);
        } else {
            SDL_PrivateJoystickRemoved(pad_ctx[i].instance_id);
            scePadClose(pad_ctx[i].handle);
            pad_ctx[i].instance_id = -1;
            pad_ctx[i].handle = -1;
            pad_ctx[i].user_id = user_ids[i];
            pad_ctx[i].name[0] = 0;
        }
    }
}

static int PS5_JoystickGetCount(void)
{
    int n = 0;

    for (int i = 0; i < SDL_arraysize(pad_ctx); i++) {
        n += (pad_ctx[i].instance_id != -1);
    }

    return n;
}

static int PS5_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    PS5_PadContext *ctx = PS5_JoystickGetDevice(device_index);
    int err;

    if (!ctx) {
        return SDL_SetError("PS5_JoystickOpen: Invalid device index");
    }

    if (!PS5_IS_REMOTE(ctx)) {
        ctx->handle = scePadOpen(ctx->user_id, PS5_PAD_PORT_TYPE_STANDARD,
                                 0, NULL);
        if (ctx->handle < 0) {
            return SDL_SetError("scePadOpen: 0x%08x", ctx->handle);
        }

        err = scePadSetVibrationMode(ctx->handle, 2);
        if(err) {
            scePadClose(ctx->handle);
            ctx->handle = -1;
            return SDL_SetError("scePadSetVibration: 0x%08x", err);
        }
    }

    joystick->nbuttons = SDL_arraysize(btn_map);
    joystick->naxes = (PS5_IS_REMOTE(ctx)) ? 0 : 6;
    joystick->nhats = 1;
    joystick->instance_id = ctx->instance_id;

    return 0;
}

static void PS5_JoystickClose(SDL_Joystick *joystick)
{
    PS5_PadContext *ctx = PS5_JoystickGetPadContext(joystick);
    int err;

    if (!ctx || PS5_IS_REMOTE(ctx)) {
        return;
    }

    err = scePadClose(ctx->handle);
    if (err != 0) {
        SDL_SetError("scePadClose: 0x%08x", err);
    }

    ctx->handle = -1;
}

static int PS5_JoystickInit(void)
{
    PS5_PadContext *ctx;
    int handle;
    int err;

    for (int i = 0; i < SDL_arraysize(pad_ctx); i++) {
        ctx = &pad_ctx[i];
        ctx->user_id = -1;
        ctx->name[0] = 0;
        ctx->handle = -1;
        ctx->instance_id = -1;
        ctx->global_id = SDL_GUIDFromString(PS5_IS_REMOTE(ctx) ? PS5_REMOTE_GUID
                                                               : PS5_PAD_GUID);
    }

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
    if (handle > 0) {
        pad_ctx[PS5_REMOTE_INDEX].handle = handle;
        SDL_strlcpy(pad_ctx[PS5_REMOTE_INDEX].name, "PS5 Remote Control",
                    sizeof(pad_ctx[PS5_REMOTE_INDEX].name));
    }

    PS5_JoystickDetect();

    return 0;
}

static void PS5_JoystickQuit(void)
{
    if (pad_ctx[PS5_REMOTE_INDEX].handle >= 0) {
        scePadClose(pad_ctx[PS5_REMOTE_INDEX].handle);
        pad_ctx[PS5_REMOTE_INDEX].handle = -1;
    }
}

static Uint32 PS5_JoystickGetCapabilities(SDL_Joystick *joystick)
{
    PS5_PadContext *ctx = PS5_JoystickGetPadContext(joystick);

    if (ctx && PS5_IS_REMOTE(ctx)) {
        return 0;
    }

    return SDL_JOYCAP_LED | SDL_JOYCAP_RUMBLE;
}

static int PS5_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble,
                              Uint16 high_frequency_rumble)
{
    PS5_PadVibration vib = {low_frequency_rumble/256, high_frequency_rumble/256};
    PS5_PadContext *ctx = PS5_JoystickGetPadContext(joystick);
    int err;

    if (!ctx) {
        return SDL_SetError("PS5_JoystickRumble: instance not connected");
    }

    err = scePadSetVibration(ctx->handle, &vib);
    if (err != 0) {
        return SDL_SetError("scePadSetVibration: 0x%08x", err);
    }

    return 0;
}

static int PS5_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green,
                              Uint8 blue)
{
    PS5_PadContext *ctx = PS5_JoystickGetPadContext(joystick);
    PS5_PadColor color = { red, green, blue, 255 };
    int err;

    if (!ctx) {
        return SDL_SetError("PS5_JoystickSetLED: instance not connected");
    }

    err = scePadSetLightBar(ctx->handle, &color);
    if (err != 0) {
        return SDL_SetError("scePadSetLightBar: 0x%08x", err);
    }

    return 0;
}

//
//
// TODO
//
//

static int PS5_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left,
                                      Uint16 right)
{
    return SDL_Unsupported();
}

static int
PS5_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static int
PS5_JoystickSetSensorsEnabled(SDL_Joystick *joystick, SDL_bool enabled)
{
    return SDL_Unsupported();
}

static const char *PS5_JoystickGetDevicePath(int index)
{
    return NULL;
}

static int PS5_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void PS5_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static int PS5_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
    return -1;
}

SDL_JoystickDriver SDL_PS5_JoystickDriver = {
    PS5_JoystickInit,
    PS5_JoystickGetCount,
    PS5_JoystickDetect,
    PS5_JoystickGetDeviceName,
    PS5_JoystickGetDevicePath,
    PS5_JoystickGetDeviceSteamVirtualGamepadSlot,
    PS5_JoystickGetDevicePlayerIndex,
    PS5_JoystickSetDevicePlayerIndex,
    PS5_JoystickGetDeviceGUID,
    PS5_JoystickGetDeviceInstanceID,
    PS5_JoystickOpen,
    PS5_JoystickRumble,
    PS5_JoystickRumbleTriggers,
    PS5_JoystickGetCapabilities,
    PS5_JoystickSetLED,
    PS5_JoystickSendEffect,
    PS5_JoystickSetSensorsEnabled,
    PS5_JoystickUpdate,
    PS5_JoystickClose,
    PS5_JoystickQuit,
    PS5_JoystickGetGamepadMapping
};

#endif /* SDL_JOYSTICK_PS5 */

/* vi: set ts=4 sw=4 expandtab: */
