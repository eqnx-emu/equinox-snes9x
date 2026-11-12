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
    CHECK(m.cursor() == VideoRow::UseDefault);  // the buttons share the last row
}

TEST_CASE("left and right move between the two buttons") {
    VideoMenu m(VideoSettings{});
    for (int i = 0; i < 10; ++i) m.moveCursor(1);
    REQUIRE(m.cursor() == VideoRow::UseDefault);
    CHECK(m.change(1) == VideoAction::None);
    CHECK(m.cursor() == VideoRow::Back);
    CHECK(m.change(1) == VideoAction::None);
    CHECK(m.cursor() == VideoRow::Back);
    m.moveCursor(1);
    CHECK(m.cursor() == VideoRow::Back);
    CHECK(m.change(-1) == VideoAction::None);
    CHECK(m.cursor() == VideoRow::UseDefault);
    CHECK(m.change(-1) == VideoAction::None);
    CHECK(m.cursor() == VideoRow::UseDefault);
}

TEST_CASE("up from either button goes to the last value row") {
    VideoSettings orig;
    orig.size = Size::Original;
    VideoMenu m(orig);
    for (int i = 0; i < 10; ++i) m.moveCursor(1);
    m.change(1);
    REQUIRE(m.cursor() == VideoRow::Back);
    m.moveCursor(-1);
    CHECK(m.cursor() == VideoRow::Border);
    VideoSettings full;
    full.size = Size::Full;
    VideoMenu f(full);
    for (int i = 0; i < 10; ++i) f.moveCursor(1);
    f.change(1);
    f.moveCursor(-1);
    CHECK(f.cursor() == VideoRow::Filter);
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
    m.change(1);
    CHECK(m.cursor() == VideoRow::Back);
    CHECK(m.confirm() == VideoAction::Close);
    CHECK(m.back() == VideoAction::Close);
}

TEST_CASE("change on the buttons leaves the settings alone") {
    VideoMenu m(VideoSettings{});
    for (int i = 0; i < 10; ++i) m.moveCursor(1);
    m.change(1);
    m.change(-1);
    CHECK(m.settings() == VideoSettings{});
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

TEST_CASE("every filter has a description") {
    for (Filter v : {Filter::None, Filter::Scanlines, Filter::Ntsc})
        CHECK(std::strlen(tr(Lang::PtBR, description(v))) > 0);
    CHECK(description(Filter::Scanlines) != description(Filter::Ntsc));
    CHECK(std::strcmp(tr(Lang::PtBR, description(Filter::Scanlines)), "Linhas escuras entre as linhas da imagem") == 0);
}
