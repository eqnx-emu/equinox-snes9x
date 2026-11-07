#include "args.hpp"

#include "doctest/doctest.h"

TEST_CASE("parses full launch line") {
    const char* argv[] = {"sdmc:/equinox/emulators/snes/equinox-snes9x.nro",
        "--protocol", "1", "--rom", "sdmc:/equinox/roms/snes/Super Metroid (USA).sfc",
        "--title", "Super Metroid", "--saves", "sdmc:/equinox/saves/snes",
        "--states", "sdmc:/equinox/states/snes",
        "--return", "sdmc:/switch/Equinox.nro", "--resume", "snes:3"};
    auto a = parseArgs(15, argv);
    REQUIRE(a);
    CHECK(a->protocol == 1);
    CHECK(a->rom == "sdmc:/equinox/roms/snes/Super Metroid (USA).sfc");
    CHECK(a->title == "Super Metroid");
    CHECK(a->saves == "sdmc:/equinox/saves/snes");
    CHECK(a->states == "sdmc:/equinox/states/snes");
    CHECK(a->returnPath == "sdmc:/switch/Equinox.nro");
    CHECK(a->resumeToken == "snes:3");
}

TEST_CASE("rejects other protocol versions") {
    const char* argv[] = {"self", "--protocol", "2", "--rom", "x", "--return", "y"};
    CHECK_FALSE(parseArgs(7, argv));
}

TEST_CASE("rejects missing protocol") {
    const char* argv[] = {"self", "--rom", "x", "--return", "y"};
    CHECK_FALSE(parseArgs(5, argv));
}

TEST_CASE("rejects missing rom or return") {
    const char* noRom[] = {"self", "--protocol", "1", "--return", "y"};
    CHECK_FALSE(parseArgs(5, noRom));
    const char* noReturn[] = {"self", "--protocol", "1", "--rom", "x"};
    CHECK_FALSE(parseArgs(5, noReturn));
}

TEST_CASE("no arguments means opened from hbmenu") {
    const char* argv[] = {"self"};
    CHECK_FALSE(parseArgs(1, argv));
}

TEST_CASE("a trailing flag without a value is ignored") {
    const char* argv[] = {"self", "--protocol", "1", "--rom", "x", "--return", "y", "--resume"};
    auto a = parseArgs(8, argv);
    REQUIRE(a);
    CHECK(a->resumeToken.empty());
}

TEST_CASE("return command quotes the path") {
    LaunchArgs a;
    a.returnPath = "sdmc:/switch/Equinox App.nro";
    a.resumeToken = "fav:all:2";
    CHECK(buildReturnCommand(a) == "\"sdmc:/switch/Equinox App.nro\" --resume \"fav:all:2\"");
}

TEST_CASE("return command sanitizes its values") {
    LaunchArgs a;
    a.returnPath = "sdmc:/switch/Eq\"x.nro";
    a.resumeToken = "a\"b";
    CHECK(buildReturnCommand(a) == "\"sdmc:/switch/Eqx.nro\" --resume \"ab\"");
}

TEST_CASE("sanitize keeps accents and apostrophes, drops quotes") {
    CHECK(sanitizeArg("Pokémon \"Red\"") == "Pokémon Red");
    CHECK(sanitizeArg("Street Fighter II' Turbo") == "Street Fighter II' Turbo");
}

TEST_CASE("sanitize drops control characters") {
    CHECK(sanitizeArg(std::string("a\n\tb\x01") + "c") == "abc");
}
