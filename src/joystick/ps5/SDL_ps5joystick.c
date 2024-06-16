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

#if SDL_JOYSTICK_PS5

#include <errno.h>

#include "../SDL_sysjoystick.h"
#include "SDL_error.h"
#include "SDL_events.h"

#include "SDL_ps5joystick.h"

#define PS5_MAX_USERS 4

#define PS5_PAD_AXIS_LX 0
#define PS5_PAD_AXIS_LY 1
#define PS5_PAD_AXIS_RX 2
#define PS5_PAD_AXIS_RY 3
#define PS5_PAD_AXIS_L2 4
#define PS5_PAD_AXIS_R2 5

typedef struct PS5_PadContext
{
    int user_id;
    int handle;
    SDL_JoystickID instance_id;
    PS5_PadData pad;
} PS5_PadContext;

static PS5_PadContext pad_ctx[PS5_MAX_USERS];

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
    PS5_PAD_BUTTON_OPTIONS,   // back:b4
    -1,                       // guide:b5
    PS5_PAD_BUTTON_TOUCH_PAD, // start:b6
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

static SDL_JoystickID PS5_JoystickGetDeviceInstanceID(int device_index)
{
    if (device_index < 0 || device_index >= SDL_arraysize(pad_ctx)) {
        return -1;
    }

    return pad_ctx[device_index].instance_id;
}

static const char *PS5_JoystickGetDeviceName(int device_index)
{
    if (device_index < 0 || device_index >= SDL_arraysize(pad_ctx)) {
        return NULL;
    }

    return "Sony DualSense";
}

static SDL_bool PS5_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    out->a.kind = EMappingKind_Button;
    out->a.target = 0;
    out->b.kind = EMappingKind_Button;
    out->b.target = 1;
    out->x.kind = EMappingKind_Button;
    out->x.target = 2;
    out->y.kind = EMappingKind_Button;
    out->y.target = 3;
    out->back.kind = EMappingKind_Button;
    out->back.target = 4;
    // out->guide.kind = EMappingKind_Button;
    // out->guide.target = 5;
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

static void PS5_JoystickUpdate(SDL_Joystick *joystick)
{
    SDL_JoystickID instance_id = SDL_JoystickInstanceID(joystick);
    PS5_PadContext *ctx = NULL;
    uint32_t btn_change;
    PS5_PadData pad;
    uint8_t hat = 0;

    for (int i = 0; i < SDL_arraysize(pad_ctx); i++) {
        if (instance_id == pad_ctx[i].instance_id) {
            ctx = &pad_ctx[i];
            break;
        }
    }

    if (!ctx || instance_id < 0) {
        SDL_SetError("PS5_JoystickUpdate: instance not connected");
        return;
    }

    switch (scePadReadState(ctx->handle, &pad)) {
    case 0:
        break;

        // TODO: on disconnect

    default:
        SDL_SetError("scePadReadState: %s", strerror(errno));
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
            if (btn_change & btn_map[i]) {
                if (pad.buttons & btn_map[i]) {
                    SDL_PrivateJoystickButton(joystick, i, SDL_PRESSED);
                } else {
                    SDL_PrivateJoystickButton(joystick, i, SDL_RELEASED);
                }

                // handle hat
                if (i == 11 && (pad.buttons & btn_map[i])) {
                    hat |= SDL_HAT_UP;
                } else if (i == 12 && (pad.buttons & btn_map[i])) {
                    hat |= SDL_HAT_DOWN;
                } else if (i == 13 && (pad.buttons & btn_map[i])) {
                    hat |= SDL_HAT_LEFT;
                } else if (i == 14 && (pad.buttons & btn_map[i])) {
                    hat |= SDL_HAT_RIGHT;
                }
            }
        }
        // send hat events now
        SDL_PrivateJoystickHat(joystick, 0, hat);
    }

    memcpy(&ctx->pad, &pad, sizeof(pad));
}

static SDL_JoystickGUID PS5_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid = { 0 };

    if (device_index < 0 || device_index >= SDL_arraysize(pad_ctx)) {
        return guid;
    }

    // TODO

    return guid;
}

static void PS5_JoystickDetect(void)
{
    int user_ids[PS5_MAX_USERS];

    if (sceUserServiceGetLoginUserIdList(user_ids) != 0) {
        SDL_SetError("sceUserServiceGetLoginUserIdList: %s", strerror(errno));
        return;
    }

    for (int i = 0; i < PS5_MAX_USERS; i++) {
        if (user_ids[i] != -1 && pad_ctx[i].user_id == -1) {
            pad_ctx[i].user_id = user_ids[i];
        }
    }
}

static int PS5_JoystickGetCount(void)
{
    int n = 0;

    for (int i = 0; i < SDL_arraysize(pad_ctx); i++) {
        n += (pad_ctx[i].user_id != -1);
    }

    return n;
}

static int PS5_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    if (device_index < 0 || device_index >= SDL_arraysize(pad_ctx)) {
        return SDL_SetError("PS5_JoystickOpen: Invalid device index");
    }

    pad_ctx[device_index].handle = scePadOpen(pad_ctx[device_index].user_id,
                                              0, 0, NULL);
    if (pad_ctx[device_index].handle < 0) {
        return SDL_SetError("scePadOpen: %s", strerror(errno));
    }

    pad_ctx[device_index].instance_id++;

    joystick->nbuttons = SDL_arraysize(btn_map);
    joystick->naxes = 6;
    joystick->nhats = 1;
    joystick->instance_id = pad_ctx[device_index].instance_id;

    return 0;
}

static void PS5_JoystickClose(SDL_Joystick *joystick)
{
    SDL_JoystickID instance_id = SDL_JoystickInstanceID(joystick);
    PS5_PadContext *ctx = NULL;
    int err;

    for (int i = 0; i < SDL_arraysize(pad_ctx); i++) {
        if (instance_id == pad_ctx[i].instance_id) {
            ctx = &pad_ctx[i];
            break;
        }
    }

    if (!ctx || instance_id < 0) {
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
    int err;

    for (int i = 0; i < SDL_arraysize(pad_ctx); i++) {
        pad_ctx[i].user_id = -1;
        pad_ctx[i].handle = -1;
        pad_ctx[i].instance_id = -1;
    }

    err = sceUserServiceInitialize(0);
    if (err != 0 && err != 0x80960003) {
        return SDL_SetError("sceUserServiceInitialize: 0x%08x", err);
    }

    err = scePadInit();
    if (err != 0) {
        return SDL_SetError("scePadInit: 0x%08x", err);
    }

    PS5_JoystickDetect();

    return PS5_JoystickGetCount() > 0 ? 0 : -1;
}

static void PS5_JoystickQuit(void)
{
    // NOP
}

//
//
// TODO
//
//

static Uint32 PS5_JoystickGetCapabilities(SDL_Joystick *joystick)
{
    return 0;
}

static int PS5_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble,
                              Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static int PS5_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left,
                                      Uint16 right)
{
    return SDL_Unsupported();
}

static int PS5_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green,
                              Uint8 blue)
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
