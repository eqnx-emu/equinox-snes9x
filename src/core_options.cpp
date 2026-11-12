#include "core_options.hpp"

#include <cstring>

const char* coreOptionValue(const char* key) {
    if (key && std::strcmp(key, "snes9x_blargg") == 0) return "disabled";
    return nullptr;
}
