#include "shader_source.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
std::string_view ltrim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    return s;
}

// Calls fn(line, start offset) for each line of text.
template <typename Fn>
void eachLine(std::string_view text, Fn fn) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        if (fn(text.substr(pos, end - pos), pos)) return;
        pos = end + 1;
    }
}
}  // namespace

std::vector<ParamDecl> parseParameters(std::string_view source) {
    constexpr std::string_view kPragma = "#pragma parameter";
    std::vector<ParamDecl> out;
    eachLine(source, [&](std::string_view line, size_t) {
        line = ltrim(line);
        if (line.substr(0, kPragma.size()) != kPragma) return false;
        std::string_view rest = ltrim(line.substr(kPragma.size()));
        const size_t nameEnd = rest.find_first_of(" \t\"");
        if (nameEnd == 0 || nameEnd == std::string_view::npos) return false;
        const size_t close = rest.rfind('"');
        if (close == std::string_view::npos || close < nameEnd) return false;
        const std::string numbers(rest.substr(close + 1));
        ParamDecl d{std::string(rest.substr(0, nameEnd)), 0, 0, 0};
        if (std::sscanf(numbers.c_str(), "%f %f %f", &d.def, &d.min, &d.max) == 3) out.push_back(d);
        return false;
    });
    return out;
}

std::optional<std::string> buildStage(std::string_view source, Stage stage, bool gles3) {
    int version = 0;
    size_t lineStart = std::string_view::npos, lineLen = 0;
    eachLine(source, [&](std::string_view line, size_t pos) {
        const std::string_view t = ltrim(line);
        if (t.substr(0, 8) != "#version") return false;
        version = std::atoi(std::string(t.substr(8)).c_str());
        lineStart = pos;
        lineLen = line.size();
        return true;
    });
    const bool modern = version >= 130;
    if (modern && !gles3) return std::nullopt;

    std::string out = modern ? "#version 300 es\n" : "#version 100\n";
    out += stage == Stage::Vertex ? "#define VERTEX\n" : "#define FRAGMENT\n";
    out += "#define PARAMETER_UNIFORM\n";
    if (lineStart == std::string_view::npos) {
        out += source;
    } else {
        out += source.substr(0, lineStart);
        out += source.substr(lineStart + lineLen);  // keeps the newline, so line numbers hold
    }
    return out;
}
