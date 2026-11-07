#include "audio_resampler.hpp"

#include <cmath>
#include <vector>

#include "doctest/doctest.h"

namespace {
std::vector<int16_t> ramp(int start, size_t frames) {
    std::vector<int16_t> v(frames * 2);
    for (size_t i = 0; i < frames; ++i) {
        v[i * 2] = static_cast<int16_t>(start + static_cast<int>(i));
        v[i * 2 + 1] = static_cast<int16_t>(-(start + static_cast<int>(i)));
    }
    return v;
}

struct SimResult {
    size_t underruns = 0;    // after the first prefill
    size_t overflowed = 0;   // frames the ring had to drop
    double minAdjust = 1.0, maxAdjust = 1.0;
};

// Event-driven model of the device: the core pushes one frame of samples per 60 Hz
// vsync (producerRate samples/s), the SDL callback pulls 1024 frames at 48 kHz.
// Same geometry as platform/audio.cpp: 150 ms ring, 60 ms target, +/-0.5 %.
SimResult simulate(double producerRate, double seconds, std::vector<long> hitchFrames = {}) {
    const double inRate = 31987.0, outRate = 48000.0;
    const auto cap = static_cast<size_t>(inRate * 0.150);
    AudioRing ring(cap);
    AudioResampler rs(inRate, outRate, static_cast<size_t>(inRate * 0.060), cap);
    std::vector<int16_t> frame(2000 * 2, 100), out(1024 * 2);
    double tw = 0.05, tr = 0.0, owed = 0.0;  // the first core frame lands 50 ms after the device starts
    const double dtw = 1.0 / 60.0, dtr = 1024.0 / outRate;
    long frameNo = 0;
    bool started = false;
    SimResult r;
    while (tr < seconds) {
        if (tw <= tr) {
            ++frameNo;
            // A hitch: this core frame lands one refresh late (a missed vsync); the loop
            // produces one frame fewer.
            for (long h : hitchFrames)
                if (frameNo == h) tw += dtw;
            owed += producerRate / 60.0;
            const auto n = static_cast<size_t>(owed);
            owed -= static_cast<double>(n);
            r.overflowed += n - ring.write(frame.data(), n);
            tw += dtw;
            continue;
        }
        const size_t real = rs.render(ring, out.data(), 1024);
        if (real > 0) started = true;
        if (started && real < 1024) ++r.underruns;
        if (started) {
            r.minAdjust = std::min(r.minAdjust, rs.lastAdjust());
            r.maxAdjust = std::max(r.maxAdjust, rs.lastAdjust());
        }
        tr += dtr;
    }
    return r;
}
}  // namespace

TEST_CASE("resampler stays silent and consumes nothing until the target is reached") {
    AudioRing ring(64);
    AudioResampler rs(32000, 48000, 16, 64);
    auto in = ramp(1, 15);
    ring.write(in.data(), 15);
    std::vector<int16_t> out(8 * 2, 99);
    CHECK(rs.render(ring, out.data(), 8) == 0);
    for (int16_t s : out) CHECK(s == 0);
    CHECK(ring.available() == 15);
    CHECK(rs.waiting());
    ring.write(in.data(), 1);
    CHECK(rs.render(ring, out.data(), 8) == 8);
    CHECK_FALSE(rs.waiting());
}

TEST_CASE("resampler at 1:1 passes samples through in order") {
    AudioRing ring(64);
    AudioResampler rs(48000, 48000, 8, 64, 0.0);
    auto in = ramp(10, 8);
    ring.write(in.data(), 8);
    std::vector<int16_t> out(6 * 2);
    CHECK(rs.render(ring, out.data(), 6) == 6);
    // Two frames of start-up latency (the interpolator starts between silent frames).
    const std::vector<int16_t> want = {0, 0, 0, 0, 10, -10, 11, -11, 12, -12, 13, -13};
    CHECK(out == want);
    CHECK(ring.available() == 2);
}

