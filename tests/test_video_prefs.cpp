#include "video_prefs.hpp"

#include "doctest/doctest.h"

TEST_CASE("parse reads the four keys") {
    auto p = parsePrefs(R"({"size":"8:7","smooth":"sharp","filter":"scanlines","border":"cover"})");
    CHECK(p.size == Size::Aspect87);
    CHECK(p.smooth == Smooth::Sharp);
    CHECK(p.filter == Filter::Scanlines);
    CHECK(p.border == Border::Cover);
}

TEST_CASE("unknown values and keys are absent") {
    auto p = parsePrefs(R"({"size":"huge","smooth":3,"crt":"on","border":"tv"})");
    CHECK(!p.size);
    CHECK(!p.smooth);
    CHECK(p.border == Border::Tv);
}

TEST_CASE("broken input gives empty prefs") {
    for (auto s : {"", "{", "[]", "null", "{\"size\":", "\xEF\xBB\xBF{\"size\":\"full\"", "garbage",
             "{\"size\":\"full\"} x", "{\"size\":\"full\",}", "{\"a\":{}}"})
        CHECK(parsePrefs(s).empty());
}

TEST_CASE("BOM and whitespace are accepted") {
    CHECK(parsePrefs("\xEF\xBB\xBF \n{ \"size\" : \"full\" }\n").size == Size::Full);
}

TEST_CASE("last duplicate key wins") {
    CHECK(parsePrefs(R"({"size":"full","size":"4:3"})").size == Size::Aspect43);
}

TEST_CASE("escaped quotes and backslashes in strings are read") {
    auto p = parsePrefs(R"({"a\"b\\":"x\"y","size":"full"})");
    CHECK(p.size == Size::Full);
}

TEST_CASE("serialize round-trips and omits absent keys") {
    VideoPrefs p;
    p.filter = Filter::Scanlines;
    CHECK(serializePrefs(p) == R"({"filter":"scanlines"})");
    CHECK(parsePrefs(serializePrefs(p)).filter == Filter::Scanlines);
    CHECK(serializePrefs(VideoPrefs{}) == "{}");
}

TEST_CASE("resolve: game over global over spec defaults") {
    VideoPrefs g;
    g.border = Border::Black;
    g.size = Size::Full;
    VideoPrefs j;
    j.size = Size::Aspect43;
    auto s = resolve(g, j);
    CHECK(s.size == Size::Aspect43);
    CHECK(s.border == Border::Black);
    CHECK(s.smooth == Smooth::Pixel);
    CHECK(s.filter == Filter::None);
}

TEST_CASE("overridesFor keeps only differences") {
    VideoSettings g;
    g.border = Border::Cover;
    VideoSettings e = g;
    e.filter = Filter::Scanlines;
    auto o = overridesFor(g, e);
    CHECK(o.filter == Filter::Scanlines);
    CHECK(!o.size);
    CHECK(!o.smooth);
    CHECK(!o.border);
    CHECK(overridesFor(g, g).empty());
}

TEST_CASE("cover border without cover falls back to equinox") {
    VideoSettings s;
    s.border = Border::Cover;
    CHECK(effectiveBorder(s, false) == Border::Equinox);
    CHECK(effectiveBorder(s, true) == Border::Cover);
}

TEST_CASE("ntsc parses only when enabled") {
    CHECK(parsePrefs(R"({"filter":"ntsc"})").filter.has_value() == kNtscEnabled);
}
