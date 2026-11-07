// Switch entry point. Without a valid launch line (opened from hbmenu) it shows a
// message for four seconds and exits; the real error screen arrives with the UI.
#include <SDL2/SDL.h>

#include <cstdio>
#include <cstring>
#include <variant>

#include "args.hpp"
#include "core.hpp"
#include "platform/audio.hpp"
#include "platform/input.hpp"
#include "platform/video.hpp"
#include "rom.hpp"
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

    bool showFps = false;
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--show-fps")) showFps = true;

    auto fail = [](const char* what) {
        std::fprintf(stderr, "%s\n", what);
        return 0;  // the return to Equinox is already scheduled; Task 7 adds the error screen
    };

    auto loaded = loadRom(args->rom);
    if (auto* err = std::get_if<RomError>(&loaded)) {
        std::fprintf(stderr, "rom error %d: %s\n", static_cast<int>(*err), args->rom.c_str());
        return fail("could not load the ROM");
    }
    const Rom& rom = std::get<Rom>(loaded);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO) != 0) return fail("SDL_Init failed");
    SDL_Window* window = SDL_CreateWindow("Equinox SNES", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN);
    int rc = 0;
    {
        Video video;
        Input input;
        Audio audio;
        Core core;
        if (!window || !video.init(window, showFps)) {
            rc = fail("video init failed");
        } else {
            input.init();
            core.onVideo = [&](const void* px, unsigned w, unsigned h, size_t pitch) {
                video.upload(px, w, h, pitch);
            };
            core.onInput = [&](unsigned port, unsigned id) { return input.state(port, id); };
            if (!core.load(rom, args->saves, args->rom)) {
                rc = fail("core failed to load the ROM");
            } else {
                if (audio.init(core.sampleRate(), core.fps()))
                    core.onAudio = [&](const int16_t* lr, size_t frames) { audio.push(lr, frames); };
                else
                    std::fprintf(stderr, "audio init failed; running silent\n");
                bool running = true;
                while (running) {
                    SDL_Event e;
                    while (SDL_PollEvent(&e)) {
                        if (e.type == SDL_QUIT) running = false;
                        input.handleEvent(e);
                    }
                    // Development exit until the Task 7 menu: Plus+Minus held for 2 s.
                    if (input.comboHeld(2000)) running = false;
                    core.runFrame();
                    video.present();  // vsync paces one frame per refresh
                }
                audio.shutdown();
                core.unload();
            }
            input.shutdown();
        }
    }
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return rc;
}
