#include "platform/audio.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr int kOutRate = 48000;
constexpr size_t kChunk = 512;
}

Audio::~Audio() { shutdown(); }

bool Audio::init(double coreRate, double coreFps) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;

    // The main loop runs one core frame per display refresh (60.000 Hz), but the core
    // is tuned for coreFps (~60.0988). It therefore produces coreRate * refresh / coreFps
    // samples per second (~31987 Hz), not the nominal ~32040. Feeding the stream that
    // effective rate keeps the ring from slowly draining or overflowing.
    double refresh = 60.0;
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.refresh_rate > 0)
        refresh = mode.refresh_rate;
    const int inRate = static_cast<int>(std::lround(coreRate * refresh / coreFps));

    ring_ = std::make_unique<AudioRing>(static_cast<size_t>(inRate / 10));  // 100 ms
    stream_ = SDL_NewAudioStream(AUDIO_S16SYS, 2, inRate, AUDIO_S16SYS, 2, kOutRate);
    if (!stream_) return false;

    SDL_AudioSpec want{};
    want.freq = kOutRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = &Audio::callback;
    want.userdata = this;
    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (!dev_) {
        shutdown();
        return false;
    }
    SDL_PauseAudioDevice(dev_, 0);
    return true;
}

void Audio::push(const int16_t* lr, size_t frames) {
    if (ring_) ring_->write(lr, frames);
}

void Audio::callback(void* user, Uint8* out, int len) {
    static_cast<Audio*>(user)->fill(out, len);
}

void Audio::fill(Uint8* out, int len) {
    int16_t chunk[kChunk * 2];
    // Underflow reads as silence from the ring, so the stream always has enough.
    while (SDL_AudioStreamAvailable(stream_) < len) {
        ring_->read(chunk, kChunk);
        if (SDL_AudioStreamPut(stream_, chunk, sizeof(chunk)) != 0) break;
    }
    const int got = SDL_AudioStreamGet(stream_, out, len);
    if (got < len) SDL_memset(out + std::max(got, 0), 0, len - std::max(got, 0));
}

void Audio::shutdown() {
    if (dev_) {
        SDL_CloseAudioDevice(dev_);  // joins the callback thread
        dev_ = 0;
    }
    if (stream_) {
        SDL_FreeAudioStream(stream_);
        stream_ = nullptr;
    }
}
