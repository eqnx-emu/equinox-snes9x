// Switch entry point. Without a valid launch line (opened from hbmenu) it shows a
// message for four seconds and exits; the real error screen arrives with the UI.
#include <SDL2/SDL.h>

#include <cstdio>

#include "args.hpp"
#include "platform/relaunch.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

static void showOpenFromEquinoxMessage() {
    const char* msg = "Abra este emulador pelo Equinox";
#ifdef __SWITCH__
    consoleInit(nullptr);
    std::printf("\n\n  %s\n", msg);
    consoleUpdate(nullptr);
    svcSleepThread(4'000'000'000LL);
    consoleExit(nullptr);
#else
    std::fprintf(stderr, "%s\n", msg);
    SDL_Delay(4000);
#endif
}

int main(int argc, char** argv) {
    const auto args = parseArgs(argc, argv);
    if (!args) {
        showOpenFromEquinoxMessage();
        return 0;
    }
    scheduleReturn(*args);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    SDL_Window* window = SDL_CreateWindow("Equinox SNES", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer =
        window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
               : nullptr;

    if (renderer) {
        const Uint32 start = SDL_GetTicks();
        while (SDL_GetTicks() - start < 2000) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {}
            SDL_SetRenderDrawColor(renderer, 0x0d, 0x10, 0x18, 0xff);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
        }
    }

    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
