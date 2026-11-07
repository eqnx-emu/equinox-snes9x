#include "platform/audio.hpp"

#include <cstdio>

namespace {
constexpr int kOutRate = 48000;
}

Audio::~Audio() { shutdown(); }

bool Audio::init(double coreRate, double coreFps) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;

    // The main loop runs one core frame per display refresh (60.000 Hz), but the core
    // is tuned for coreFps (~60.0988). It therefore produces coreRate * refresh / coreFps
    // samples per second (~31987 Hz), not the nominal ~32040. The resampler starts from
    // that rate and its rate control absorbs the remaining clock drift.
    double refresh = 60.0;
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.refresh_rate > 0)
        refresh = mode.refresh_rate;
    const double inRate = coreRate * refresh / coreFps;

    const auto cap = static_cast<size_t>(inRate * 0.150);    // 150 ms ring
    // 60 ms (~3.5 frames) cushion: with 1024-frame callbacks it rides out a missed vsync
    // at any phase with margin (50 ms is the bare minimum; see the host sim test). Two
    // missed vsyncs in a row need ~70 ms.
    const auto target = static_cast<size_t>(inRate * 0.060);
    ring_ = std::make_unique<AudioRing>(cap);
    resampler_ = std::make_unique<AudioResampler>(inRate, kOutRate, target, cap);

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
    SDL_PauseAudioDevice(dev_, 0);  // silent, without consuming, until the ring is prefilled
    return true;
}

void Audio::push(const int16_t* lr, size_t frames) {
    if (ring_) ring_->write(lr, frames);
}

void Audio::pause(bool paused) {
    if (!dev_) return;
    if (!paused) {
        SDL_LockAudioDevice(dev_);
        resampler_->restart();
        SDL_UnlockAudioDevice(dev_);
    }
    SDL_PauseAudioDevice(dev_, paused ? 1 : 0);
}

void Audio::callback(void* user, Uint8* out, int len) {
    auto* self = static_cast<Audio*>(user);
    self->resampler_->render(*self->ring_, reinterpret_cast<int16_t*>(out),
                             static_cast<size_t>(len) / (2 * sizeof(int16_t)));
}

void Audio::shutdown() {
    if (dev_) {
        SDL_CloseAudioDevice(dev_);  // joins the callback thread
        dev_ = 0;
        std::fprintf(stderr, "audio: %zu underruns\n", resampler_->underruns());
    }
}
