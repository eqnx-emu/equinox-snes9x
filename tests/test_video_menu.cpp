#include "video_menu.hpp"

#include <cstring>

#include "doctest/doctest.h"

TEST_CASE("size cycles forward and back with wrap") {
    VideoMenu m(VideoSettings{});
    CHECK(m.cursor() == VideoRow::Size);
    CHECK(m.change(1) == VideoAction::Changed);
    CHECK(m.settings().size == Size::Aspect87);
    m.change(1);
    CHECK(m.settings().size == Size::Aspect43);
    m.change(1);
    CHECK(m.settings().size == Size::Full);
    m.change(1);
    CHECK(m.settings().size == Size::Original);
    m.change(-1);
    CHECK(m.settings().size == Size::Full);
    m.change(-1);
    CHECK(m.settings().size == Size::Aspect43);
}

TEST_CASE("border is disabled in full screen and skipped by the cursor") {
    VideoSettings s;
    s.size = Size::Full;
    VideoMenu m(s);
    CHECK_FALSE(m.rowEnabled(VideoRow::Border));
    CHECK(m.rowEnabled(VideoRow::Filter));
    m.moveCursor(1);
    m.moveCursor(1);
    CHECK(m.cursor() == VideoRow::Filter);
    m.moveCursor(1);
    CHECK(m.cursor() == VideoRow::UseDefault);
    m.moveCursor(-1);
    CHECK(m.cursor() == VideoRow::Filter);
}

TEST_CASE("leaving full screen re-enables border") {
    VideoSettings s;
    s.size = Size::Full;
    VideoMenu m(s);
    m.change(1);
    CHECK(m.settings().size == Size::Original);
    CHECK(m.rowEnabled(VideoRow::Border));
}

TEST_CASE("cursor stops at the ends") {
    VideoMenu m(VideoSettings{});
    m.moveCursor(-1);
    CHECK(m.cursor() == VideoRow::Size);
    for (int i = 0; i < 10; ++i) m.moveCursor(1);
    CHECK(m.cursor() == VideoRow::Back);
    m.moveCursor(1);
    CHECK(m.cursor() == VideoRow::Back);
}

TEST_CASE("filter never reaches ntsc when it is not allowed") {
    VideoMenu m(VideoSettings{}, false);
    m.moveCursor(1);
    m.moveCursor(1);
    CHECK(m.cursor() == VideoRow::Filter);
    m.change(1);
    CHECK(m.settings().filter == Filter::Scanlines);
    m.change(1);
    CHECK(m.settings().filter == Filter::None);
    m.change(-1);
    CHECK(m.settings().filter == Filter::Scanlines);
}

TEST_CASE("filter reaches ntsc when allowed") {
    VideoMenu m(VideoSettings{}, true);
    m.moveCursor(1);
    m.moveCursor(1);
    m.change(1);
    m.change(1);
    CHECK(m.settings().filter == Filter::Ntsc);
    m.change(1);
    CHECK(m.settings().filter == Filter::None);
}

TEST_CASE("tv border is offered only in original") {
    VideoSettings s;
    s.size = Size::Aspect43;
    s.border = Border::Cover;
    VideoMenu m(s);
    m.moveCursor(1);
    m.moveCursor(1);
    m.moveCursor(1);
    REQUIRE(m.cursor() == VideoRow::Border);
    m.change(1);
    CHECK(m.settings().border == Border::Black);  // skips tv
    m.change(-1);
    CHECK(m.settings().border == Border::Cover);
    VideoMenu o(VideoSettings{});  // original
    o.moveCursor(1);
    o.moveCursor(1);
    o.moveCursor(1);
    o.change(1);
    o.change(1);
    CHECK(o.settings().border == Border::Tv);
}

TEST_CASE("smooth and border cycle") {
    VideoMenu m(VideoSettings{});
    m.moveCursor(1);
    m.change(1);
    CHECK(m.settings().smooth == Smooth::Sharp);
    m.change(-1);
    m.change(-1);
    CHECK(m.settings().smooth == Smooth::Soft);
    m.moveCursor(1);
    m.moveCursor(1);
    CHECK(m.cursor() == VideoRow::Border);
    m.change(-1);
    CHECK(m.settings().border == Border::Black);
}

TEST_CASE("confirm and back") {
    VideoMenu m(VideoSettings{});
    CHECK(m.confirm() == VideoAction::Changed);  // acts as change(+1) on Size
    CHECK(m.settings().size == Size::Aspect87);
    for (int i = 0; i < 4; ++i) m.moveCursor(1);
    CHECK(m.cursor() == VideoRow::UseDefault);
    CHECK(m.confirm() == VideoAction::UseDefault);
    m.moveCursor(1);
    CHECK(m.cursor() == VideoRow::Back);
    CHECK(m.confirm() == VideoAction::Close);
    CHECK(m.back() == VideoAction::Close);
}

TEST_CASE("change does nothing on the button rows") {
    VideoMenu m(VideoSettings{});
    for (int i = 0; i < 4; ++i) m.moveCursor(1);
    CHECK(m.change(1) == VideoAction::None);
}

TEST_CASE("reset replaces the settings") {
    VideoMenu m(VideoSettings{});
    m.change(1);
    m.reset(VideoSettings{});
    CHECK(m.settings() == VideoSettings{});
}

TEST_CASE("labels have text") {
    for (Size v : {Size::Original, Size::Aspect87, Size::Aspect43, Size::Full})
        CHECK(std::strlen(tr(Lang::EnUS, label(v))) > 0);
    for (Smooth v : {Smooth::Pixel, Smooth::Sharp, Smooth::Soft})
        CHECK(std::strlen(tr(Lang::PtBR, label(v))) > 0);
    for (Filter v : {Filter::None, Filter::Scanlines, Filter::Ntsc})
        CHECK(std::strlen(tr(Lang::PtBR, label(v))) > 0);
    for (Border v : {Border::Equinox, Border::Cover, Border::Tv, Border::Black})
        CHECK(std::strlen(tr(Lang::EnUS, label(v))) > 0);
    CHECK(label(Size::Aspect87) == Str::Size87);
}
