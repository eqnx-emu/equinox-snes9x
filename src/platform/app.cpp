// Switch entry point: SDL window, error screens, the main loop and the in-game menu.
#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>

#include "args.hpp"
#include "async_writer.hpp"
#include "core.hpp"
#include "file_io.hpp"
#include "menu.hpp"
#include "paths.hpp"
#include "platform/audio.hpp"
#include "platform/input.hpp"
#include "platform/menu_view.hpp"
#include "platform/png_load.hpp"
#include "platform/relaunch.hpp"
#include "platform/video.hpp"
#include "rom.hpp"
#include "strings.hpp"
#include "video_art.hpp"
#include "video_prefs.hpp"

#ifdef __SWITCH__
#include <switch.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

// Bottom-of-screen message. Save failures are debounced: the SRAM saver retries every
// 5 s, so the toast appears at most once per 10 s while failures continue.
class Toasts {
public:
    void show(std::string text) {
        text_ = std::move(text);
        until_ = SDL_GetTicks() + 2000;
    }
    void saveFailed(Lang lang) {
        const Uint32 now = SDL_GetTicks();
        if (failShown_ && now - lastFail_ < 10000) return;
        failShown_ = true;
        lastFail_ = now;
        show(tr(lang, Str::SaveFailed));
    }
    bool active() const { return SDL_GetTicks() < until_; }
    const std::string& text() const { return text_; }

private:
    std::string text_;
    Uint32 until_ = 0, lastFail_ = 0;
    bool failShown_ = false;
};

static Toasts g_toasts;
static Lang g_lang = Lang::EnUS;

// Battery save (SRAM) persistence. Writes go through writeFileAtomic so a power cut
// never leaves a half-written .srm. A write happens only when the content hash changed:
// polled every 60 frames with at least 5 s between writes, and immediately on menu exit,
// losing focus (HOME), an exit request, and before returning to Equinox.
//
// Autosaves run on a worker thread (AsyncWriter) from a copy of the SRAM, so the frame
// loop only hashes and copies. That matters for Super FX carts, whose SRAM doubles as GSU
// work RAM and changes every frame. flush() is synchronous: it returns once the latest
// contents are on storage (or the write failed).
class SramSaver {
public:
    void attach(Core* core, std::string path) {
        core_ = core;
        path_ = std::move(path);
        auto ram = core_->saveRam();
        // A candidate of exactly ram.size() bytes wins, so a short file left by a power
        // cut never beats a complete .bak.
        if (!ram.empty())
            if (auto saved = readFileWithRecovery(path_, ram.size())) {
                const size_t n = std::min(ram.size(), saved->size());
                std::memcpy(ram.data(), saved->data(), n);
            }
        lastHash_ = hash(core_->saveRam());
        lastWrite_ = SDL_GetTicks();
        writer_ = std::make_unique<AsyncWriter>(path_, [](const std::string& p, std::span<const uint8_t> d) {
            return writeFileAtomic(p, d);
        });
    }
    // Waits for the last write, then stops the worker.
    void detach() {
        writer_.reset();
        core_ = nullptr;
    }

    // Called once per frame.
    void tick() {
        if (!core_ || ++frames_ < 60) return;
        frames_ = 0;
        checkFailure();
        if (SDL_GetTicks() - lastWrite_ < 5000) return;
        submitIfChanged();
    }
    // Writes now if the content changed, and waits until it is done.
    void flush() {
        if (!core_) return;
        submitIfChanged();
        writer_->waitIdle();
        checkFailure();
    }

private:
    static uint64_t hash(std::span<const uint8_t> d) {
        uint64_t h = 1469598103934665603ull;  // FNV-1a
        for (uint8_t b : d) h = (h ^ b) * 1099511628211ull;
        return h;
    }
    void submitIfChanged() {
        auto ram = core_->saveRam();
        if (ram.empty()) return;
        const uint64_t h = hash(ram);
        if (h == lastHash_) return;
        lastWrite_ = SDL_GetTicks();
        lastHash_ = h;
        writer_->submit(std::vector<uint8_t>(ram.begin(), ram.end()));
    }
    void checkFailure() {
        if (!writer_->takeFailure()) return;
        std::fprintf(stderr, "could not save %s\n", path_.c_str());
        lastHash_ = ~hash(core_->saveRam());  // force a retry on the next trigger
        g_toasts.saveFailed(g_lang);
    }
    Core* core_ = nullptr;
    std::string path_;
    std::unique_ptr<AsyncWriter> writer_;
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


// Docked, the runner renders 1920x1080; handheld, 1280x720.
static bool isDocked() {
#ifdef __SWITCH__
    return appletGetOperationMode() == AppletOperationMode_Console;
#else
    return false;
#endif
}

static void screenSize(bool docked, int& w, int& h) {
    w = docked ? 1920 : 1280;
    h = docked ? 1080 : 720;
}

// Called once per frame: follows the console into and out of the dock.
static void followDock(SDL_Window* window, Video& video, bool& docked) {
    const bool now = isDocked();
    if (now == docked) return;
    docked = now;
    int w = 0, h = 0;
    screenSize(docked, w, h);
    SDL_SetWindowSize(window, w, h);
    video.setScreen(w, h);
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(video.renderer(), &ow, &oh);
    std::fprintf(stderr, "%s: screen %dx%d, renderer output %dx%d\n",
                 docked ? "docked" : "handheld", w, h, ow, oh);
}

static bool fileExists(const std::string& path) {
    if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
        std::fclose(f);
        return true;
    }
    return false;
}

