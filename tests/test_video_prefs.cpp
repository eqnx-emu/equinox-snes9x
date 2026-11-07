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
             "{\"size\":\"full\"} x", "{\"size\":\"full\",}"})
        CHECK(parsePrefs(s).empty());
}

TEST_CASE("broken nested values and escapes give empty prefs") {
    for (auto s : {R"({"size":"full","a":{"b":1,}})", R"({"size":"full","a":[1,]})",
             R"({"size":"full","a":[1})", R"({"size":"full","a":{"b"}})", R"({"size":"full","a":{1:2}})",
             R"({"size":"full","a":"\x"})", R"({"size":"full","a":"\u12"})",
             R"({"size":"full","a":"\u12g4"})", R"({"size":"full","a":tru})",
             R"({"size":"full","a":[[[)", R"({"size":"full","a":}"})"})
        CHECK(parsePrefs(s).empty());
}

TEST_CASE("nested values of unknown keys are skipped") {
    CHECK(parsePrefs(R"({"x":{"a":[1,"}"]},"size":"full"})").size == Size::Full);
    CHECK(parsePrefs(R"({"x":[1,2,{"y":null}],"smooth":"soft"})").smooth == Smooth::Soft);
    CHECK(parsePrefs(R"({"x":{},"y":[],"z":[[{"w":"]"}]],"filter":"scanlines"})").filter == Filter::Scanlines);
    CHECK(parsePrefs(R"({"size":{"v":"full"},"border":"tv"})").border == Border::Tv);
    CHECK(!parsePrefs(R"({"size":{"v":"full"},"border":"tv"})").size);
}

TEST_CASE("nesting is bounded") {
    auto nested = [](int depth) {
        return "{\"x\":" + std::string(depth, '[') + std::string(depth, ']') + ",\"size\":\"full\"}";
    };
    CHECK(parsePrefs(nested(10)).size == Size::Full);
    CHECK(parsePrefs(nested(100000)).empty());
}

TEST_CASE("every JSON escape is read") {
    CHECK(parsePrefs(R"({"note":"a\/b\n\b\f\r\t","border":"tv"})").border == Border::Tv);
    CHECK(parsePrefs("{\"size\":\"\\u0066ull\"}").size == Size::Full);
    CHECK(parsePrefs("{\"n\":\"\\u00e9\\u20AC\\ud83d\\ude00\",\"size\":\"4:3\"}").size == Size::Aspect43);
    CHECK(!parsePrefs(R"({"size":"full\n"})").size);
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

TEST_CASE("updateOverrides stores a change that differs from the global") {
    VideoSettings g;
    g.border = Border::Cover;
    VideoSettings e = g;
    e.filter = Filter::Scanlines;
    auto o = updateOverrides({}, g, e);
    CHECK(o.filter == Filter::Scanlines);
    CHECK(!o.size);
    CHECK(!o.smooth);
    CHECK(!o.border);
    CHECK(updateOverrides({}, g, g).empty());
}

TEST_CASE("updateOverrides keeps an untouched choice that now equals the global") {
    VideoPrefs stored;
    stored.size = Size::Aspect43;
    VideoSettings g;
    g.size = Size::Aspect43;  // the global default moved to the game's choice
    VideoSettings e = g;
    e.smooth = Smooth::Sharp;  // the player only changes smoothing
    auto o = updateOverrides(stored, g, e);
    CHECK(o.size == Size::Aspect43);
    CHECK(o.smooth == Smooth::Sharp);
    g.size = Size::Original;  // the global moves back: the game stays 4:3
    CHECK(resolve(VideoPrefs{.size = g.size}, o).size == Size::Aspect43);
}

TEST_CASE("updateOverrides drops a changed option set back to the global") {
    VideoPrefs stored;
    stored.border = Border::Black;
    VideoSettings g;
    VideoSettings e = g;  // border changed from black back to the global equinox
    CHECK(updateOverrides(stored, g, e).empty());
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

#include <filesystem>
#include <fstream>
#include <string>

namespace {
namespace fs = std::filesystem;
fs::path prefsDir() {
    static fs::path d = [] {
        auto p = fs::temp_directory_path() / "eqx_test_video_prefs";
        fs::remove_all(p);
        fs::create_directories(p);
        return p;
    }();
    return d;
}
std::string u8(const fs::path& p) {
    auto s = p.u8string();
    return std::string(s.begin(), s.end());
}
}  // namespace

TEST_CASE("savePrefs then loadPrefs round-trips") {
    auto path = u8(prefsDir() / "a.video.json");
    VideoPrefs p;
    p.size = Size::Aspect43;
    p.border = Border::Tv;
    REQUIRE(savePrefs(path, p));
    auto back = loadPrefs(path);
    CHECK(back.size == Size::Aspect43);
    CHECK(back.border == Border::Tv);
    CHECK_FALSE(back.smooth);
    CHECK_FALSE(back.filter);
}

TEST_CASE("savePrefs of empty prefs removes the file") {
    auto path = u8(prefsDir() / "b.video.json");
    VideoPrefs p;
    p.filter = Filter::Scanlines;
    REQUIRE(savePrefs(path, p));
    REQUIRE(fs::exists(path));
    CHECK(savePrefs(path, VideoPrefs{}));
    CHECK_FALSE(fs::exists(path));
    CHECK(savePrefs(path, VideoPrefs{}));  // already gone is fine
}

TEST_CASE("savePrefs under a parent that is a file returns false") {
    auto blocker = prefsDir() / "blocker";
    { std::ofstream(blocker) << "x"; }
    VideoPrefs p;
    p.size = Size::Full;
    CHECK_FALSE(savePrefs(u8(blocker / "c.video.json"), p));
}

TEST_CASE("loadPrefs recovers an interrupted save and an empty save clears it") {
    const std::string path = u8(prefsDir() / "r.video.json");
    {
        std::ofstream(prefsDir() / "r.video.json.bak") << R"({"size":"4:3"})";
    }
    CHECK(loadPrefs(path).size == Size::Aspect43);
    REQUIRE(savePrefs(path, VideoPrefs{}));
    CHECK(loadPrefs(path).empty());
    CHECK_FALSE(fs::exists(prefsDir() / "r.video.json.bak"));
}

TEST_CASE("loadPrefs of a missing file is empty") {
    CHECK(loadPrefs(u8(prefsDir() / "nope.video.json")).empty());
}
