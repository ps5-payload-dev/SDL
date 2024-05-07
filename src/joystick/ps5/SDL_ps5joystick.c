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

static int user_ids[4];
static unsigned int buttons = 0;

static const unsigned int btn_map[] = {
  PS5_PAD_BUTTON_CROSS,     // a:b0
  PS5_PAD_BUTTON_CIRCLE,    // b:b1
  PS5_PAD_BUTTON_SQUARE,    // x:b2
  PS5_PAD_BUTTON_TRIANGLE,  // y:b3
  PS5_PAD_BUTTON_OPTIONS,   // back:b4
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
    return device_index;
}

static const char *PS5_JoystickGetDeviceName(int index)
{
    return "Sony DualSense";
}

static void PS5_JoystickUpdate(SDL_Joystick *joystick)
{
    int handle = (int) SDL_JoystickInstanceID(joystick);
    unsigned int btn_change;
    PS5_PadData pad;

    if (scePadReadState(handle, &pad) != 0) {
        SDL_SetError("scePadReadState: %s\n", strerror(errno));
        return;
    }

    btn_change = buttons ^ pad.buttons;
    if(btn_change) {
      for (int i=0; i<SDL_arraysize(btn_map); i++) {
	if (btn_change & btn_map[i]) {

	  if(pad.buttons & btn_map[i]) {
	    SDL_PrivateJoystickButton(joystick, i, SDL_PRESSED);
	  } else {
	    SDL_PrivateJoystickButton(joystick, i, SDL_RELEASED);
	  }
	}
      }
      buttons = pad.buttons;
    }
}

static void PS5_JoystickClose(SDL_Joystick *joystick)
{
    int handle = (int) SDL_JoystickInstanceID(joystick);

    if(scePadClose(handle) != 0) {
        SDL_SetError("scePadClose: %s\n", strerror(errno));
	return;
    }
}

static void PS5_JoystickQuit(void)
{
    //NOP
}

static SDL_JoystickGUID PS5_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid;

    // TODO
    SDL_zero(guid);

    return guid;
}


static void PS5_JoystickDetect(void)
{
  // TODO
}

static int PS5_JoystickGetCount(void)
{
  return 1; // TODO
}


static int PS5_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    int handle;

    handle = scePadOpen(user_ids[device_index], 0, 0, NULL);
    if(handle < 0) {
      SDL_SetError("scePadOpen: %s\n", strerror(errno));
      return -1;
    }

    joystick->nbuttons = SDL_arraysize(btn_map);
    joystick->naxes = 6;
    joystick->nhats = 1;
    joystick->instance_id = handle;

    return 0;
}


static int PS5_JoystickInit(void)
{
    memset(user_ids, 0xff, sizeof(user_ids));

    if(sceUserServiceInitialize(0) != 0) {
        return SDL_SetError("sceUserServiceInitialize: %s\n", strerror(errno));
    }

    if (sceUserServiceGetLoginUserIdList(user_ids) != 0) {
        return SDL_SetError("sceUserServiceGetLoginUserIdList: %s\n",
			    strerror(errno));
    }

    if (scePadInit() != 0) {
        return SDL_SetError("scePadInit: %s\n", strerror(errno));
    }

    return 0;
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

