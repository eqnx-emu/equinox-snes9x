#include <vector>

#include "audio_ring.hpp"
#include "doctest/doctest.h"

static std::vector<int16_t> seq(int startFrame, size_t frames) {
    std::vector<int16_t> v(frames * 2);
    for (size_t i = 0; i < frames; ++i) {
        v[i * 2] = static_cast<int16_t>(startFrame + i);
        v[i * 2 + 1] = static_cast<int16_t>(-(startFrame + static_cast<int>(i)));
    }
    return v;
}

TEST_CASE("write then read returns same samples") {
    AudioRing ring(8);
    auto in = seq(1, 5);
    CHECK(ring.write(in.data(), 5) == 5);
    CHECK(ring.available() == 5);
    std::vector<int16_t> out(10, 99);
    CHECK(ring.read(out.data(), 5) == 5);
    CHECK(out == in);
    CHECK(ring.available() == 0);
}

TEST_CASE("overflow keeps oldest and reports frames written") {
    AudioRing ring(4);
    auto in = seq(1, 6);
    CHECK(ring.write(in.data(), 6) == 4);
    CHECK(ring.available() == 4);
    std::vector<int16_t> out(8);
    CHECK(ring.read(out.data(), 4) == 4);
    CHECK(out == seq(1, 4));
}

TEST_CASE("underflow fills silence") {
    AudioRing ring(8);
    auto in = seq(1, 2);
    ring.write(in.data(), 2);
    std::vector<int16_t> out(8, 99);
    CHECK(ring.read(out.data(), 4) == 2);
    CHECK(out[0] == 1);
    CHECK(out[2] == 2);
    for (size_t i = 4; i < 8; ++i) CHECK(out[i] == 0);
}

TEST_CASE("wraps around the end") {
    AudioRing ring(4);
    auto a = seq(1, 3);
    ring.write(a.data(), 3);
    std::vector<int16_t> out(8);
    ring.read(out.data(), 3);
    auto b = seq(10, 3);  // crosses the end of the storage
    CHECK(ring.write(b.data(), 3) == 3);
    CHECK(ring.read(out.data(), 3) == 3);
    out.resize(6);
    CHECK(out == b);
}
