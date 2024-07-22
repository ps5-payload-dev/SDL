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

typedef enum SceImeDialogStatus
{
    SCE_IME_DIALOG_STATUS_NONE,
    SCE_IME_DIALOG_STATUS_RUNNING,
    SCE_IME_DIALOG_STATUS_FINISHED
} SceImeDialogStatus;

typedef int (*SceImeTextFilter)(wchar_t*, uint32_t*, const wchar_t*, uint32_t);

typedef struct SceImeDialogParam
{
    int userId;
    enum {
	SCE_IME_TYPE_DEFAULT,
	SCE_IME_TYPE_BASIC_LATIN,
	SCE_IME_TYPE_URL,
	SCE_IME_TYPE_MAIL,
	SCE_IME_TYPE_NUMBER
    } type;
    uint64_t supportedLanguages;
    enum {
	SCE_IME_ENTER_LABEL_DEFAULT,
	SCE_IME_ENTER_LABEL_SEND,
	SCE_IME_ENTER_LABEL_SEARCH,
	SCE_IME_ENTER_LABEL_GO,
    } enterLabel;
    enum {
	SCE_IME_INPUT_METHOD_DEFAULT
    } inputMethod;
    SceImeTextFilter filter;
    uint32_t option;
    uint32_t maxTextLength;
    wchar_t *inputTextBuffer;
    float posx;
    float posy;
    enum {
	SCE_IME_HALIGN_LEFT,
	SCE_IME_HALIGN_CENTER,
	SCE_IME_HALIGN_RIGHT
    } halign;
    enum {
	SCE_IME_VALIGN_TOP,
	SCE_IME_VALIGN_CENTER,
	SCE_IME_VALIGN_BOTTOM
    } valign;
    const wchar_t *placeholder;
    const wchar_t *title;
    int8_t reserved[16];
} SceImeDialogParam;

typedef struct SceImeDialogResult
{
    enum {
	SCE_IME_DIALOG_END_STATUS_OK,
	SCE_IME_DIALOG_END_STATUS_USER_CANCELED,
	SCE_IME_DIALOG_END_STATUS_ABORTED,
    } outcome;
    int8_t reserved[12];
} SceImeDialogResult;


int sceKeyboardInit(void);
int sceKeyboardOpen(int, int, int, void *);
int sceKeyboardReadState(int, keyboard_state_t *);
int sceKeyboardClose(int);

int sceUserServiceInitialize(void *);
int sceUserServiceGetForegroundUser(int *);

int sceImeDialogInit(const SceImeDialogParam*, void*);
int sceImeDialogGetResult(SceImeDialogResult*);
int sceImeDialogTerm(void);

SceImeDialogStatus sceImeDialogGetStatus(void);


static int g_keyboard_handle = -1;
static keyboard_state_t g_prev_keyboard_state = {0};

static SceImeDialogStatus g_ime_dialog_status = SCE_IME_DIALOG_STATUS_NONE;;
static wchar_t g_ime_dialog_title[0x80] = {0};
static wchar_t g_ime_dialog_text[0x800] = {0};

static SceImeDialogParam g_ime_dialog_param =
{
    .title = g_ime_dialog_title,
    .inputTextBuffer = g_ime_dialog_text,
    .maxTextLength = sizeof(g_ime_dialog_text) / sizeof(g_ime_dialog_text[0])
};

int PS5_Keyboard_Open(void)
{
    int user_id;
    long junk;
    int err;

    err = sceUserServiceGetForegroundUser(&user_id);
    if (err != 0) {
        return SDL_SetError("sceUserServiceGetForegroundUser: 0x%08x", err);
    }

    g_ime_dialog_param.userId = user_id;
    g_keyboard_handle = sceKeyboardOpen(user_id, 0, 0, &junk);
    if (g_keyboard_handle <= 0) {
        return SDL_SetError("sceKeyboardOpen: 0x%08x", g_keyboard_handle);
    }

    return 0;
}

