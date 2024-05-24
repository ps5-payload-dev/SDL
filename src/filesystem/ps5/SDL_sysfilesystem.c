/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_FILESYSTEM_PS5

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */

#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

#include <sys/stat.h>

#include "SDL_error.h"
#include "SDL_stdinc.h"
#include "SDL_filesystem.h"


const char** getargv(void);

int sceUserServiceInitialize(void*);
int sceUserServiceGetForegroundUser(uint32_t *user_id);


char *SDL_GetBasePath(void)
{
    const char** argv = getargv();
    char dirname[PATH_MAX];

    if (!argv || !argv[0] || !strstr(argv[0], "/")) {
      return strdup("/data/");
    }

    if (!getcwd(dirname, sizeof(dirname))) {
        dirname[0] = 0;
    }

    if (argv[0][0] != '/') {
        strcat(dirname, "/");
	strcat(dirname, argv[0]);
    } else {
        strcpy(dirname, argv[0]);
    }

    for (int i=strlen(dirname); i>0; i--) {
        if (dirname[i-1] == '/') {
            dirname[i] = 0;
            break;
        }
    }

    return strdup(dirname);
}

static int first_run = 1;

char *SDL_GetPrefPath(const char *org, const char *app)
{
    char envr[PATH_MAX];
    uint32_t user_id;
    char *retval = 0;
    size_t len = 0;
    char *ptr = 0;

    if (!app) {
        SDL_InvalidParamError("app");
        return NULL;
    }
    if (!org) {
        org = "";
    }

    if (first_run) {
        if (sceUserServiceInitialize(0)) {
            SDL_SetError("sceUserServiceInitialize: %s", strerror(errno));
        }
        first_run = 0;
    }
    if (sceUserServiceGetForegroundUser(&user_id)) {
        strcpy(envr, "/data/");
    } else {
        sprintf(envr, "/user/home/%04x/", user_id);
    }
    len = SDL_strlen(envr);

    len += SDL_strlen(org) + SDL_strlen(app) + 3;
    retval = (char *) SDL_malloc(len);
    if (!retval) {
        SDL_OutOfMemory();
        return NULL;
    }

    if (*org) {
        SDL_snprintf(retval, len, "%s%s/%s/", envr, org, app);
    } else {
        SDL_snprintf(retval, len, "%s%s/", envr, app);
    }

    for (ptr = retval + 1; *ptr; ptr++) {
        if (*ptr == '/') {
            *ptr = '\0';
            mkdir(retval, 0777);
            *ptr = '/';
        }
    }

    mkdir(retval, 0777);

    return retval;
}

#endif /* SDL_FILESYSTEM_PS5 */

/* vi: set ts=4 sw=4 expandtab: */
