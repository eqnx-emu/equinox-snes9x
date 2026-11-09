#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <map>
#include <string>

#include "menu.hpp"
#include "strings.hpp"
#include "video_menu.hpp"

// Draws the pause menu, the video menu, toasts and the error screen over the game frame
// with the console's shared system font. Without the font (logged) the shapes still draw.
// Everything is laid out in 1280x720 and scaled to the screen; text is rendered at the
// screen's size so it stays sharp docked.
class MenuView {
public:
    ~MenuView();
    bool init(SDL_Renderer* renderer);
    void shutdown();
    // The renderer's output height; call before drawing (cheap when unchanged).
    void setScreenHeight(int h);

    // Dims the frame already drawn and draws the sheet on the right.
    void drawMenu(const Menu& menu, const std::string& gameTitle, Lang lang);
    // The same sheet without dimming, so the change shows beside it.
    void drawVideoMenu(const VideoMenu& menu, Lang lang);
    // Bottom-of-screen message.
    void drawToast(const std::string& text);
    // Full screen: message, plus a "Back to Equinox" hint (with an A badge) when withBack.
    void drawError(const std::string& message, bool withBack, Lang lang);

private:
    struct Cached {
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0;
    };
    // Draws in 1280x720 coordinates for its lifetime.
    class Scaled {
    public:
        explicit Scaled(const MenuView& v);
        ~Scaled();
        Scaled(const Scaled&) = delete;
        Scaled& operator=(const Scaled&) = delete;

    private:
        SDL_Renderer* r_;
    };
    int screenPx(int px) const;  // a 720p size in screen pixels
    TTF_Font* font(int px);
    const Cached* text(const std::string& s, int px, SDL_Color c);
    // align: 0 = x is the left edge, 1 = centred on x, 2 = x is the right edge.
    void drawText(const std::string& s, int px, SDL_Color c, int x, int y, int align = 0);
    int textWidth(const std::string& s, int px);
    std::string fit(const std::string& s, int px, int maxW);
    void fillRound(int x, int y, int w, int h, int r, SDL_Color c);
    void badgeA(int cx, int cy, int r);
    // The sheet's left edge for this frame: it slides in from the right when
    // no sheet was drawn just before.
    int sheetLeft(int width);
    void drawIcon(MenuItem icon, int cx, int cy, int size, SDL_Color c);

    Uint32 sheetOpenedAt_ = 0, sheetDrawnAt_ = 0;
    SDL_Renderer* r_ = nullptr;
    float scale_ = 1.0f;
    bool ttf_ = false;
    std::string fontData_;  // the shared font lives in system memory; copied once
    std::map<int, TTF_Font*> fonts_;  // by screen pixel size
    std::map<std::string, Cached> cache_;
};

// Console language: any pt* is PtBR, everything else EnUS.
Lang systemLang();
