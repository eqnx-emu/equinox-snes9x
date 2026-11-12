#include "core_options.hpp"

#include <cstring>

#include "doctest/doctest.h"

TEST_CASE("snes9x_blargg is always off: NTSC is a shader now") {
    CHECK(std::strcmp(coreOptionValue("snes9x_blargg"), "disabled") == 0);
}

TEST_CASE("other core options are left to the core defaults") {
    CHECK(coreOptionValue("snes9x_overclock_superfx") == nullptr);
    CHECK(coreOptionValue("snes9x_blargg_extra") == nullptr);
    CHECK(coreOptionValue("") == nullptr);
    CHECK(coreOptionValue(nullptr) == nullptr);
}
