#pragma once

#include <SDL2/SDL.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "audio_resampler.hpp"
#include "audio_ring.hpp"

// SDL audio output. The core's samples go into an AudioRing (main thread); the SDL
// callback pulls them through an AudioResampler, which prefills 60 ms, resamples to
// 48 kHz and nudges the rate to keep the ring at that level.
class Audio {
public:
    ~Audio();

    // coreRate: the core's nominal sample rate; coreFps: its nominal frame rate.
    bool init(double coreRate, double coreFps);
    void push(const int16_t* lr, size_t frames);
    void shutdown();
    // Paused: the device stops pulling (silence while the menu is open). Unpausing
    // prefills again before playing.
    void pause(bool paused);

private:
    static void callback(void* user, Uint8* out, int len);

    SDL_AudioDeviceID dev_ = 0;
    std::unique_ptr<AudioRing> ring_;
    std::unique_ptr<AudioResampler> resampler_;
};
