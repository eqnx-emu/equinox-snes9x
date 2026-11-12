#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A RetroArch .glsl file holds both stages; VERTEX or FRAGMENT picks one.

struct ParamDecl {
    std::string name;
    float def, min, max;
};

// Every `#pragma parameter NAME "description" default min max step` line.
std::vector<ParamDecl> parseParameters(std::string_view source);

enum class Stage { Vertex, Fragment };

// The source ready for glShaderSource: a GLES version line, the stage define and
// PARAMETER_UNIFORM, then the source without its own #version. Sources below 130 (or
// without a version) become `#version 100`; 130 and up become `#version 300 es`, which
// needs GLES3, so without it the result is nullopt.
std::optional<std::string> buildStage(std::string_view source, Stage stage, bool gles3);
