// Minimal Switch entry point: paints the screen #0d1018 for two seconds and exits.
#include <SDL2/SDL.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

int main(int, char**) {
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
