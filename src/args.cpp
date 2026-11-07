#include "args.hpp"

#include <cstdlib>
#include <cstring>

std::string sanitizeArg(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        const auto u = static_cast<unsigned char>(c);
        if (c == '"' || u < 0x20 || u == 0x7f) continue;
        out.push_back(c);
    }
    return out;
}

std::optional<LaunchArgs> parseArgs(int argc, const char* const* argv) {
    LaunchArgs a;
    for (int i = 1; i + 1 < argc; i += 2) {
        const char* key = argv[i];
        const char* value = argv[i + 1];
        if (!std::strcmp(key, "--protocol")) a.protocol = std::atoi(value);
        else if (!std::strcmp(key, "--rom")) a.rom = value;
        else if (!std::strcmp(key, "--title")) a.title = value;
        else if (!std::strcmp(key, "--saves")) a.saves = value;
        else if (!std::strcmp(key, "--states")) a.states = value;
        else if (!std::strcmp(key, "--return")) a.returnPath = value;
        else if (!std::strcmp(key, "--resume")) a.resumeToken = value;
        else --i;  // unknown token: resync one by one
    }
    if (a.protocol != kProtocolVersion || a.rom.empty() || a.returnPath.empty()) return std::nullopt;
    return a;
}

std::string buildReturnCommand(const LaunchArgs& args) {
    return "\"" + sanitizeArg(args.returnPath) + "\" --resume \"" + sanitizeArg(args.resumeToken) + "\"";
}
