#include "platform/menu_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace {
constexpr SDL_Color kPanel{0x17, 0x1c, 0x28, 255};
constexpr SDL_Color kAmber{0xf5, 0xb5, 0x44, 255};
constexpr SDL_Color kAmberSoft{0xf5, 0xb5, 0x44, 40};
constexpr SDL_Color kText{0xe8, 0xeb, 0xf2, 255};
constexpr SDL_Color kMuted{0x5b, 0x64, 0x78, 255};
constexpr SDL_Color kTitle{0x9a, 0xa3, 0xb8, 255};
constexpr int kScreenW = 1280, kScreenH = 720;
constexpr int kPanelW = 560, kRow = 68, kPad = 28;
}  // namespace

Lang systemLang() {
#ifdef __SWITCH__
    Lang lang = Lang::EnUS;
    if (R_SUCCEEDED(setInitialize())) {
        u64 code = 0;
        SetLanguage sl;
        if (R_SUCCEEDED(setGetSystemLanguage(&code)) && R_SUCCEEDED(setMakeLanguage(code, &sl))) {
            if (sl == SetLanguage_PTBR || sl == SetLanguage_PT) lang = Lang::PtBR;
        }
        setExit();
    }
    return lang;
#else
    return Lang::EnUS;
#endif
}

MenuView::~MenuView() { shutdown(); }

bool MenuView::init(SDL_Renderer* renderer) {
    r_ = renderer;
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return false;
    }
    ttf_ = true;
#ifdef __SWITCH__
    PlFontData pf{};
    if (R_SUCCEEDED(plInitialize(PlServiceType_User)) &&
        R_SUCCEEDED(plGetSharedFontByType(&pf, PlSharedFontType_Standard)) && pf.address) {
        fontData_.assign(static_cast<const char*>(pf.address), pf.size);
    } else {
        std::fprintf(stderr, "shared system font unavailable; menu text will not draw\n");
    }
#else
    std::fprintf(stderr, "no system font on this platform; menu text will not draw\n");
#endif
    return !fontData_.empty();
}

void MenuView::shutdown() {
    for (auto& [k, c] : cache_)
        if (c.tex) SDL_DestroyTexture(c.tex);
    cache_.clear();
    for (auto& [k, f] : fonts_)
        if (f) TTF_CloseFont(f);
    fonts_.clear();
    if (ttf_) {
        TTF_Quit();
        ttf_ = false;
#ifdef __SWITCH__
        plExit();
#endif
    }
    fontData_.clear();
}

TTF_Font* MenuView::font(int px) {
    if (fontData_.empty()) return nullptr;
    auto it = fonts_.find(px);
    if (it != fonts_.end()) return it->second;
    SDL_RWops* rw = SDL_RWFromConstMem(fontData_.data(), static_cast<int>(fontData_.size()));
    TTF_Font* f = rw ? TTF_OpenFontRW(rw, 1, px) : nullptr;
    if (!f) std::fprintf(stderr, "TTF_OpenFontRW(%d) failed: %s\n", px, TTF_GetError());
    fonts_[px] = f;
    return f;
}

const MenuView::Cached* MenuView::text(const std::string& s, int px, SDL_Color c) {
    if (s.empty()) return nullptr;
    char key[32];
    std::snprintf(key, sizeof key, "%d|%02x%02x%02x|", px, c.r, c.g, c.b);
    const std::string k = std::string(key) + s;
    auto it = cache_.find(k);
    if (it != cache_.end()) return it->second.tex ? &it->second : nullptr;
    // Messages are few and fixed; the bound only guards against an odd game title churn.
    if (cache_.size() > 64) {
        for (auto& [kk, cc] : cache_)
            if (cc.tex) SDL_DestroyTexture(cc.tex);
        cache_.clear();
    }
    Cached out;
    if (TTF_Font* f = font(px)) {
        if (SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s.c_str(), c)) {
            out.tex = SDL_CreateTextureFromSurface(r_, surf);
            out.w = surf->w;
            out.h = surf->h;
            SDL_FreeSurface(surf);
        }
    }
    auto& slot = cache_[k] = out;
    return slot.tex ? &slot : nullptr;
}

int MenuView::textWidth(const std::string& s, int px) {
    int w = 0, h = 0;
    if (TTF_Font* f = font(px)) TTF_SizeUTF8(f, s.c_str(), &w, &h);
    return w;
}

