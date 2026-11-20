#include "filters.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "doctest/doctest.h"
#include "shader_preset.hpp"
#include "shader_source.hpp"

namespace {
namespace fs = std::filesystem;

const fs::path kRoot = fs::path(EQX_ROMFS_DIR) / "shaders";

constexpr Filter kShaderFilters[] = {Filter::Scanlines, Filter::Ntsc,      Filter::ZfastCrt,
                                     Filter::CrtEasymode, Filter::CrtLottes, Filter::CrtHyllian,
                                     Filter::CrtGeom};
constexpr Intensity kLevels[] = {Intensity::Light, Intensity::Medium, Intensity::Strong};

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct Range {
    float min, max;
};

// "#pragma parameter NAME "desc" default min max step" of every shader the preset names.
std::map<std::string, Range> declaredParameters(const fs::path& preset) {
    std::map<std::string, Range> out;
    std::istringstream lines(slurp(preset));
    for (std::string line; std::getline(lines, line);) {
        char shader[256] = {};
        if (std::sscanf(line.c_str(), "shader%*d = %255s", shader) != 1) continue;
        std::istringstream src(slurp(preset.parent_path() / shader));
        for (std::string s; std::getline(src, s);) {
            if (s.rfind("#pragma parameter ", 0) != 0) continue;
            std::istringstream words(s.substr(18));
            std::string name;
            words >> name;
            const size_t close = s.rfind('"');
            float def = 0, mn = 0, mx = 0;
            REQUIRE(std::sscanf(s.c_str() + close + 1, "%f %f %f", &def, &mn, &mx) == 3);
            out[name] = {mn, mx};
        }
    }
    return out;
}

float overrideOf(Filter f, Intensity i, const char* name) {
    for (const ParamOverride& o : intensityOverrides(f, i))
        if (std::string(o.name) == name) return o.value;
    return -1;
}
}  // namespace

TEST_CASE("no preset without a filter") { CHECK(presetPath(Filter::None) == nullptr); }

TEST_CASE("every shader filter names a bundled preset") {
    for (Filter f : kShaderFilters) {
        REQUIRE(presetPath(f) != nullptr);
        CHECK(fs::is_regular_file(kRoot / presetPath(f)));
    }
}

TEST_CASE("every override is a declared parameter within its range") {
    for (Filter f : kShaderFilters) {
        const auto declared = declaredParameters(kRoot / presetPath(f));
        REQUIRE_FALSE(declared.empty());
        for (Intensity i : kLevels)
            for (const ParamOverride& o : intensityOverrides(f, i)) {
                INFO(presetPath(f), " ", o.name);
                const auto it = declared.find(o.name);
                REQUIRE(it != declared.end());
                CHECK(o.value >= it->second.min);
                CHECK(o.value <= it->second.max);
            }
    }
}

TEST_CASE("only crt-geom strong curves") {
    CHECK(overrideOf(Filter::CrtGeom, Intensity::Light, "CURVATURE") == 0.0f);
    CHECK(overrideOf(Filter::CrtGeom, Intensity::Medium, "CURVATURE") == 0.0f);
    CHECK(overrideOf(Filter::CrtGeom, Intensity::Strong, "CURVATURE") == 1.0f);
    for (Intensity i : kLevels) {
        CHECK(overrideOf(Filter::CrtLottes, i, "CURVATURE") == 0.0f);
        CHECK(overrideOf(Filter::CrtLottes, i, "CORNER") == 0.0f);
    }
}

TEST_CASE("every bundled preset parses and its stages build on GLES3") {
    for (Filter f : kShaderFilters) {
        const fs::path preset = kRoot / presetPath(f);
        INFO(presetPath(f));
        const auto p = parsePreset(slurp(preset), preset.parent_path().generic_string());
        REQUIRE(p);
        for (const PassSpec& pass : p->passes) {
            const std::string src = slurp(pass.shader);
            REQUIRE_FALSE(src.empty());
            CHECK(buildStage(src, Stage::Vertex, true));
            CHECK(buildStage(src, Stage::Fragment, true));
        }
    }
}

TEST_CASE("light and strong differ from medium") {
    for (Filter f : kShaderFilters) {
        CHECK_FALSE(intensityOverrides(f, Intensity::Light).empty());
        CHECK_FALSE(intensityOverrides(f, Intensity::Strong).empty());
    }
}
