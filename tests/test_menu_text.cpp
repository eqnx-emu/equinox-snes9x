#include "menu_text.hpp"

#include <string>

#include "doctest/doctest.h"

namespace {
std::tm at(int year, int yday, int mon, int mday, int hour, int min) {
    std::tm t{};
    t.tm_year = year - 1900;
    t.tm_yday = yday;
    t.tm_mon = mon - 1;
    t.tm_mday = mday;
    t.tm_hour = hour;
    t.tm_min = min;
    return t;
}

// One unit per byte: easy widths to reason about.
int bytes(const std::string& s) { return static_cast<int>(s.size()); }
}  // namespace

TEST_CASE("time played") {
    CHECK(playedFor(Lang::PtBR, 30) == "Jogando há menos de 1 min");
    CHECK(playedFor(Lang::PtBR, 42 * 60 + 59) == "Jogando há 42 min");
    CHECK(playedFor(Lang::PtBR, 65 * 60) == "Jogando há 1 h 05 min");
    CHECK(playedFor(Lang::EnUS, 42 * 60) == "Playing for 42 min");
    CHECK(playedFor(Lang::EnUS, 10) == "Playing for less than 1 min");
    CHECK(playedFor(Lang::EnUS, 2 * 3600) == "Playing for 2 h 00 min");
}

TEST_CASE("when the state was saved") {
    const std::tm now = at(2026, 315, 11, 12, 18, 0);
    CHECK(savedAt(Lang::PtBR, at(2026, 315, 11, 12, 14, 32), now) == "Hoje às 14:32");
    CHECK(savedAt(Lang::PtBR, at(2026, 314, 11, 11, 9, 5), now) == "Ontem às 09:05");
    CHECK(savedAt(Lang::PtBR, at(2026, 300, 10, 28, 23, 0), now) == "28/10 às 23:00");
    CHECK(savedAt(Lang::EnUS, at(2026, 315, 11, 12, 14, 32), now) == "Today at 14:32");
    CHECK(savedAt(Lang::EnUS, at(2026, 314, 11, 11, 9, 5), now) == "Yesterday at 09:05");
    CHECK(savedAt(Lang::EnUS, at(2026, 300, 10, 28, 23, 0), now) == "10/28 at 23:00");
}

TEST_CASE("yesterday across the new year") {
    const std::tm now = at(2027, 0, 1, 1, 10, 0);
    CHECK(savedAt(Lang::PtBR, at(2026, 364, 12, 31, 22, 15), now) == "Ontem às 22:15");
    CHECK(savedAt(Lang::PtBR, at(2025, 364, 12, 31, 22, 15), now) == "31/12 às 22:15");
}

TEST_CASE("a title wraps on words into two lines") {
    const auto l = wrapLines("Aventura de Exemplo (USA)", 14, bytes, 2);
    REQUIRE(l.size() == 2);
    CHECK(l[0] == "Aventura de");
    CHECK(l[1] == "Exemplo (USA)");
}

TEST_CASE("a short title is one line") {
    const auto l = wrapLines("Jogo", 12, bytes, 2);
    REQUIRE(l.size() == 1);
    CHECK(l[0] == "Jogo");
}

TEST_CASE("the last line ends in an ellipsis when the text does not fit") {
    const auto l = wrapLines("Aventura de Exemplo (USA)", 12, bytes, 2);
    CHECK(l[1] != "Exemplo (USA)");
    const auto cut = wrapLines("um dois tres quatro cinco seis", 10, bytes, 2);
    REQUIRE(cut.size() == 2);
    CHECK(cut[0] == "um dois");
    CHECK(cut[1].size() <= 10);
    CHECK(cut[1].substr(cut[1].size() - 3) == "...");
}

TEST_CASE("a word longer than the line is cut") {
    const auto l = wrapLines("Supercalifragilistico", 8, bytes, 2);
    REQUIRE(l.size() == 2);
    CHECK(l[0] == "Supercal");
    CHECK(l[1].size() <= 8);
    CHECK(l[1].substr(l[1].size() - 3) == "...");
}

TEST_CASE("an empty title has no lines") { CHECK(wrapLines("", 10, bytes, 2).empty()); }
