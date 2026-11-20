#pragma once

#include <span>

#include "video_prefs.hpp"

// The bundled RetroArch presets live in the .nro's romfs, at the same paths as in
// libretro/glsl-shaders.
inline constexpr const char* kShaderRoot = "romfs:/shaders/";

// The filter's .glslp, relative to kShaderRoot. nullptr for Filter::None.
const char* presetPath(Filter);

// A shader parameter set over the value its #pragma parameter declares.
struct ParamOverride {
    const char* name;
    float value;
};

// What an intensity level changes in the filter's shader parameters. Medium keeps the
// shader's own defaults, except where those defaults curve the picture.
std::span<const ParamOverride> intensityOverrides(Filter, Intensity);
