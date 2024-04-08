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

#ifndef _SDL_PS5AUDIO_H_
#define _SDL_PS5AUDIO_H_

#include "../SDL_sysaudio.h"

/* Hidden "this" pointer for the audio functions */
#define _THIS   SDL_AudioDevice *this

#define NUM_BUFFERS 2

struct SDL_PrivateAudioData {
    /* The hardware output channel. */
    int32_t aout;
    /* The raw allocated mixing buffer. */
    Uint8 *rawbuf;
    /* Individual mixing buffers. */
    Uint8 *mixbufs[NUM_BUFFERS];
    /* Index of the next available mixing buffer. */
    int next_buffer;
};

#define PROSPERO_AUDIO_OUT_PORT_TYPE_MAIN 0

#define PROSPERO_AUDIO_OUT_PARAM_FORMAT_S16_MONO     0
#define PROSPERO_AUDIO_OUT_PARAM_FORMAT_S16_STEREO   1
#define PROSPERO_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO   3
#define PROSPERO_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO 4

#define PROSPERO_USER_SERVICE_USER_ID_SYSTEM 0xFF


int32_t sceAudioOutInit(void);
int32_t sceAudioOutOpen(int32_t userId, int32_t type, int32_t index,
                        uint32_t len, uint32_t freq, uint32_t param);
int32_t sceAudioOutOutput(int32_t handle, const void *p);
int32_t sceAudioOutClose(int32_t handle);


#endif /* _SDL_PS5AUDIO_H_ */

/* vi: set ts=4 sw=4 expandtab: */