TEST_CASE("resampler interpolates linearly between input frames") {
    AudioRing ring(64);
    AudioResampler rs(24000, 48000, 4, 64, 0.0);  // step 0.5
    std::vector<int16_t> in = {0, 0, 100, -100, 200, -200, 300, -300};
    ring.write(in.data(), 4);
    std::vector<int16_t> out(8 * 2);
    rs.render(ring, out.data(), 8);
    // Pairs (a,b) start silent: (0,0) (0,0) (0,in0=0) (0,100) (100,200), two outputs each.
    const std::vector<int16_t> left = {0, 0, 0, 0, 0, 50, 100, 150};
    for (size_t i = 0; i < 8; ++i) {
        CHECK(out[i * 2] == left[i]);
        CHECK(out[i * 2 + 1] == -left[i]);
    }
}

TEST_CASE("resampler consumes faster when the ring is fuller than the target, clamped") {
    AudioRing ring(4000);
    AudioResampler rs(32000, 48000, 1000, 4000);
    std::vector<int16_t> buf(4000 * 2, 1), out(64 * 2);
    ring.write(buf.data(), 1500);
    rs.render(ring, out.data(), 64);
    CHECK(rs.lastAdjust() == doctest::Approx(1.0025));  // 50 % above target: +0.25 %
    ring.write(buf.data(), 4000);                       // fills to capacity: 300 % above
    rs.render(ring, out.data(), 64);
    CHECK(rs.lastAdjust() == doctest::Approx(1.005));  // clamped at +0.5 %
}

TEST_CASE("resampler consumes slower when the ring is below the target") {
    AudioRing ring(4000);
    AudioResampler rs(32000, 48000, 1000, 4000);
    std::vector<int16_t> buf(4000 * 2, 1), out(64 * 2);
    ring.write(buf.data(), 1000);
    rs.render(ring, out.data(), 64);  // starts at the target
    CHECK(rs.lastAdjust() == doctest::Approx(1.0));
    std::vector<int16_t> big(800 * 2);
    rs.render(ring, big.data(), 800);  // drains ~533 frames
    rs.render(ring, out.data(), 64);
    CHECK(rs.lastAdjust() < 1.0);
    CHECK(rs.lastAdjust() >= 0.995);
}

TEST_CASE("resampler underrun plays what it has, pads silence and prefills again") {
    AudioRing ring(64);
    AudioResampler rs(48000, 48000, 4, 64, 0.0);
    auto in = ramp(1, 4);
    ring.write(in.data(), 4);
    std::vector<int16_t> out(10 * 2, 99);
    const size_t real = rs.render(ring, out.data(), 10);
    CHECK(real == 4);
    for (size_t i = real * 2; i < out.size(); ++i) CHECK(out[i] == 0);
    CHECK(rs.underruns() == 1);
    CHECK(rs.waiting());
    CHECK(ring.available() == 0);
}

TEST_CASE("simulated play: no underruns or overflows with matched clocks") {
    const auto r = simulate(31987.0, 60.0);
    CHECK(r.underruns == 0);
    CHECK(r.overflowed == 0);
}

TEST_CASE("simulated play: no underruns with the producer 0.02 % slow or fast") {
    for (double drift : {-0.0002, 0.0002}) {
        CAPTURE(drift);
        const auto r = simulate(31987.0 * (1.0 + drift), 120.0);
        CHECK(r.underruns == 0);
        CHECK(r.overflowed == 0);
    }
}

TEST_CASE("simulated play: absorbs the nominal 32040 Hz (0.17 % fast) and 0.1 % slow") {
    for (double rate : {32040.0, 31987.0 * 0.999}) {
        CAPTURE(rate);
        const auto r = simulate(rate, 120.0);
        CHECK(r.underruns == 0);
        CHECK(r.overflowed == 0);
        CHECK(r.maxAdjust <= 1.005);
        CHECK(r.minAdjust >= 0.995);
    }
}

TEST_CASE("simulated play: isolated missed vsyncs do not underrun, at any phase") {
    // 20 s apart, so the ring recovers in between; each lands at a different phase
    // relative to the 1024-frame callback.
    const auto r = simulate(31987.0 * 0.9998, 125.0, {600, 1807, 3014, 4221, 5428, 6635});
    CHECK(r.underruns == 0);
    CHECK(r.overflowed == 0);
}
