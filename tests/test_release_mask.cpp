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

namespace {
constexpr uint32_t kSelect = 1u << 2, kStart = 1u << 3, kL2 = 1u << 12, kR2 = 1u << 13, kA = 1u << 8;
}

TEST_CASE("the menu combo is Minus and Plus (Select and Start) held together") {
    CHECK(isMenuCombo(kSelect | kStart));
    CHECK(isMenuCombo(kSelect | kStart | kA));
    CHECK_FALSE(isMenuCombo(kSelect));
    CHECK_FALSE(isMenuCombo(kStart));
    CHECK_FALSE(isMenuCombo(kL2 | kR2));  // ZL+ZR no longer opens the menu
    CHECK_FALSE(isMenuCombo(0));
}

TEST_CASE("the menu combo hides Select and Start from the game until each is released") {
    ReleaseMask m;
    m.hideMenuCombo(kSelect | kStart);
    CHECK_FALSE(m.filter(2, true));
    CHECK_FALSE(m.filter(3, true));
    // Plus released first: Select, still held, stays hidden.
    CHECK_FALSE(m.filter(3, false));
    m.hideMenuCombo(kSelect);
    CHECK_FALSE(m.filter(2, true));
    CHECK_FALSE(m.filter(2, false));
    // Both released: each press reaches the game again.
    CHECK(m.filter(2, true));
    CHECK(m.filter(3, true));
}

TEST_CASE("Minus or Plus alone reaches the game") {
    ReleaseMask m;
    m.hideMenuCombo(kStart);
    CHECK(m.filter(3, true));
    m.hideMenuCombo(kSelect);
    CHECK(m.filter(2, true));
}

TEST_CASE("other buttons are unaffected") {
    ReleaseMask m;
    m.suppress(1u << 8);
    CHECK(m.filter(0, true));
    CHECK_FALSE(m.filter(1, false));
}