// Full-screen error. With a back button, A ends it and main returns, which opens
// Equinox. Without one (nothing to return to) it closes by itself after 8 s or on A/B.
static void showError(Video& video, Input& input, MenuView& view, Str msg, bool withBack) {
    const Uint32 start = SDL_GetTicks();
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return;
            input.handleEvent(e);
        }
        const auto edges = input.poll();
        if (edges.confirm || (!withBack && edges.back)) return;
        if (!withBack && SDL_GetTicks() - start > 8000) return;
        view.drawError(tr(g_lang, msg), withBack, g_lang);
        video.flip();
    }
}

static Str romErrorText(RomError e) {
    switch (e) {
        case RomError::NotFound: return Str::RomNotFound;
        case RomError::Unreadable: return Str::RomUnreadable;
        case RomError::NoRomInZip: return Str::NoRomInZip;
        case RomError::Unsupported: return Str::RomUnsupported;
        case RomError::TooBig: return Str::RomTooBig;
        default: return Str::RomUnreadable;
    }
}

// The menu, savestates and the frame loop.
static void gameLoop(const LaunchArgs& args, SDL_Window* window, bool docked, Video& video,
                     Input& input, Audio& audio, Core& core, MenuView& view) {
    const std::string stateFile = statePath(args.states, args.rom);
    const std::string title = args.title.empty() ? romStem(args.rom) : args.title;
    Menu menu;
    bool running = true;

    auto openMenu = [&] {
        menu.setHasState(fileExists(stateFile));
        menu.open();
        audio.pause(true);
    };
    auto closeMenu = [&] {
        menu.close();
        audio.pause(false);
        input.suppressHeld();  // the closing A/B must not reach the game
        g_sram.flush();  // menu exit is one of the SRAM flush triggers
    };
    auto saveState = [&] {
        const auto st = core.serialize();
        if (!st.empty() && writeFileAtomic(stateFile, st)) {
            menu.setHasState(true);
            g_toasts.show(tr(g_lang, Str::StateSaved));
        } else {
            std::fprintf(stderr, "could not write state %s\n", stateFile.c_str());
            g_toasts.show(tr(g_lang, Str::SaveFailed));  // user action: always report
        }
    };
    auto loadState = [&] {
        const auto st = readFile(stateFile);
        if (st && core.unserialize(*st)) {
            g_toasts.show(tr(g_lang, Str::StateLoaded));
            closeMenu();
        } else {
            std::fprintf(stderr, "could not load state %s\n", stateFile.c_str());
            g_toasts.show(tr(g_lang, Str::NoState));
        }
    };

    while (running) {
        followDock(window, video, docked);
        bool touched = false;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_APP_WILLENTERBACKGROUND || e.type == SDL_APP_TERMINATING) g_sram.flush();
            if (e.type == SDL_FINGERDOWN) touched = true;
            input.handleEvent(e);
        }
        const auto edges = input.poll();
        const bool padLost = input.takePadLost();

        if (!menu.isOpen()) {
            if (edges.menuCombo || touched || padLost) openMenu();
        } else {
            MenuAction act = MenuAction::None;
            if (edges.up) menu.up();
            if (edges.down) menu.down();
            if (edges.confirm) act = menu.confirm();
            else if (edges.back) act = menu.back();
            switch (act) {
                case MenuAction::Close: closeMenu(); break;
                case MenuAction::SaveState: saveState(); break;
                case MenuAction::LoadState: loadState(); break;
                case MenuAction::OpenVideo: break;  // wired in Task 7
                case MenuAction::Exit:
                    g_sram.flush();
                    if (!scheduleReturn(args))
                        std::fprintf(stderr, "scheduleReturn failed: loader cannot chain-load\n");
                    running = false;
                    break;
                case MenuAction::None: break;
            }
        }

        if (menu.isOpen()) {
            video.drawGame();
            view.drawMenu(menu, title, g_lang);
        } else {
            core.runFrame();
            g_sram.tick();
            video.drawGame();
        }
        if (g_toasts.active()) view.drawToast(g_toasts.text());
        video.flip();  // vsync paces one frame per refresh
    }
    g_sram.flush();  // exit: before the scheduled return to Equinox fires
}

