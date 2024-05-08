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
#include "SDL_events.h"
#include "SDL_error.h"

#include "SDL_ps5joystick.h"

#define PS5_MAX_USERS 4

typedef struct PS5_PadContext {
  int user_id;
  int handle;
  SDL_JoystickID instance_id;
  PS5_PadData pad;
} PS5_PadContext;

static PS5_PadContext pad_ctx[PS5_MAX_USERS];

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
    if(device_index < 0 || device_index >= SDL_arraysize(pad_ctx)) {
        return -1;
    }

    return pad_ctx[device_index].instance_id;
}

static const char *PS5_JoystickGetDeviceName(int device_index)
{
    if(device_index < 0 || device_index >= SDL_arraysize(pad_ctx)) {
        return NULL;
    }

    return "Sony DualSense";
}

static void PS5_JoystickUpdate(SDL_Joystick *joystick)
{
    SDL_JoystickID instance_id = SDL_JoystickInstanceID(joystick);
    PS5_PadContext *ctx = NULL;
    uint32_t btn_change;
    PS5_PadData pad;

    for (int i=0; i<SDL_arraysize(pad_ctx); i++) {
        if (instance_id == pad_ctx[i].instance_id) {
	    ctx = &pad_ctx[i];
	    break;
	}
    }

    if(!ctx || instance_id < 0) {
        SDL_SetError("PS5_JoystickUpdate: instance not connected");
	return;
    }
    
    switch(scePadReadState(ctx->handle, &pad)) {
    case 0:
      break;

      // TODO: on disconnect
      
    default:
      SDL_SetError("scePadReadState: %s", strerror(errno));
      return;
    }

    btn_change = ctx->pad.buttons ^ pad.buttons;
    if(btn_change) {
      for (int i=0; i<SDL_arraysize(btn_map); i++) {
	if (btn_change & btn_map[i]) {
	  if (pad.buttons & btn_map[i]) {
	    SDL_PrivateJoystickButton(joystick, i, SDL_PRESSED);
	  } else {
	    SDL_PrivateJoystickButton(joystick, i, SDL_RELEASED);
	  }
	}
      }
    }

    memcpy(&ctx->pad, &pad, sizeof(pad));
}

static SDL_JoystickGUID PS5_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid = {0};

    if(device_index < 0 || device_index >= SDL_arraysize(pad_ctx)) {
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

    for (int i=0; i<PS5_MAX_USERS; i++) {
        if(user_ids[i] != -1 && pad_ctx[i].user_id == -1) {
	    pad_ctx[i].user_id = user_ids[i];
	}
    }
}

static int PS5_JoystickGetCount(void)
{
    int n = 0;

    for(int i=0; i<SDL_arraysize(pad_ctx); i++) {
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
    if(pad_ctx[device_index].handle < 0) {
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

    for (int i=0; i<SDL_arraysize(pad_ctx); i++) {
        if (instance_id == pad_ctx[i].instance_id) {
	    ctx = &pad_ctx[i];
	    break;
	}
    }

    if(!ctx || instance_id < 0) {
	return;
    }
    
    if(scePadClose(ctx->handle) != 0) {
        SDL_SetError("scePadClose: %s", strerror(errno));
	//return;
    }

    ctx->handle = -1;
}

static int PS5_JoystickInit(void)
{
    for(int i=0; i<SDL_arraysize(pad_ctx); i++) {
        pad_ctx[i].user_id = -1;
	pad_ctx[i].handle = -1;
	pad_ctx[i].instance_id = -1;
    }

    if(sceUserServiceInitialize(0) != 0) {
        return SDL_SetError("sceUserServiceInitialize: %s", strerror(errno));
    }

    if (scePadInit() != 0) {
        return SDL_SetError("scePadInit: %s", strerror(errno));
    }

    PS5_JoystickDetect();

    return PS5_JoystickGetCount() > 0 ? 0 : -1;
}

static void PS5_JoystickQuit(void)
{
    //NOP
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

static SDL_bool PS5_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    return SDL_TRUE;
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

