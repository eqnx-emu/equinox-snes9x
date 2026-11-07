#include "core_options.hpp"

#include <cstring>

#include "doctest/doctest.h"

TEST_CASE("snes9x_blargg follows the ntsc switch") {
    CHECK(std::strcmp(coreOptionValue("snes9x_blargg", true), "composite") == 0);
    CHECK(std::strcmp(coreOptionValue("snes9x_blargg", false), "disabled") == 0);
}

TEST_CASE("other core options are left to the core defaults") {
    CHECK(coreOptionValue("snes9x_overclock_superfx", true) == nullptr);
    CHECK(coreOptionValue("snes9x_blargg_extra", true) == nullptr);
    CHECK(coreOptionValue("", false) == nullptr);
    CHECK(coreOptionValue(nullptr, true) == nullptr);
}
