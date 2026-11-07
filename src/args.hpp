#pragma once

#include <optional>
#include <string>
#include <string_view>

constexpr int kProtocolVersion = 1;

struct LaunchArgs {
    int protocol = 0;
    std::string rom, title, saves, states, returnPath, resumeToken;
};

// Reads: <self> --protocol 1 --rom <p> --title <t> --saves <dir> --states <dir>
//             --return <equinox.nro> --resume <token>
// Returns std::nullopt when --rom or --return is missing, or protocol != kProtocolVersion.
std::optional<LaunchArgs> parseArgs(int argc, const char* const* argv);

// Line handed to envSetNextLoad: "\"<returnPath>\" --resume \"<token>\"".
std::string buildReturnCommand(const LaunchArgs& args);

// Drops double quotes and control characters: libnx's argv parser has no escapes.
std::string sanitizeArg(std::string_view s);