int PS5_Keyboard_Close(void)
{
    int err;

    if (g_keyboard_handle <= 0) {
        return 0;
    }

    err = sceKeyboardClose(g_keyboard_handle);
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

static int PS5_ImeDialog_PumpEvents(void)
{
    SceImeDialogStatus status = sceImeDialogGetStatus();
    SceImeDialogResult result = {0};
    char text[0x800];

    if(g_ime_dialog_status == status) {
	return 0;
    } else {
	g_ime_dialog_status = status;
    }

    switch(status) {
    case SCE_IME_DIALOG_STATUS_NONE:
	return 0;

    case SCE_IME_DIALOG_STATUS_RUNNING:
	return 0;

    case SCE_IME_DIALOG_STATUS_FINISHED:
	break;

    default:
	return -1;
    }

    if(sceImeDialogGetResult(&result)) {
	return -1;
    }

    switch(result.outcome) {
    case SCE_IME_DIALOG_END_STATUS_OK:
      if(wcstombs(text, g_ime_dialog_text, sizeof(text)) >= 0) {
	SDL_SendKeyboardText(text);
      }
      SDL_SendKeyboardKeyAutoRelease(SDL_SCANCODE_RETURN);
      break;

    case SCE_IME_DIALOG_END_STATUS_USER_CANCELED:
    case SCE_IME_DIALOG_END_STATUS_ABORTED:
	break;
    default:
	return -1;
    }

    sceImeDialogTerm();

    return 0;
}

int PS5_Keyboard_PumpEvents(void)
{
    keyboard_state_t curr;
    uint32_t diff;
    int err;

    PS5_ImeDialog_PumpEvents();

    if (g_keyboard_handle <= 0) {
        return 0;
    }

    err = sceKeyboardReadState(g_keyboard_handle, &curr);
    if (err != 0) {
        return SDL_SetError("sceKeyboardReadState: 0x%08x", err);
    }

    if (!curr.available) {
        return 0;
    }

    diff = g_prev_keyboard_state.modifiers ^ curr.modifiers;
    if (diff) {
        if (diff & 0x01) {
            if (g_prev_keyboard_state.modifiers & 0x01) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LCTRL);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LCTRL);
            }
        }
        if (diff & 0x02) {
            if (g_prev_keyboard_state.modifiers & 0x02) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LSHIFT);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LSHIFT);
            }
        }
        if (diff & 0x04) {
            if (g_prev_keyboard_state.modifiers & 0x04) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LALT);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LALT);
            }
        }
        if (diff & 0x08) {
            if (g_prev_keyboard_state.modifiers & 0x08) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LGUI);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LGUI);
            }
        }
        if (diff & 0x10) {
            if (g_prev_keyboard_state.modifiers & 0x10) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RCTRL);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RCTRL);
            }
        }
        if (diff & 0x20) {
            if (g_prev_keyboard_state.modifiers & 0x20) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RSHIFT);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RSHIFT);
            }
        }
        if (diff & 0x40) {
            if (g_prev_keyboard_state.modifiers & 0x40) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RALT);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RALT);
            }
        }
        if (diff & 0x80) {
            if (g_prev_keyboard_state.modifiers & 0x80) {
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RGUI);
            } else {
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RGUI);
            }
        }
    }

    for (int i = 0; i < 16; i++) {
        if (g_prev_keyboard_state.scankey[i] == curr.scankey[i]) {
            continue;
        }

        if (g_prev_keyboard_state.scankey[i] == 0) {
            SDL_SendKeyboardKey(SDL_PRESSED, curr.scankey[i]);
        } else {
            SDL_SendKeyboardKey(SDL_RELEASED, g_prev_keyboard_state.scankey[i]);
        }
    }

    memcpy(&g_prev_keyboard_state, &curr, sizeof(curr));

    return 0;
}

SDL_bool PS5_HasScreenKeyboardSupport(_THIS)
{
    return SDL_TRUE;
}

void PS5_ShowScreenKeyboard(_THIS, SDL_Window *window)
{
    memset(g_ime_dialog_text, 0, sizeof(g_ime_dialog_text));
    sceImeDialogInit(&g_ime_dialog_param, NULL);
}

void PS5_HideScreenKeyboard(_THIS, SDL_Window *window)
{
    if(g_ime_dialog_status == SCE_IME_DIALOG_STATUS_FINISHED) {
      sceImeDialogTerm();
    }
}

SDL_bool PS5_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
    if(g_ime_dialog_status == SCE_IME_DIALOG_STATUS_RUNNING) {
        return SDL_TRUE;
    }

    return SDL_FALSE;
}

#endif /* SDL_VIDEO_DRIVER_PS5 */

/* vi: set ts=4 sw=4 expandtab: */
