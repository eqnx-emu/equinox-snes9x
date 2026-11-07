#include "video_art.hpp"

#include "doctest/doctest.h"

TEST_CASE("backdrop uses only the three equinox colors") {
    auto img = equinoxBackdrop(320, 180);
    REQUIRE(img.px.size() == 320u * 180u);
    for (auto c : img.px) CHECK((c == 0xFF3A2F52u || c == 0xFF1B1828u || c == 0xFFB5A8E0u));
}

TEST_CASE("backdrop is lighter at the top than at the bottom") {
    auto img = equinoxBackdrop(320, 180);
    auto light = [&](int y) { int n = 0; for (int x = 0; x < 320; x++) n += img.px[y * 320 + x] == 0xFF3A2F52u; return n; };
    CHECK(light(2) > light(177));
}

TEST_CASE("backdrop is the same every time") {
    CHECK(equinoxBackdrop(64, 36).px == equinoxBackdrop(64, 36).px);
}

TEST_CASE("backdrop has loose stars only in the top half") {
    auto img = equinoxBackdrop(320, 180);
    int top = 0, bottom = 0;
    for (int y = 0; y < 180; y++)
        for (int x = 0; x < 320; x++)
            if (img.px[y * 320 + x] == 0xFFB5A8E0u) (y < 90 ? top : bottom)++;
    CHECK(top > 0);
    CHECK(bottom == 0);
}

TEST_CASE("backdrop of an empty size is empty") {
    CHECK(equinoxBackdrop(0, 10).px.empty());
}

TEST_CASE("scanline mask alternates") {
    auto m = scanlineMask(224);
    REQUIRE(m.w == 1); REQUIRE(m.h == 448);
    CHECK((m.px[0] >> 24) == 0); CHECK((m.px[1] >> 24) > 0);
}

TEST_CASE("blurred cover is darker and smooth") {
    Image c{4, 4, std::vector<uint32_t>(16, 0xFFFFFFFFu)}; c.px[0] = 0xFF000000u;
    auto b = blurCover(c, 8, 8);
    REQUIRE(b.px.size() == 64u);
    for (auto p : b.px) CHECK((p & 0xFF) < 0xC0);
}

TEST_CASE("blur spreads a sharp edge") {
    Image c{16, 16, std::vector<uint32_t>(256, 0xFF000000u)};
    for (int y = 0; y < 16; y++)
        for (int x = 8; x < 16; x++) c.px[y * 16 + x] = 0xFFFFFFFFu;
    auto b = blurCover(c, 16, 16);
    auto g = [&](int x) { return b.px[8 * 16 + x] & 0xFF; };
    CHECK(g(7) > g(0));
    CHECK(g(8) < g(15));
    CHECK(g(7) != g(8));
    CHECK(g(0) <= g(7));
}

TEST_CASE("blur covers a wide cover without distorting the center") {
    // 4:1 cover, red in the middle half and blue at the sides; a 2:1 output crops the sides.
    Image c{16, 4, std::vector<uint32_t>(64, 0xFF0000FFu)};
    for (int y = 0; y < 4; y++)
        for (int x = 4; x < 12; x++) c.px[y * 16 + x] = 0xFFFF0000u;
    auto b = blurCover(c, 8, 4);
    REQUIRE(b.w == 8); REQUIRE(b.h == 4);
    CHECK(((b.px[2 * 8 + 4] >> 16) & 0xFF) > (b.px[2 * 8 + 4] & 0xFF));
}

TEST_CASE("blur of an empty cover is empty") { CHECK(blurCover(Image{}, 8, 8).px.empty()); }
