#include "core_options.hpp"

#include <cstring>

const char* coreOptionValue(const char* key, bool ntsc) {
    if (key && std::strcmp(key, "snes9x_blargg") == 0) return ntsc ? "composite" : "disabled";
    return nullptr;
}

bool ntscFor(Filter filter) { return kNtscEnabled && filter == Filter::Ntsc; }
