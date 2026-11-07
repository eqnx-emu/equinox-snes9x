// Switch entry point. Without a valid launch line (opened from hbmenu) it shows a
// message for four seconds and exits; the real error screen arrives with the UI.
#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <variant>

#include "args.hpp"
#include "core.hpp"
#include "file_io.hpp"
#include "paths.hpp"
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

// Battery save (SRAM) persistence. Writes go through writeFileAtomic so a power cut
// never leaves a half-written .srm. A write happens only when the content hash changed:
// polled every 60 frames with at least 5 s between writes, and immediately (no delay)
// on menu exit, losing focus (HOME), an exit request, and before returning to Equinox.
class SramSaver {
public:
    void attach(Core* core, std::string path) {
        core_ = core;
        path_ = std::move(path);
        if (auto saved = readFile(path_)) {
            auto ram = core_->saveRam();
            const size_t n = std::min(ram.size(), saved->size());
            if (n) std::memcpy(ram.data(), saved->data(), n);
        }
        lastHash_ = hash(core_->saveRam());
        lastWrite_ = SDL_GetTicks();
    }
    void detach() { core_ = nullptr; }

    // Called once per frame.
    void tick() {
        if (!core_ || ++frames_ < 60) return;
        frames_ = 0;
        if (SDL_GetTicks() - lastWrite_ < 5000) return;
        write();
    }
    // Immediate write if the content changed.
    void flush() {
        if (core_) write();
    }

private:
    static uint64_t hash(std::span<const uint8_t> d) {
        uint64_t h = 1469598103934665603ull;  // FNV-1a
        for (uint8_t b : d) h = (h ^ b) * 1099511628211ull;
        return h;
    }
    void write() {
        auto ram = core_->saveRam();
        if (ram.empty()) return;
        const uint64_t h = hash(ram);
        if (h == lastHash_) return;
        lastWrite_ = SDL_GetTicks();
        if (writeFileAtomic(path_, ram)) {
            lastHash_ = h;
        } else {
            // The "Nao foi possivel salvar" toast arrives with the Task 7 UI; retried on the next trigger.
            std::fprintf(stderr, "could not save %s\n", path_.c_str());
        }
    }
    Core* core_ = nullptr;
    std::string path_;
    uint64_t lastHash_ = 0;
    Uint32 lastWrite_ = 0;
    int frames_ = 0;
};

static SramSaver g_sram;

#ifdef __SWITCH__
// HOME / focus loss / exit request. The hook runs on the main thread inside
// appletMainLoop (called by SDL_PollEvent), so the core is idle and flushing is safe.
static AppletHookCookie g_hookCookie;
static void appletHookFn(AppletHookType type, void*) {
    if (type == AppletHookType_OnFocusState) {
        if (appletGetFocusState() != AppletFocusState_InFocus) g_sram.flush();
    } else if (type == AppletHookType_OnExitRequest) {
        g_sram.flush();
    }
}
#endif

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
                g_sram.attach(&core, srmPath(args->saves, args->rom));
#ifdef __SWITCH__
                appletHook(&g_hookCookie, appletHookFn, nullptr);
#endif
                bool running = true;
                while (running) {
                    SDL_Event e;
                    while (SDL_PollEvent(&e)) {
                        if (e.type == SDL_QUIT) running = false;
                        if (e.type == SDL_APP_WILLENTERBACKGROUND || e.type == SDL_APP_TERMINATING)
                            g_sram.flush();
                        input.handleEvent(e);
                    }
                    // Development exit until the Task 7 menu: Plus+Minus held for 2 s.
                    if (input.comboHeld(2000)) running = false;
                    core.runFrame();
                    g_sram.tick();
                    video.present();  // vsync paces one frame per refresh
                }
                g_sram.flush();  // exit: before the scheduled return to Equinox fires
#ifdef __SWITCH__
                appletUnhook(&g_hookCookie);
#endif
                g_sram.detach();
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
