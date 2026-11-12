#include "shader_layout.hpp"

#include "doctest/doctest.h"

namespace {
bool same(Size2 a, Size2 b) { return a.w == b.w && a.h == b.h; }

Preset onePass() {
    Preset p;
    p.passes.push_back(PassSpec{"a.glsl"});
    return p;
}

Preset ntsc() {
    Preset p;
    PassSpec a{"p1"};
    a.scaleTypeX = a.scaleTypeY = ScaleType::Source;
    a.scaleX = 4;
    PassSpec b{"p2"};
    b.scaleTypeX = b.scaleTypeY = ScaleType::Source;
    b.scaleX = 0.5f;
    p.passes = {a, b};
    return p;
}
}  // namespace

TEST_CASE("a single unscaled pass goes to the viewport") {
    const auto s = passSizes(onePass(), {256, 224}, {1024, 896});
    REQUIRE(s.size() == 1);
    CHECK(same(s[0], {1024, 896}));
    CHECK_FALSE(needsFinalBlit(onePass()));
}

TEST_CASE("ntsc-adaptive sizes from the source and needs a final blit") {
    const auto s = passSizes(ntsc(), {256, 224}, {1280, 720});
    REQUIRE(s.size() == 2);
    CHECK(same(s[0], {1024, 224}));
    CHECK(same(s[1], {512, 224}));
    CHECK(needsFinalBlit(ntsc()));
    const auto hi = passSizes(ntsc(), {512, 448}, {1280, 720});
    CHECK(same(hi[0], {2048, 448}));
    CHECK(same(hi[1], {1024, 448}));
}

TEST_CASE("viewport and absolute scales") {
    Preset p = onePass();
    p.passes[0].scaleTypeX = p.passes[0].scaleTypeY = ScaleType::Viewport;
    p.passes[0].scaleX = p.passes[0].scaleY = 0.5f;
    CHECK(same(passSizes(p, {256, 224}, {1280, 720})[0], {640, 360}));
    CHECK(needsFinalBlit(p));
    p.passes[0].scaleTypeX = ScaleType::Absolute;
    p.passes[0].scaleX = 320;
    CHECK(passSizes(p, {256, 224}, {1280, 720})[0].w == 320);
}

TEST_CASE("unscaled passes before the last stay at source size") {
    Preset p = onePass();
    p.passes.push_back(PassSpec{"b.glsl"});
    const auto s = passSizes(p, {256, 224}, {1024, 896});
    CHECK(same(s[0], {256, 224}));
    CHECK(same(s[1], {1024, 896}));
    CHECK_FALSE(needsFinalBlit(p));
}

TEST_CASE("texture coordinates flip only the game frame") {
    CHECK(passTexCoords(true) == std::array<float, 8>{0, 1, 1, 1, 0, 0, 1, 0});
    CHECK(passTexCoords(false) == std::array<float, 8>{0, 0, 1, 0, 0, 1, 1, 1});
}
