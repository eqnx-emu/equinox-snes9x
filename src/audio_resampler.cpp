#include "audio_resampler.hpp"

#include <algorithm>
#include <cstring>

AudioResampler::AudioResampler(double inRate, double outRate, size_t targetFrames, size_t maxInFrames,
                               double maxAdjust)
    : base_(inRate / outRate), maxAdjust_(maxAdjust), target_(std::max<size_t>(targetFrames, 1)),
      in_(maxInFrames * 2) {}

size_t AudioResampler::render(AudioRing& ring, int16_t* out, size_t frames) {
    const size_t avail = std::min(ring.available(), in_.size() / 2);
    if (waiting_) {
        if (avail < target_) {
            std::memset(out, 0, frames * 2 * sizeof(int16_t));
            return 0;
        }
        waiting_ = false;
    }

    const double dev = std::clamp((static_cast<double>(avail) - static_cast<double>(target_)) /
                                      static_cast<double>(target_),
                                  -1.0, 1.0);
    lastAdjust_ = 1.0 + maxAdjust_ * dev;
    const double step = base_ * lastAdjust_;

    // How many output frames the available input covers. Same arithmetic as the loop
    // below, so exactly `used` frames are consumed.
    size_t n = 0, used = 0;
    double f = frac_;
    while (n < frames) {
        const double nf = f + step;
        const size_t k = static_cast<size_t>(nf);
        if (used + k > avail) break;
        used += k;
        f = nf - static_cast<double>(k);
        ++n;
    }
    ring.read(in_.data(), used);

    size_t idx = 0;
    f = frac_;
    for (size_t i = 0; i < n; ++i) {
        out[i * 2] = static_cast<int16_t>(a_[0] + (b_[0] - a_[0]) * f);
        out[i * 2 + 1] = static_cast<int16_t>(a_[1] + (b_[1] - a_[1]) * f);
        const double nf = f + step;
        const size_t k = static_cast<size_t>(nf);
        for (size_t j = 0; j < k; ++j, ++idx) {
            a_[0] = b_[0];
            a_[1] = b_[1];
            b_[0] = in_[idx * 2];
            b_[1] = in_[idx * 2 + 1];
        }
        f = nf - static_cast<double>(k);
    }
    frac_ = f;

    if (n < frames) {
        std::memset(out + n * 2, 0, (frames - n) * 2 * sizeof(int16_t));
        ++underruns_;
        waiting_ = true;
    }
    return n;
}
