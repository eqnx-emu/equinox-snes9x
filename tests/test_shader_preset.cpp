#include "shader_preset.hpp"

#include "doctest/doctest.h"

namespace {
constexpr const char* kNtscAdaptive = R"(shaders = 2

shader0 = shaders/ntsc-adaptive/ntsc-pass1.glsl
scale_type0 = source
scale_x0 = 4.0
filter_linear0 = false
scale_y0 = 1.0
float_framebuffer0 = true

shader1 = shaders/ntsc-adaptive/ntsc-pass2.glsl
scale_type1 = source
scale_x1 = 0.5
scale_y1 = 1.0
filter_linear1 = false
wrap_mode1 = mirrored_repeat
)";
}  // namespace

TEST_CASE("ntsc-adaptive reads as two source-scaled passes") {
    const auto p = parsePreset(kNtscAdaptive, "romfs:/shaders/ntsc");
    REQUIRE(p);
    REQUIRE(p->passes.size() == 2);
    const PassSpec& a = p->passes[0];
    CHECK(a.shader == "romfs:/shaders/ntsc/shaders/ntsc-adaptive/ntsc-pass1.glsl");
    CHECK(a.scaleTypeX == ScaleType::Source);
    CHECK(a.scaleTypeY == ScaleType::Source);
    CHECK(a.scaleX == 4.0f);
    CHECK(a.scaleY == 1.0f);
    CHECK(a.floatFramebuffer);
    CHECK(a.filterLinear == false);
    CHECK(a.wrap == Wrap::ClampToEdge);
    const PassSpec& b = p->passes[1];
    CHECK(b.wrap == Wrap::MirroredRepeat);
    CHECK(b.scaleX == 0.5f);
    CHECK_FALSE(b.floatFramebuffer);
}

TEST_CASE("crt-geom is one pass with no scale type") {
    const auto p = parsePreset("shaders = 1\n\nshader0 = shaders/crt-geom.glsl\nfilter_linear0 = false\n", "d");
    REQUIRE(p);
    REQUIRE(p->passes.size() == 1);
    CHECK_FALSE(p->passes[0].scaleTypeX);
    CHECK_FALSE(p->passes[0].scaleTypeY);
    CHECK(p->passes[0].shader == "d/shaders/crt-geom.glsl");
}

TEST_CASE("a preset with textures is refused") {
    CHECK_FALSE(parsePreset("shaders = 1\nshader0 = a.glsl\ntextures = \"LUT\"\nLUT = lut.png\n", "d"));
}

TEST_CASE("a missing shader or count is refused") {
    CHECK_FALSE(parsePreset("shaders = 2\nshader0 = a.glsl\n", "d"));
    CHECK_FALSE(parsePreset("shader0 = a.glsl\n", "d"));
    CHECK_FALSE(parsePreset("shaders = 0\n", "d"));
}

TEST_CASE("quoted values, comments, per-axis keys and ignored keys") {
    const auto p = parsePreset(
        "# a comment\n"
        "shaders = \"1\"\n"
        "shader0 = \"a.glsl\"  # trailing\n"
        "scale_type_x0 = viewport\n"
        "scale_type_y0 = absolute\n"
        "scale_x0 = 0.5\n"
        "scale_y0 = 240\n"
        "wrap_mode0 = clamp_to_border\n"
        "srgb_framebuffer0 = true\n"
        "mipmap_input0 = true\n"
        "alias0 = Foo\n"
        "frame_count_mod0 = 2\n"
        "parameters = \"A;B\"\n"
        "A = 0.5\n"
        "B = \"2\"\n",
        "d");
    REQUIRE(p);
    CHECK(p->passes[0].shader == "d/a.glsl");
    CHECK(p->passes[0].scaleTypeX == ScaleType::Viewport);
    CHECK(p->passes[0].scaleTypeY == ScaleType::Absolute);
    CHECK(p->passes[0].scaleY == 240.0f);
    CHECK(p->passes[0].wrap == Wrap::ClampToEdge);
    CHECK_FALSE(p->passes[0].filterLinear);
    REQUIRE(p->parameters.size() == 2);
    CHECK(p->parameters[0].first == "A");
    CHECK(p->parameters[0].second == 0.5f);
    CHECK(p->parameters[1].first == "B");
    CHECK(p->parameters[1].second == 2.0f);
}

TEST_CASE("scale applies to both axes and repeat wraps") {
    const auto p = parsePreset("shaders = 1\nshader0 = a.glsl\nscale_type0 = source\nscale0 = 2\n"
                               "wrap_mode0 = repeat\nfilter_linear0 = true\n",
                               "d");
    REQUIRE(p);
    CHECK(p->passes[0].scaleX == 2.0f);
    CHECK(p->passes[0].scaleY == 2.0f);
    CHECK(p->passes[0].wrap == Wrap::Repeat);
    CHECK(p->passes[0].filterLinear == true);
}