std::string MenuView::fit(const std::string& s, int px, int maxW) {
    if (textWidth(s, px) <= maxW) return s;
    std::string t = s;
    while (!t.empty()) {
        size_t n = t.size() - 1;  // drop one whole UTF-8 character
        while (n > 0 && (static_cast<unsigned char>(t[n]) & 0xC0) == 0x80) --n;
        t.resize(n);
        if (textWidth(t + "...", px) <= maxW) break;
    }
    return t + "...";
}

void MenuView::drawText(const std::string& s, int px, SDL_Color c, int x, int y, int align) {
    const Cached* t = text(s, px, c);
    if (!t) return;
    int dx = x;
    if (align == 1) dx = x - t->w / 2;
    else if (align == 2) dx = x - t->w;
    const SDL_Rect dst{dx, y, t->w, t->h};
    SDL_RenderCopy(r_, t->tex, nullptr, &dst);
}

// Rounded rectangle: a middle band plus one inset scanline per corner row.
void MenuView::fillRound(int x, int y, int w, int h, int r, SDL_Color c) {
    r = std::min({r, w / 2, h / 2});
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
    const SDL_Rect mid{x, y + r, w, h - 2 * r};
    SDL_RenderFillRect(r_, &mid);
    for (int i = 0; i < r; ++i) {
        const double dy = r - i - 0.5;
        const int inset = r - static_cast<int>(std::lround(std::sqrt(double(r) * r - dy * dy)));
        const SDL_Rect top{x + inset, y + i, w - 2 * inset, 1};
        const SDL_Rect bot{x + inset, y + h - 1 - i, w - 2 * inset, 1};
        SDL_RenderFillRect(r_, &top);
        SDL_RenderFillRect(r_, &bot);
    }
}

void MenuView::badgeA(int cx, int cy, int r) {
    fillRound(cx - r, cy - r, 2 * r, 2 * r, r, kAmber);
    drawText("A", r * 3 / 2, kPanel, cx, cy - r * 3 / 4 - 2, 1);
}

void MenuView::drawMenu(const Menu& menu, const std::string& gameTitle, Lang lang) {
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r_, 0, 0, 0, 170);
    const SDL_Rect all{0, 0, kScreenW, kScreenH};
    SDL_RenderFillRect(r_, &all);

    const int panelH = kPad * 2 + 56 + 4 * kRow;
    const int px = (kScreenW - kPanelW) / 2, py = (kScreenH - panelH) / 2;
    fillRound(px, py, kPanelW, panelH, 12, kPanel);

    drawText(fit(gameTitle, 26, kPanelW - 2 * kPad), 26, kTitle, px + kPad, py + kPad);

    const Str labels[4] = {Str::Continue, Str::SaveState, Str::LoadState, Str::Exit};
    int y = py + kPad + 56;
    for (int i = 0; i < 4; ++i, y += kRow) {
        const bool disabled = i == static_cast<int>(MenuItem::LoadState) && !menu.hasState();
        const bool sel = static_cast<int>(menu.selected()) == i;
        const SDL_Color c = disabled ? kMuted : (sel ? kAmber : kText);
        if (sel) fillRound(px + 12, y, kPanelW - 24, kRow - 8, 10, kAmberSoft);
        drawText(tr(lang, labels[i]), 32, c, px + kPad + 8, y + 8);
    }
}

void MenuView::drawToast(const std::string& msg) {
    const int w = std::max(240, textWidth(msg, 28) + 64), h = 56;
    const int y = kScreenH - h - 40;
    fillRound((kScreenW - w) / 2, y, w, h, 12, SDL_Color{0x17, 0x1c, 0x28, 235});
    drawText(msg, 28, kText, kScreenW / 2, y + 8, 1);
}

void MenuView::drawError(const std::string& message, bool withBack, Lang lang) {
    SDL_SetRenderDrawColor(r_, 0x0d, 0x10, 0x18, 255);
    SDL_RenderClear(r_);
    const int w = 900, h = withBack ? 260 : 190;
    const int px = (kScreenW - w) / 2, py = (kScreenH - h) / 2;
    fillRound(px, py, w, h, 12, kPanel);
    drawText(fit(message, 34, w - 2 * kPad), 34, kText, kScreenW / 2, py + 52, 1);
    if (withBack) {
        const std::string label = tr(lang, Str::BackToEquinox);
        const int lw = textWidth(label, 30);
        const int x0 = (kScreenW - (44 + lw)) / 2;
        badgeA(x0 + 18, py + 188, 18);
        drawText(label, 30, kAmber, x0 + 44, py + 168);
    }
}
