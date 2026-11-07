#include "menu.hpp"

#include "doctest/doctest.h"

TEST_CASE("menu opens on Continue") {
    Menu m;
    CHECK_FALSE(m.isOpen());
    m.open();
    CHECK(m.isOpen());
    CHECK(m.selected() == MenuItem::Continue);
}

TEST_CASE("menu down wraps from Exit to Continue") {
    Menu m;
    m.setHasState(true);
    m.open();
    m.down();
    CHECK(m.selected() == MenuItem::SaveState);
    m.down();
    CHECK(m.selected() == MenuItem::LoadState);
    m.down();
    CHECK(m.selected() == MenuItem::Video);
    m.down();
    CHECK(m.selected() == MenuItem::Exit);
    m.down();
    CHECK(m.selected() == MenuItem::Continue);
    m.up();
    CHECK(m.selected() == MenuItem::Exit);
}

TEST_CASE("menu load state is skipped when there is no state") {
    Menu m;
    m.setHasState(false);
    m.open();
    m.down();
    CHECK(m.selected() == MenuItem::SaveState);
    m.down();
    CHECK(m.selected() == MenuItem::Video);
    m.up();
    CHECK(m.selected() == MenuItem::SaveState);
}

TEST_CASE("menu confirm maps items to actions") {
    Menu m;
    m.setHasState(true);
    m.open();
    CHECK(m.confirm() == MenuAction::Close);
    CHECK_FALSE(m.isOpen());
    m.open();
    m.down();
    CHECK(m.confirm() == MenuAction::SaveState);
    m.down();
    CHECK(m.confirm() == MenuAction::LoadState);
}

TEST_CASE("menu confirm on Exit returns Exit") {
    Menu m;
    m.open();
    m.up();
    CHECK(m.selected() == MenuItem::Exit);
    CHECK(m.confirm() == MenuAction::Exit);
}

TEST_CASE("menu back closes") {
    Menu m;
    m.open();
    CHECK(m.back() == MenuAction::Close);
    CHECK_FALSE(m.isOpen());
}

TEST_CASE("menu ignores input while closed") {
    Menu m;
    CHECK(m.confirm() == MenuAction::None);
    CHECK(m.back() == MenuAction::None);
}

TEST_CASE("menu confirm on Video opens the video menu") {
    Menu m;
    m.open();
    m.down();
    m.down();  // SaveState, then Video (no saved state)
    CHECK(m.selected() == MenuItem::Video);
    CHECK(m.confirm() == MenuAction::OpenVideo);
    CHECK(m.isOpen());
}
