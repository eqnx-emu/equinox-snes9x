#include "shader_preset.hpp"

#include <cstdlib>
#include <map>

namespace {
std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

// The value of `key = value`: a quoted value runs to its closing quote; an unquoted one
// stops at a comment.
std::string valueOf(std::string_view v) {
    v = trim(v);
    if (!v.empty() && v.front() == '"') {
        v.remove_prefix(1);
        return std::string(v.substr(0, v.find('"')));
    }
    return std::string(trim(v.substr(0, v.find('#'))));
}

std::map<std::string, std::string> readKeys(std::string_view text) {
    std::map<std::string, std::string> keys;
    while (!text.empty()) {
        const size_t end = text.find('\n');
        std::string_view line = trim(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        if (line.empty() || line.front() == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        keys[std::string(trim(line.substr(0, eq)))] = valueOf(line.substr(eq + 1));
    }
    return keys;
}

std::optional<float> number(const std::string& s) {
    if (s.empty()) return std::nullopt;
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end == s.c_str()) return std::nullopt;
    return v;
}

bool truthy(const std::string& s) { return s == "true" || s == "1"; }

std::optional<ScaleType> scaleType(const std::string& s) {
    if (s == "source") return ScaleType::Source;
    if (s == "viewport") return ScaleType::Viewport;
    if (s == "absolute") return ScaleType::Absolute;
    return std::nullopt;
}
}  // namespace

std::optional<Preset> parsePreset(std::string_view text, const std::string& dir) {
    const auto keys = readKeys(text);
    auto get = [&](const std::string& k) -> const std::string* {
        const auto it = keys.find(k);
        return it == keys.end() ? nullptr : &it->second;
    };
    if (get("textures")) return std::nullopt;
    const std::string* count = get("shaders");
    const auto n = count ? number(*count) : std::nullopt;
    if (!n || *n < 1) return std::nullopt;

    Preset p;
    for (int i = 0; i < static_cast<int>(*n); ++i) {
        const std::string idx = std::to_string(i);
        const std::string* shader = get("shader" + idx);
        if (!shader || shader->empty()) return std::nullopt;
        PassSpec pass;
        pass.shader = dir + "/" + *shader;
        if (const auto* v = get("filter_linear" + idx)) pass.filterLinear = truthy(*v);
        if (const auto* v = get("wrap_mode" + idx)) {
            if (*v == "repeat") pass.wrap = Wrap::Repeat;
            else if (*v == "mirrored_repeat") pass.wrap = Wrap::MirroredRepeat;
        }
        if (const auto* v = get("float_framebuffer" + idx)) pass.floatFramebuffer = truthy(*v);
        if (const auto* v = get("scale_type" + idx)) pass.scaleTypeX = pass.scaleTypeY = scaleType(*v);
        if (const auto* v = get("scale_type_x" + idx)) pass.scaleTypeX = scaleType(*v);
        if (const auto* v = get("scale_type_y" + idx)) pass.scaleTypeY = scaleType(*v);
        if (const auto* v = get("scale" + idx))
            if (auto f = number(*v)) pass.scaleX = pass.scaleY = *f;
        if (const auto* v = get("scale_x" + idx))
            if (auto f = number(*v)) pass.scaleX = *f;
        if (const auto* v = get("scale_y" + idx))
            if (auto f = number(*v)) pass.scaleY = *f;
        p.passes.push_back(std::move(pass));
    }

    if (const std::string* names = get("parameters")) {
        std::string_view list = *names;
        while (!list.empty()) {
            const size_t semi = list.find(';');
            const std::string name(trim(list.substr(0, semi)));
            list = semi == std::string_view::npos ? std::string_view{} : list.substr(semi + 1);
            if (const auto* v = get(name))
                if (auto f = number(*v)) p.parameters.emplace_back(name, *f);
        }
    }
    return p;
}
