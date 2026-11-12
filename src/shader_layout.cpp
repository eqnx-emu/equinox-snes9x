#include "shader_layout.hpp"

#include <cmath>

namespace {
struct Axis {
    ScaleType type;
    float scale;
};

// One axis of a pass, with the defaults filled in.
Axis axis(const PassSpec& pass, bool last, bool x) {
    const auto& type = x ? pass.scaleTypeX : pass.scaleTypeY;
    const float scale = x ? pass.scaleX : pass.scaleY;
    if (!pass.scaleTypeX && !pass.scaleTypeY) return {last ? ScaleType::Viewport : ScaleType::Source, 1.0f};
    return {type.value_or(ScaleType::Source), scale};
}

unsigned sized(Axis a, unsigned source, unsigned viewport) {
    float v = a.scale;
    if (a.type == ScaleType::Source) v *= static_cast<float>(source);
    else if (a.type == ScaleType::Viewport) v *= static_cast<float>(viewport);
    const long r = std::lround(v);
    return r < 1 ? 1u : static_cast<unsigned>(r);
}
}  // namespace

std::vector<Size2> passSizes(const Preset& p, Size2 input, Size2 viewport) {
    std::vector<Size2> out;
    out.reserve(p.passes.size());
    Size2 source = input;
    for (size_t i = 0; i < p.passes.size(); ++i) {
        const bool last = i + 1 == p.passes.size();
        source = {sized(axis(p.passes[i], last, true), source.w, viewport.w),
                  sized(axis(p.passes[i], last, false), source.h, viewport.h)};
        out.push_back(source);
    }
    return out;
}

bool needsFinalBlit(const Preset& p) {
    if (p.passes.empty()) return false;
    for (bool x : {true, false}) {
        const Axis a = axis(p.passes.back(), true, x);
        if (a.type != ScaleType::Viewport || a.scale != 1.0f) return true;
    }
    return false;
}

std::array<float, 8> passTexCoords(bool fromGameFrame) {
    if (fromGameFrame) return {0, 1, 1, 1, 0, 0, 1, 0};
    return {0, 0, 1, 0, 0, 1, 1, 1};
}
