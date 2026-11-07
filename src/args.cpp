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
    // A value runs until the next "--" flag. Launchers that don't keep quotes (hbmenu's
    // netloader) split "Super Mario World.zip" into several argv entries; this joins them back.
    auto isFlag = [](const char* t) { return t[0] == '-' && t[1] == '-'; };
    int i = 1;
    while (i < argc) {
        const char* key = argv[i++];
        std::string value;
        bool any = false;
        while (i < argc && !isFlag(argv[i])) {
            if (any) value.push_back(' ');
            value += argv[i++];
            any = true;
        }
        if (!any) continue;  // flag without a value
        if (!std::strcmp(key, "--protocol")) a.protocol = std::atoi(value.c_str());
        else if (!std::strcmp(key, "--rom")) a.rom = value;
        else if (!std::strcmp(key, "--title")) a.title = value;
        else if (!std::strcmp(key, "--saves")) a.saves = value;
        else if (!std::strcmp(key, "--states")) a.states = value;
        else if (!std::strcmp(key, "--return")) a.returnPath = value;
        else if (!std::strcmp(key, "--resume")) a.resumeToken = value;
        else if (!std::strcmp(key, "--cover")) a.cover = value;
        else if (!std::strcmp(key, "--video")) a.video = value;
    }
    if (a.protocol != kProtocolVersion || a.rom.empty() || a.returnPath.empty()) return std::nullopt;
    return a;
}

std::string buildReturnCommand(const LaunchArgs& args) {
    return "\"" + sanitizeArg(args.returnPath) + "\" --resume \"" + sanitizeArg(args.resumeToken) + "\"";
}
