#include "../../SDL_internal.h"

#ifdef __PROSPERO__

#include "SDL_main.h"

#ifdef main
#undef main
#endif

int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceLoadExec(const char*, const char**);

int main(int argc, char *argv[])
{
    sceSystemServiceHideSplashScreen();
    SDL_main(argc, argv);
    sceSystemServiceLoadExec("exit", 0);
}

#endif // __PROSPERO__

/* vi: set ts=4 sw=4 expandtab: */
