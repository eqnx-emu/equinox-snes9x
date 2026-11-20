#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A RetroArch .glslp preset, as far as the bundled presets use it.

enum class ScaleType { Source, Viewport, Absolute };
enum class Wrap { ClampToEdge, Repeat, MirroredRepeat };

struct PassSpec {
    std::string shader;  // path, already resolved against the preset's folder
    std::optional<bool> filterLinear;
    Wrap wrap = Wrap::ClampToEdge;
    std::optional<ScaleType> scaleTypeX, scaleTypeY;
    float scaleX = 1, scaleY = 1;
    bool floatFramebuffer = false;
};

struct Preset {
    std::vector<PassSpec> passes;
    std::vector<std::pair<std::string, float>> parameters;  // the preset's own overrides
};

// nullopt when `shaders` is missing or 0, a shaderN is missing, or the preset needs
// textures (LUTs are not supported).
std::optional<Preset> parsePreset(std::string_view text, const std::string& dir);
