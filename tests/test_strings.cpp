#include "strings.hpp"

#include <cstring>

#include "doctest/doctest.h"

TEST_CASE("every string has text in both languages") {
    for (int i = 0; i <= static_cast<int>(Str::BackToEquinox); ++i) {
        const auto s = static_cast<Str>(i);
        REQUIRE(tr(Lang::PtBR, s) != nullptr);
        REQUIRE(tr(Lang::EnUS, s) != nullptr);
        CHECK(std::strlen(tr(Lang::PtBR, s)) > 0);
        CHECK(std::strlen(tr(Lang::EnUS, s)) > 0);
    }
}

TEST_CASE("known translations") {
    CHECK(std::strcmp(tr(Lang::PtBR, Str::Exit), "Sair para o Equinox") == 0);
    CHECK(std::strcmp(tr(Lang::EnUS, Str::Exit), "Exit to Equinox") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::Continue), "Continuar") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::SaveState), "Salvar estado") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::LoadState), "Carregar estado") == 0);
}
