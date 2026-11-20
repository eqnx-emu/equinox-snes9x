#include "filters.hpp"

namespace {
constexpr ParamOverride kScanlinesLight[] = {{"SCANLINE_SINE_COMP_A", 0.05f}, {"SCANLINE_SINE_COMP_B", 0.25f}};
constexpr ParamOverride kScanlinesStrong[] = {{"SCANLINE_SINE_COMP_A", 0.25f}, {"SCANLINE_SINE_COMP_B", 0.80f}};

constexpr ParamOverride kNtscLight[] = {{"quality", 0.0f}};
constexpr ParamOverride kNtscStrong[] = {{"quality", 2.0f}};

constexpr ParamOverride kZfastLight[] = {{"LOWLUMSCAN", 3.0f}, {"HILUMSCAN", 4.0f}, {"MASK_DARK", 0.10f}};
constexpr ParamOverride kZfastStrong[] = {
    {"LOWLUMSCAN", 9.0f}, {"HILUMSCAN", 14.0f}, {"MASK_DARK", 0.45f}, {"BRIGHTBOOST", 1.40f}};

constexpr ParamOverride kEasymodeLight[] = {{"MASK_STRENGTH", 0.10f}, {"SCANLINE_STRENGTH", 0.50f}};
constexpr ParamOverride kEasymodeStrong[] = {
    {"MASK_STRENGTH", 0.60f}, {"SCANLINE_BRIGHT_MIN", 0.25f}, {"BRIGHT_BOOST", 1.40f}};

constexpr ParamOverride kLottesLight[] = {
    {"MASK_INTENSITY", 0.20f}, {"SCANLINE_THINNESS", 0.30f}, {"CURVATURE", 0.0f}, {"CORNER", 0.0f}};
constexpr ParamOverride kLottesMedium[] = {{"CURVATURE", 0.0f}, {"CORNER", 0.0f}};
constexpr ParamOverride kLottesStrong[] = {
    {"MASK_INTENSITY", 0.80f}, {"SCANLINE_THINNESS", 0.80f}, {"CURVATURE", 0.0f}, {"CORNER", 0.0f}};

constexpr ParamOverride kHyllianLight[] = {{"SCANLINES_STRENGTH", 0.30f}, {"MASK_INTENSITY", 0.20f}};
constexpr ParamOverride kHyllianStrong[] = {
    {"SCANLINES_STRENGTH", 0.80f}, {"MASK_INTENSITY", 0.80f}, {"COLOR_BOOST", 1.45f}};

constexpr ParamOverride kGeomLight[] = {
    {"CURVATURE", 0.0f}, {"cornersize", 0.001f}, {"DOTMASK", 0.10f}, {"scanline_weight", 0.20f}};
constexpr ParamOverride kGeomMedium[] = {{"CURVATURE", 0.0f}, {"cornersize", 0.001f}};
constexpr ParamOverride kGeomStrong[] = {{"CURVATURE", 1.0f}, {"DOTMASK", 0.60f}};

struct Levels {
    std::span<const ParamOverride> light, medium, strong;
};

Levels levels(Filter f) {
    switch (f) {
        case Filter::None: return {};
        case Filter::Scanlines: return {kScanlinesLight, {}, kScanlinesStrong};
        case Filter::Ntsc: return {kNtscLight, {}, kNtscStrong};
        case Filter::ZfastCrt: return {kZfastLight, {}, kZfastStrong};
        case Filter::CrtEasymode: return {kEasymodeLight, {}, kEasymodeStrong};
        case Filter::CrtLottes: return {kLottesLight, kLottesMedium, kLottesStrong};
        case Filter::CrtHyllian: return {kHyllianLight, {}, kHyllianStrong};
        case Filter::CrtGeom: return {kGeomLight, kGeomMedium, kGeomStrong};
    }
    return {};
}
}  // namespace

const char* presetPath(Filter f) {
    switch (f) {
        case Filter::None: return nullptr;
        case Filter::Scanlines: return "scanlines/scanline.glslp";
        case Filter::Ntsc: return "ntsc/ntsc-adaptive.glslp";
        case Filter::ZfastCrt: return "crt/zfast-crt.glslp";
        case Filter::CrtEasymode: return "crt/crt-easymode.glslp";
        case Filter::CrtLottes: return "crt/crt-lottes-fast.glslp";
        case Filter::CrtHyllian: return "crt/crt-hyllian.glslp";
        case Filter::CrtGeom: return "crt/crt-geom.glslp";
    }
    return nullptr;
}

std::span<const ParamOverride> intensityOverrides(Filter f, Intensity i) {
    const Levels l = levels(f);
    switch (i) {
        case Intensity::Light: return l.light;
        case Intensity::Medium: return l.medium;
        case Intensity::Strong: return l.strong;
    }
    return {};
}
