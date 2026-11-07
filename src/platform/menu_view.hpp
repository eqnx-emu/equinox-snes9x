#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <map>
#include <string>

#include "menu.hpp"
#include "strings.hpp"

// Draws the pause menu, toasts and the error screen over the game frame with the
// console's shared system font. Without the font (logged) the shapes still draw.
class MenuView {
public:
    ~MenuView();
    bool init(SDL_Renderer* renderer);
    void shutdown();

    // Dims the frame already drawn and draws the panel.
    void drawMenu(const Menu& menu, const std::string& gameTitle, Lang lang);
    // Bottom-of-screen message.
    void drawToast(const std::string& text);
    // Full screen: message, plus a "Back to Equinox" hint (with an A badge) when withBack.
    void drawError(const std::string& message, bool withBack, Lang lang);

private:
    struct Cached {
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0;
    };
    TTF_Font* font(int px);
    const Cached* text(const std::string& s, int px, SDL_Color c);
    // align: 0 = x is the left edge, 1 = centred on x, 2 = x is the right edge.
    void drawText(const std::string& s, int px, SDL_Color c, int x, int y, int align = 0);
    int textWidth(const std::string& s, int px);
    std::string fit(const std::string& s, int px, int maxW);
    void fillRound(int x, int y, int w, int h, int r, SDL_Color c);
    void badgeA(int cx, int cy, int r);

    SDL_Renderer* r_ = nullptr;
    bool ttf_ = false;
    std::string fontData_;  // the shared font lives in system memory; copied once
    std::map<int, TTF_Font*> fonts_;
    std::map<std::string, Cached> cache_;
};

// Console language: any pt* is PtBR, everything else EnUS.
Lang systemLang();
