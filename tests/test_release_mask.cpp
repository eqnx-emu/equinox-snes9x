#include "release_mask.hpp"

#include "doctest/doctest.h"

TEST_CASE("held button stays hidden until released") {
    ReleaseMask m;
    m.suppress(1u << 8);  // A held when the menu closed
    CHECK_FALSE(m.filter(8, true));
    CHECK_FALSE(m.filter(8, true));
    CHECK_FALSE(m.filter(8, false));  // released
    CHECK(m.filter(8, true));         // a new press passes
}

TEST_CASE("other buttons are unaffected") {
    ReleaseMask m;
    m.suppress(1u << 8);
    CHECK(m.filter(0, true));
    CHECK_FALSE(m.filter(1, false));
}
