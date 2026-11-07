#pragma once

#include <SDL2/SDL.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "audio_ring.hpp"

// SDL audio output. The core's samples go into a 100 ms AudioRing (main thread);
// the SDL callback pulls them through an SDL_AudioStream that resamples to 48 kHz.
class Audio {
public:
    ~Audio();

    // coreRate: the core's nominal sample rate; coreFps: its nominal frame rate.
    bool init(double coreRate, double coreFps);
    void push(const int16_t* lr, size_t frames);
    void shutdown();
    void pause(bool paused);  // device stops pulling: silence while the menu is open

private:
    static void callback(void* user, Uint8* out, int len);
    void fill(Uint8* out, int len);

    SDL_AudioDeviceID dev_ = 0;
    SDL_AudioStream* stream_ = nullptr;
    std::unique_ptr<AudioRing> ring_;
};
