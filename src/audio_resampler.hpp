#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "audio_ring.hpp"

// Consumer side of the audio path (runs on the audio callback thread). Pulls interleaved
// stereo int16 frames from an AudioRing and linearly resamples them to the output rate.
//
// - Prefill: after construction and restart() it outputs silence without consuming until
//   the ring holds targetFrames, so playback starts with a cushion.
// - Dynamic rate control: the resample step is inRate/outRate scaled by up to
//   +/-maxAdjust, in proportion to how far the ring level is from targetFrames (fuller
//   ring: consume faster). Independent display and audio clocks then cannot drain or
//   overflow the ring.
// - Underrun: the frames that could be made are played, the rest of the buffer is
//   silence that never enters the ring, and it prefills again.
class AudioResampler {
public:
    AudioResampler(double inRate, double outRate, size_t targetFrames, size_t maxInFrames,
                   double maxAdjust = 0.005);

    // Writes `frames` output frames to `out`. Returns how many carried real audio.
    size_t render(AudioRing& ring, int16_t* out, size_t frames);
    void restart() { waiting_ = true; }

    bool waiting() const { return waiting_; }
    double lastAdjust() const { return lastAdjust_; }  // step / (inRate/outRate)
    size_t underruns() const { return underruns_; }

private:
    double base_, maxAdjust_, frac_ = 0.0, lastAdjust_ = 1.0;
    size_t target_, underruns_ = 0;
    bool waiting_ = true;
    int16_t a_[2] = {0, 0}, b_[2] = {0, 0};  // input frames around the read position
    std::vector<int16_t> in_;
};
