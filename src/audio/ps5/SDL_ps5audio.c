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

#ifdef SDL_AUDIO_DRIVER_PS5

#include "SDL_audio.h"
#include "SDL_ps5audio.h"


inline static Uint16 PS5AUDIO_SampleSize(Uint16 size)
{
    if (size >= 2048) return 2048;
    if (size >= 1792) return 1792;
    if (size >= 1536) return 1536;
    if (size >= 1280) return 1280;
    if (size >= 1024) return 1024;
    if (size >= 768) return 768;
    if (size >= 512) return 512;
    return 256;
}

static int PS5AUDIO_OpenDevice(_THIS, const char *devname)
{
    SDL_bool supported_format = SDL_FALSE;
    SDL_AudioFormat test_format;
    size_t mix_len, i;
    Uint8 fmt;

    this->hidden = (struct SDL_PrivateAudioData *) SDL_malloc(sizeof(*this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(this->hidden);

    test_format = SDL_FirstAudioFormat(this->spec.format);
    while ((!supported_format) && (test_format)) {
        if (test_format == AUDIO_S16LSB) {
            supported_format = SDL_TRUE;
            fmt = (this->spec.channels == 1) ? PROSPERO_AUDIO_OUT_PARAM_FORMAT_S16_MONO
                                             : PROSPERO_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;
        } else if (test_format == AUDIO_F32LSB) {
            supported_format = SDL_TRUE;
            fmt = (this->spec.channels == 1) ? PROSPERO_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO
                                             : PROSPERO_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO;
        } else {
            test_format = SDL_NextAudioFormat();
        }
    }

    if (!supported_format) {
        return SDL_SetError("PS5AUDIO_OpenDevice: unsupported audio format: 0x%08x",
                            test_format);
    }

    this->spec.format = test_format;
    this->spec.samples = PS5AUDIO_SampleSize(this->spec.samples);
    this->spec.freq = 48000;
    this->spec.channels = this->spec.channels == 1 ? 1 : 2;

    /* Update the fragment size as size in bytes. */
    SDL_CalculateAudioSpec(&this->spec);

    /* Allocate the mixing buffer.  Its size and starting address must
       be a multiple of 64 bytes.  Our sample count is already a multiple of
       64, so spec->size should be a multiple of 64 as well. */
    mix_len = this->spec.size * NUM_BUFFERS;
    if (posix_memalign((void**)&this->hidden->rawbuf, 64, mix_len)) {
        return SDL_SetError("PS5AUDIO_OpenDevice: couldn't allocate mix buffer");
    }

    this->hidden->aout = sceAudioOutOpen(PROSPERO_USER_SERVICE_USER_ID_SYSTEM,
                                         PROSPERO_AUDIO_OUT_PORT_TYPE_MAIN,
                                         0, this->spec.samples, 48000, fmt);
    if (this->hidden->aout < 1) {
        free(this->hidden->rawbuf);
        this->hidden->rawbuf = NULL;
        return SDL_SetError("sceAudioOutOpen: %s", strerror(this->hidden->aout));
    }

    SDL_memset(this->hidden->rawbuf, 0, mix_len);
    for (i = 0; i < NUM_BUFFERS; i++) {
        this->hidden->mixbufs[i] = &this->hidden->rawbuf[i * this->spec.size];
    }

    this->hidden->next_buffer = 0;

    return 0;
}

static void PS5AUDIO_PlayDevice(_THIS)
{
    Uint8 *buf = this->hidden->mixbufs[this->hidden->next_buffer];
    sceAudioOutOutput(this->hidden->aout, buf);
    this->hidden->next_buffer = (this->hidden->next_buffer + 1) % NUM_BUFFERS;
}

/* This function waits until it is possible to write a full sound buffer */
static void PS5AUDIO_WaitDevice(_THIS)
{
    sceAudioOutOutput(this->hidden->aout, NULL);
}

static Uint8 *PS5AUDIO_GetDeviceBuf(_THIS)
{
    return this->hidden->mixbufs[this->hidden->next_buffer];
}

static void PS5AUDIO_CloseDevice(_THIS)
{
    int res;
    if (this->hidden->aout > 0) {
        res = sceAudioOutClose(this->hidden->aout);
        if (res != 0) {
            SDL_SetError("sceAudioOutClose: %s", strerror(res));
        }
        this->hidden->aout = -1;
    }

    if (this->hidden->rawbuf != NULL) {
        free(this->hidden->rawbuf);
        this->hidden->rawbuf = NULL;
    }
}

static void PS5AUDIO_ThreadInit(_THIS)
{
}


static void PS5AUDIO_Deinitialize(void)
{
}

static SDL_bool PS5AUDIO_Init(SDL_AudioDriverImpl *impl)
{
    if (sceAudioOutInit()) {
        return SDL_FALSE;
    }

    impl->ThreadInit = PS5AUDIO_ThreadInit;
    impl->OpenDevice = PS5AUDIO_OpenDevice;
    impl->PlayDevice = PS5AUDIO_PlayDevice;
    impl->WaitDevice = PS5AUDIO_WaitDevice;
    impl->GetDeviceBuf = PS5AUDIO_GetDeviceBuf;
    impl->CloseDevice = PS5AUDIO_CloseDevice;
    impl->Deinitialize = PS5AUDIO_Deinitialize;

    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;

    return SDL_TRUE;
}

AudioBootStrap PS5AUDIO_bootstrap = { "ps5", "PS5 audio driver", PS5AUDIO_Init, SDL_FALSE };

#endif /* SDL_AUDIO_DRIVER_PS5 */

/* vi: set ts=4 sw=4 expandtab: */