// While the core is attached to the SRAM saver: on every way out of the session,
// exceptions included, unhook the applet callback, write the SRAM and detach.
class SramSession {
public:
    SramSession(Core& core, const std::string& path) {
        g_sram.attach(&core, path);
#ifdef __SWITCH__
        appletHook(&g_hookCookie, appletHookFn, nullptr);
#endif
    }
    ~SramSession() {
#ifdef __SWITCH__
        appletUnhook(&g_hookCookie);
#endif
        try {
            g_sram.flush();
        } catch (...) {
        }
        g_sram.detach();
    }
    SramSession(const SramSession&) = delete;
    SramSession& operator=(const SramSession&) = delete;
};

static void run(const std::optional<LaunchArgs>& args, bool showFps, SDL_Window*& window) {
    const bool docked = isDocked();
    int screenW = 0, screenH = 0;
    screenSize(docked, screenW, screenH);
    window = SDL_CreateWindow("Equinox SNES", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenW,
                              screenH, SDL_WINDOW_SHOWN);
    Video video;
    Input input;
    Audio audio;
    Core core;
    MenuView view;
    if (!window || !video.init(window, showFps)) {
        std::fprintf(stderr, "video init failed\n");
        return;
    }
    input.init();
    if (!view.init(video.renderer())) std::fprintf(stderr, "menu text unavailable\n");

    if (!args) {
        showError(video, input, view, Str::OpenFromEquinox, false);
    } else {
        auto loaded = loadRom(args->rom);
        if (auto* err = std::get_if<RomError>(&loaded)) {
            std::fprintf(stderr, "rom error %d: %s\n", static_cast<int>(*err), args->rom.c_str());
            showError(video, input, view, romErrorText(*err), true);
        } else {
            const Rom& rom = std::get<Rom>(loaded);
            core.onVideo = [&](const void* px, unsigned w, unsigned h, size_t pitch) {
                video.upload(px, w, h, pitch);
            };
            core.onInput = [&](unsigned port, unsigned id) { return input.state(port, id); };
            if (!core.load(rom, args->saves, args->rom)) {
                std::fprintf(stderr, "core failed to load the ROM\n");
                showError(video, input, view, Str::RomUnsupported, true);
            } else {
                if (audio.init(core.sampleRate(), core.fps()))
                    core.onAudio = [&](const int16_t* lr, size_t frames) { audio.push(lr, frames); };
                else
                    std::fprintf(stderr, "audio init failed; running silent\n");
                video.setSettings(resolve(loadPrefs(args->video),
                                          loadPrefs(videoPrefsPath(args->saves, args->rom))));
                // Decoded and blurred once, before the first frame.
                if (!args->cover.empty())
                    video.setCover(blurCover(loadPng(args->cover), screenW / 8, screenH / 8));
                {
                    SramSession session(core, srmPath(args->saves, args->rom));
                    gameLoop(*args, window, docked, video, input, audio, core, view);
                }
                audio.shutdown();
                core.unload();
            }
        }
    }
    view.shutdown();
    input.shutdown();
}

int main(int argc, char** argv) {
    // Under nxlink (development only), send stdout/stderr to the PC running `nxlink -s`.
    // The socket is left open until the process ends: closing it here crashed the exit
    // when `nxlink -s` was still listening, since stdout/stderr still pointed at it.
    if (__nxlink_host.s_addr != 0 && R_SUCCEEDED(socketInitializeDefault())) {
        if (nxlinkStdio() < 0) socketExit();
    }

    std::optional<LaunchArgs> args;
    try {
        args = parseArgs(argc, argv);
        // Once launched by Equinox, any exit (menu, error, HOME, an exception) reopens it.
        if (args && !scheduleReturn(*args)) std::fprintf(stderr, "scheduleReturn failed at start\n");
    } catch (...) {
        std::fprintf(stderr, "could not read the launch arguments\n");
    }

    bool showFps = false;
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--show-fps")) showFps = true;

    g_lang = systemLang();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL_Init failed\n");
        return 0;
    }
    SDL_Window* window = nullptr;
    // No exception may reach std::terminate: that would abort without reopening Equinox.
    // Unwinding runs SramSession first, so the battery save is written on the way out.
    try {
        run(args, showFps, window);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
    } catch (...) {
        std::fprintf(stderr, "fatal: unknown exception\n");
    }
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
