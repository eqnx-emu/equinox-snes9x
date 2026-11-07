#include "strings.hpp"

namespace {
struct Pair {
    const char* pt;
    const char* en;
};

// Same order as enum Str.
constexpr Pair kTable[] = {
    {"Continuar", "Continue"},
    {"Salvar estado", "Save state"},
    {"Carregar estado", "Load state"},
    {"Sair para o Equinox", "Exit to Equinox"},
    {"Estado salvo", "State saved"},
    {"Estado carregado", "State loaded"},
    {"N\xC3\xA3o foi poss\xC3\xADvel salvar", "Could not save"},
    {"Nenhum estado salvo", "No saved state"},
    {"Abra este emulador pelo Equinox", "Open this emulator from Equinox"},
    {"ROM n\xC3\xA3o encontrada", "ROM not found"},
    {"N\xC3\xA3o foi poss\xC3\xADvel ler a ROM", "Could not read the ROM"},
    {"Nenhuma ROM de SNES dentro do zip", "No SNES ROM inside the zip"},
    {"ROM n\xC3\xA3o suportada", "Unsupported ROM"},
    {"Voltar ao Equinox", "Back to Equinox"},
    {"Arquivo grande demais para uma ROM de SNES", "File too big for a SNES ROM"},
    {"Vídeo", "Video"},
    {"Vídeo", "Video"},
    {"Tamanho", "Size"},
    {"Suavização", "Smoothing"},
    {"Filtro", "Filter"},
    {"Moldura", "Border"},
    {"Usar o padrão", "Use default"},
    {"Voltar", "Back"},
    {"Original", "Original"},
    {"8:7", "8:7"},
    {"4:3", "4:3"},
    {"Tela inteira", "Full screen"},
    {"Pixel", "Pixel"},
    {"Nítido", "Sharp"},
    {"Suave", "Soft"},
    {"Nenhum", "None"},
    {"Scanlines", "Scanlines"},
    {"NTSC", "NTSC"},
    {"Equinox", "Equinox"},
    {"Capa do jogo", "Game cover"},
    {"TV de tubo", "CRT TV"},
    {"Preto", "Black"},
    {"Não deu para guardar o ajuste", "Could not save the setting"},
};
}  // namespace

const char* tr(Lang lang, Str s) {
    const auto i = static_cast<unsigned>(s);
    if (i >= sizeof kTable / sizeof kTable[0]) return "";
    return lang == Lang::PtBR ? kTable[i].pt : kTable[i].en;
}
