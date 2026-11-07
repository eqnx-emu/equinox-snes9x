#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "rom.hpp"

// Thin wrapper over the statically linked libretro Snes9x core. Only one instance
// can be loaded at a time (libretro callbacks are plain C functions).
class Core {
public:
    ~Core();

    // retro_init + retro_load_game, handing the ROM over as data. systemDir is the
    // saves folder (Snes9x needs no BIOS). romPath only names the game to the core.
    bool load(const Rom& rom, const std::string& systemDir, const std::string& romPath = {});
    void runFrame();  // retro_run
    void unload();

    double fps() const { return fps_; }                // ~60.0988
    double sampleRate() const { return sampleRate_; }  // ~32040
    std::span<uint8_t> saveRam();                      // empty when the game has none
    std::vector<uint8_t> serialize();
    bool unserialize(std::span<const uint8_t> state);
    // The Blargg NTSC composite filter (snes9x_blargg). Takes effect on the next frame;
    // before load(), from the first one.
    void setNtsc(bool on);

    // Wired by the platform layer.
    std::function<void(const void* px, unsigned w, unsigned h, size_t pitch)> onVideo;
    std::function<void(const int16_t* lr, size_t frames)> onAudio;
    std::function<int16_t(unsigned port, unsigned id)> onInput;  // id: RETRO_DEVICE_ID_JOYPAD_*

private:
    bool loaded_ = false;
    double fps_ = 60.0988;
    double sampleRate_ = 32040.0;
};
