#include "thumbnail.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

#include "doctest/doctest.h"
#include "miniz/miniz.h"

namespace {
uint16_t rgb565(int r, int g, int b) { return static_cast<uint16_t>((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)); }

uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

// The PNG's pixels: concatenated IDAT, inflated, one filter byte per row.
std::vector<uint8_t> pixelsOf(const std::vector<uint8_t>& png, unsigned& w, unsigned& h) {
    w = be32(&png[16]);
    h = be32(&png[20]);
    std::vector<uint8_t> z;
    for (size_t p = 8; p + 8 <= png.size();) {
        const uint32_t len = be32(&png[p]);
        if (std::memcmp(&png[p + 4], "IDAT", 4) == 0) z.insert(z.end(), &png[p + 8], &png[p + 8 + len]);
        p += 12 + len;
    }
    std::vector<uint8_t> raw(static_cast<size_t>(h) * (1 + w * 3));
    mz_ulong n = static_cast<mz_ulong>(raw.size());
    REQUIRE(mz_uncompress(raw.data(), &n, z.data(), static_cast<mz_ulong>(z.size())) == MZ_OK);
    return raw;
}
}  // namespace

TEST_CASE("a flat frame shrinks to the same colour") {
    std::vector<uint16_t> frame(256 * 224, rgb565(255, 0, 0));
    const auto t = thumbnailRgb(frame.data(), 256, 224, 160, 120);
    REQUIRE(t.size() == 160 * 120 * 3);
    CHECK(t[0] == 255);
    CHECK(t[1] == 0);
    CHECK(t[2] == 0);
    CHECK(t[t.size() - 3] == 255);
}

TEST_CASE("each output pixel averages its block") {
    // 4x2 frame: left half white, right half black, into 2x1.
    const uint16_t W = rgb565(255, 255, 255), K = 0;
    const std::vector<uint16_t> frame = {W, W, K, K, W, W, K, K};
    const auto t = thumbnailRgb(frame.data(), 4, 2, 2, 1);
    REQUIRE(t.size() == 6);
    CHECK(t[0] == 255);
    CHECK(t[3] == 0);
    // 2x1 into 1x1: the mean of white and black.
    const std::vector<uint16_t> two = {W, K};
    const auto m = thumbnailRgb(two.data(), 2, 1, 1, 1);
    CHECK(m[0] >= 126);
    CHECK(m[0] <= 129);
}

TEST_CASE("an empty frame gives no thumbnail") {
    CHECK(thumbnailRgb(nullptr, 0, 0, 160, 120).empty());
    CHECK(encodePng({}, 0, 0).empty());
}

TEST_CASE("the PNG holds the thumbnail") {
    std::vector<uint8_t> rgb(4 * 3 * 3, 0);
    rgb[0] = 10;
    rgb[1] = 20;
    rgb[2] = 30;
    rgb[rgb.size() - 1] = 200;
    const auto png = encodePng(rgb, 4, 3);
    REQUIRE(png.size() > 33);
    CHECK(std::memcmp(png.data(), "\x89PNG\r\n\x1a\n", 8) == 0);
    unsigned w = 0, h = 0;
    const auto raw = pixelsOf(png, w, h);
    CHECK(w == 4);
    CHECK(h == 3);
    CHECK(raw[1] == 10);  // row 0, after its filter byte
    CHECK(raw[2] == 20);
    CHECK(raw[3] == 30);
    CHECK(raw[raw.size() - 1] == 200);
}
