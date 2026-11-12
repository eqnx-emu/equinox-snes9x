#include "strings.hpp"

#include <cstring>

#include "doctest/doctest.h"

TEST_CASE("every string has text in both languages") {
    for (int i = 0; i <= static_cast<int>(Str::DescCrtGeom); ++i) {
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

TEST_CASE("video strings") {
    CHECK(std::strcmp(tr(Lang::PtBR, Str::Video), "Vídeo") == 0);
    CHECK(std::strcmp(tr(Lang::EnUS, Str::SizeFull), "Full screen") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::BorderCover), "Capa do jogo") == 0);
}

TEST_CASE("shader filter and intensity strings") {
    CHECK(std::strcmp(tr(Lang::PtBR, Str::IntensityStrong), "Forte") == 0);
    CHECK(std::strcmp(tr(Lang::EnUS, Str::Intensity), "Intensity") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::Intensity), "Intensidade") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::IntensityLight), "Leve") == 0);
    CHECK(std::strcmp(tr(Lang::EnUS, Str::IntensityMedium), "Medium") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::FilterCrtGeom), "CRT Geom") == 0);
    CHECK(std::strcmp(tr(Lang::EnUS, Str::FilterZfastCrt), "zfast CRT") == 0);
}

TEST_CASE("pause and video sheet strings") {
    CHECK(std::strcmp(tr(Lang::PtBR, Str::HintChoose), "Escolher") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::HintBackToGame), "Voltar ao jogo") == 0);
    CHECK(std::strcmp(tr(Lang::EnUS, Str::HintChange), "Change") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::LastState), "Último estado salvo") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::SmoothOffWithFilter), "Desligada com filtro") == 0);
    CHECK(std::strcmp(tr(Lang::PtBR, Str::VideoHint), "As mudanças aparecem no jogo enquanto você escolhe") == 0);
}
