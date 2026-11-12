#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <map>
#include <optional>
#include <string>

#include "menu.hpp"
#include "strings.hpp"
#include "video_art.hpp"
#include "video_menu.hpp"

// The last savestate, as the pause menu shows it.
struct StateCard {
    std::string when;             // "Hoje às 14:32"
    const Image* thumb = nullptr;  // null or empty: the card draws without a picture
    int thumbVersion = 0;          // changes whenever *thumb does
};

// What the pause sheet's header and state card show.
struct PauseInfo {
    std::string console;  // "Super Nintendo"
    std::string title;
    std::string played;   // "Jogando há 42 min"
    std::optional<StateCard> state;
};

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
    void drawMenu(const Menu& menu, const PauseInfo& info, Lang lang);
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
    // A button glyph in the hint bar.
    enum class Key { A, B, LeftRight };

    int screenPx(int px) const;  // a 720p size in screen pixels
    TTF_Font* font(int px);
    const Cached* text(const std::string& s, int px, SDL_Color c);
    // align: 0 = x is the left edge, 1 = centred on x, 2 = x is the right edge.
    void drawText(const std::string& s, int px, SDL_Color c, int x, int y, int align = 0);
    // The same, with the text's box centred vertically on cy.
    void drawTextMid(const std::string& s, int px, SDL_Color c, int x, int cy, int align = 0);
    int textWidth(const std::string& s, int px);
    std::string fit(const std::string& s, int px, int maxW);
    void fillRound(int x, int y, int w, int h, int r, SDL_Color c);
    // A 1-px outline of a rounded rectangle, filled inside with `inside`.
    void outlineRound(int x, int y, int w, int h, int r, SDL_Color line, SDL_Color inside);
    void triangle(float x0, float y0, float x1, float y1, float x2, float y2, SDL_Color c);
    void badgeA(int cx, int cy, int r);
    // "(A) Choose  (B) Back" right-aligned to rightX, centred on cy.
    void drawHints(std::initializer_list<std::pair<Key, std::string>> hints, int rightX, int cy);
    // The sheet's left edge for this frame: it slides in from the right when
    // no sheet was drawn just before.
    int sheetLeft(int width);
    void drawIcon(MenuItem icon, int cx, int cy, int size, SDL_Color c);
    void drawStateCard(const StateCard& card, int x, int y, int w, Lang lang);
    void drawValueRow(const std::string& name, const std::string& value, bool sel, bool enabled, int x, int y,
                      int w, int h, int valuePx, bool fill = true);

    Uint32 sheetOpenedAt_ = 0, sheetDrawnAt_ = 0;
    SDL_Renderer* r_ = nullptr;
    float scale_ = 1.0f;
    bool ttf_ = false;
    std::string fontData_;  // the shared font lives in system memory; copied once
    std::map<int, TTF_Font*> fonts_;  // by screen pixel size
    std::map<std::string, Cached> cache_;
    SDL_Texture* thumbTex_ = nullptr;  // the state card's picture
    int thumbVersion_ = -1;
};

// Console language: any pt* is PtBR, everything else EnUS.
Lang systemLang();
