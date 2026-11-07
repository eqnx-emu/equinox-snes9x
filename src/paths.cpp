#include "paths.hpp"

std::string romStem(const std::string& romPath) {
    const auto slash = romPath.find_last_of("/\\");
    std::string name = slash == std::string::npos ? romPath : romPath.substr(slash + 1);
    const auto dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name.resize(dot);
    return name;
}

static std::string join(const std::string& dir, const std::string& file) {
    if (dir.empty()) return file;
    const char last = dir.back();
    return (last == '/' || last == '\\') ? dir + file : dir + "/" + file;
}

std::string srmPath(const std::string& savesDir, const std::string& romPath) {
    return join(savesDir, romStem(romPath) + ".srm");
}

std::string statePath(const std::string& statesDir, const std::string& romPath) {
    return join(statesDir, romStem(romPath) + ".state");
}

std::string videoPrefsPath(const std::string& savesDir, const std::string& romPath) {
    return join(savesDir, romStem(romPath) + ".video.json");
}
