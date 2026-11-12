#include "platform/menu_view.hpp"

#include "menu_text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace {
constexpr SDL_Color kPanel{0x14, 0x1a, 0x29, 255};
constexpr SDL_Color kLine{0x2a, 0x33, 0x50, 255};
constexpr SDL_Color kCard{0x1b, 0x22, 0x34, 255};
constexpr SDL_Color kAmber{0xf5, 0xb5, 0x44, 255};
constexpr SDL_Color kAmberSoft{0xf5, 0xb5, 0x44, 36};
constexpr SDL_Color kText{0xe8, 0xeb, 0xf2, 255};
constexpr SDL_Color kSecondary{0x9a, 0xa3, 0xb8, 255};
constexpr SDL_Color kDisabled{0x6b, 0x74, 0x88, 255};
constexpr SDL_Color kDesc{0xc9, 0xcf, 0xdc, 255};
constexpr SDL_Color kDot{0x4a, 0x54, 0x70, 255};
constexpr SDL_Color kSegment{0x0e, 0x13, 0x20, 255};
constexpr int kScreenW = 1280, kScreenH = 720;
constexpr int kPauseW = 480, kVideoW = 560, kPad = 32;
constexpr Uint32 kSlideMs = 180;

struct Seg {
    float x0, y0, x1, y1;
};

// Icons are 2-unit strokes on a 24-unit grid, the way line icons are drawn.
constexpr Seg kPlay[] = {{7, 4.5f, 7, 19.5f}, {7, 19.5f, 19, 12}, {19, 12, 7, 4.5f}};
constexpr Seg kSave[] = {{12, 3, 12, 14}, {7.5f, 9.5f, 12, 14}, {12, 14, 16.5f, 9.5f},
                         {4, 15, 4, 19}, {4, 19, 20, 19}, {20, 19, 20, 15}};
constexpr Seg kLoad[] = {{12, 14, 12, 3}, {7.5f, 7.5f, 12, 3}, {12, 3, 16.5f, 7.5f},
                         {4, 15, 4, 19}, {4, 19, 20, 19}, {20, 19, 20, 15}};
constexpr Seg kScreen[] = {{3, 4, 21, 4}, {21, 4, 21, 16}, {21, 16, 3, 16}, {3, 16, 3, 4},
                           {9, 20, 15, 20}, {12, 16, 12, 20}};
constexpr Seg kExit[] = {{14, 4, 5, 4}, {5, 4, 5, 20}, {5, 20, 14, 20}, {10, 12, 20, 12},
                         {16.5f, 8.5f, 20, 12}, {20, 12, 16.5f, 15.5f}};

std::pair<const Seg*, size_t> segments(MenuItem icon) {
    switch (icon) {
        case MenuItem::Continue: return {kPlay, std::size(kPlay)};
        case MenuItem::SaveState: return {kSave, std::size(kSave)};
        case MenuItem::LoadState: return {kLoad, std::size(kLoad)};
        case MenuItem::Video: return {kScreen, std::size(kScreen)};
        case MenuItem::Exit: return {kExit, std::size(kExit)};
    }
    return {kPlay, 0};
}

float distToSeg(float x, float y, const Seg& s) {
    const float dx = s.x1 - s.x0, dy = s.y1 - s.y0;
    const float len2 = dx * dx + dy * dy;
    float t = len2 > 0 ? ((x - s.x0) * dx + (y - s.y0) * dy) / len2 : 0;
    t = std::clamp(t, 0.0f, 1.0f);
    const float ex = s.x0 + t * dx - x, ey = s.y0 + t * dy - y;
    return std::sqrt(ex * ex + ey * ey);
}

// Whether a point of the icon is inked; x and y go from 0 to 24.
bool iconCovers(MenuItem icon, float x, float y) {
    const auto [segs, n] = segments(icon);
    for (size_t i = 0; i < n; ++i)
        if (distToSeg(x, y, segs[i]) <= 1.0f) return true;
    return false;
}
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
    if (thumbTex_) SDL_DestroyTexture(thumbTex_);
    thumbTex_ = nullptr;
    thumbVersion_ = -1;
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

void MenuView::setScreenHeight(int h) {
    const float s = h > 0 ? h / float(kScreenH) : 1.0f;
    if (s == scale_) return;
    scale_ = s;
    // Text textures were rendered for the old size.
    for (auto& [k, c] : cache_)
        if (c.tex) SDL_DestroyTexture(c.tex);
    cache_.clear();
}

MenuView::Scaled::Scaled(const MenuView& v) : r_(v.r_) { SDL_RenderSetScale(r_, v.scale_, v.scale_); }
MenuView::Scaled::~Scaled() { SDL_RenderSetScale(r_, 1.0f, 1.0f); }

int MenuView::screenPx(int px) const { return static_cast<int>(std::lround(px * scale_)); }

// Fonts open at the screen's pixel size, so docked text is rendered at 1080p, not enlarged.
TTF_Font* MenuView::font(int px) {
    if (fontData_.empty()) return nullptr;
    const int sp = screenPx(px);
    auto it = fonts_.find(sp);
    if (it != fonts_.end()) return it->second;
    SDL_RWops* rw = SDL_RWFromConstMem(fontData_.data(), static_cast<int>(fontData_.size()));
    TTF_Font* f = rw ? TTF_OpenFontRW(rw, 1, sp) : nullptr;
    if (!f) std::fprintf(stderr, "TTF_OpenFontRW(%d) failed: %s\n", sp, TTF_GetError());
    fonts_[sp] = f;
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
    return static_cast<int>(std::ceil(w / scale_));
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
    // The texture is in screen pixels: place it on whole screen pixels so it copies 1:1.
    const float w = t->w / scale_, h = t->h / scale_;
    float dx = static_cast<float>(x);
    if (align == 1) dx -= w / 2;
    else if (align == 2) dx -= w;
    const auto snap = [&](float v) { return std::round(v * scale_) / scale_; };
    const SDL_FRect dst{snap(dx), snap(static_cast<float>(y)), w, h};
    SDL_RenderCopyF(r_, t->tex, nullptr, &dst);
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

void MenuView::drawTextMid(const std::string& s, int px, SDL_Color c, int x, int cy, int align) {
    const Cached* t = text(s, px, c);
    if (!t) return;
    drawText(s, px, c, x, cy - static_cast<int>(std::lround(t->h / scale_ / 2)), align);
}

void MenuView::outlineRound(int x, int y, int w, int h, int r, SDL_Color line, SDL_Color inside) {
    fillRound(x, y, w, h, r, line);
    fillRound(x + 1, y + 1, w - 2, h - 2, std::max(0, r - 1), inside);
}

void MenuView::triangle(float x0, float y0, float x1, float y1, float x2, float y2, SDL_Color c) {
    const SDL_Vertex v[3] = {{{x0, y0}, c, {0, 0}}, {{x1, y1}, c, {0, 0}}, {{x2, y2}, c, {0, 0}}};
    SDL_RenderGeometry(r_, nullptr, v, 3, nullptr, 0);
}

void MenuView::badgeA(int cx, int cy, int r) {
    fillRound(cx - r, cy - r, 2 * r, 2 * r, r, kAmber);
    drawTextMid("A", r * 6 / 5, kPanel, cx, cy, 1);
}

void MenuView::drawHints(std::initializer_list<std::pair<Key, std::string>> hints, int rightX, int cy) {
    constexpr int kKey = 26, kGap = 8, kBetween = 24, kPx = 17;
    const auto keyW = [](Key k) { return k == Key::LeftRight ? 46 : kKey; };
    int total = 0;
    for (const auto& [k, label] : hints) total += keyW(k) + kGap + textWidth(label, kPx) + kBetween;
    int x = rightX - (total - kBetween);
    for (const auto& [k, label] : hints) {
        const int w = keyW(k);
        outlineRound(x, cy - kKey / 2, w, kKey, kKey / 2, kSecondary, kPanel);
        outlineRound(x + 1, cy - kKey / 2 + 1, w - 2, kKey - 2, kKey / 2 - 1, kSecondary, kPanel);
        if (k == Key::LeftRight) {
            const float mx = x + w / 2.0f, y0 = static_cast<float>(cy);
            triangle(mx - 5, y0 - 5, mx - 5, y0 + 5, mx - 12, y0, kSecondary);
            triangle(mx + 5, y0 - 5, mx + 5, y0 + 5, mx + 12, y0, kSecondary);
        } else {
            drawTextMid(k == Key::A ? "A" : "B", 14, kSecondary, x + w / 2, cy, 1);
        }
        x += w + kGap;
        drawTextMid(label, kPx, kSecondary, x, cy);
        x += textWidth(label, kPx) + kBetween;
    }
}

void MenuView::drawStateCard(const StateCard& card, int x, int y, int w, Lang lang) {
    constexpr int kH = 112, kThumbW = 112, kThumbH = 84;
    fillRound(x, y, w, kH, 16, kCard);
    const Image* img = card.thumb;
    if (card.thumbVersion != thumbVersion_) {
        if (thumbTex_) SDL_DestroyTexture(thumbTex_);
        thumbTex_ = nullptr;
        thumbVersion_ = card.thumbVersion;
        if (img && img->w > 0 && img->h > 0) {
            thumbTex_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, img->w, img->h);
            if (thumbTex_) {
                SDL_UpdateTexture(thumbTex_, nullptr, img->px.data(), img->w * 4);
                SDL_SetTextureScaleMode(thumbTex_, SDL_ScaleModeLinear);
            }
        }
    }
    const SDL_Rect frame{x + 14, y + 14, kThumbW, kThumbH};
    SDL_SetRenderDrawColor(r_, kLine.r, kLine.g, kLine.b, 255);
    SDL_RenderFillRect(r_, &frame);
    if (thumbTex_ && img && img->w > 0 && img->h > 0) {
        const float k = std::min(kThumbW / float(img->w), kThumbH / float(img->h));
        const float w2 = img->w * k, h2 = img->h * k;
        const SDL_FRect dst{frame.x + (kThumbW - w2) / 2, frame.y + (kThumbH - h2) / 2, w2, h2};
        SDL_RenderCopyF(r_, thumbTex_, nullptr, &dst);
    }
    const int tx = x + 14 + kThumbW + 16;
    drawTextMid(fit(tr(lang, Str::LastState), 18, x + w - tx - 12), 18, kText, tx, y + 44);
    drawTextMid(card.when, 16, kSecondary, tx, y + 72);
}

void MenuView::drawMenu(const Menu& menu, const PauseInfo& info, Lang lang) {
    const Scaled scaled(*this);
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r_, 6, 9, 18, 148);
    const SDL_Rect all{0, 0, kScreenW, kScreenH};
    SDL_RenderFillRect(r_, &all);

    const int px = sheetLeft(kPauseW);
    fillRound(px, 0, kPauseW, kScreenH, 0, kPanel);
    fillRound(px, 0, 1, kScreenH, 0, kLine);
    const int left = px + kPad, width = kPauseW - 2 * kPad, textX = left + 12;

    int y = 40;
    drawText(info.console, 17, kSecondary, textX, y);
    y += 28;
    const auto lines = wrapLines(info.title, width - 24, [&](const std::string& t) { return textWidth(t, 30); }, 2);
    for (const std::string& line : lines) {
        drawText(line, 30, kText, textX, y);
        y += 38;
    }
    y += 4;
    drawText(info.played, 17, kSecondary, textX, y);
    y += 24 + 28;

    constexpr int kRowH = 60, kRowGap = 4;
    const Str labels[5] = {Str::Continue, Str::SaveState, Str::LoadState, Str::Video, Str::Exit};
    for (int i = 0; i < 5; ++i, y += kRowH + kRowGap) {
        const bool disabled = i == static_cast<int>(MenuItem::LoadState) && !menu.hasState();
        const bool sel = static_cast<int>(menu.selected()) == i;
        const SDL_Color c = disabled ? kDisabled : (sel ? kAmber : kText);
        const int cy = y + kRowH / 2;
        if (sel) {
            fillRound(left, y, width, kRowH, 14, kAmberSoft);
            badgeA(left + width - 16 - 15, cy, 15);
        }
        drawIcon(static_cast<MenuItem>(i), left + 16 + 13, cy, 26, c);
        drawTextMid(tr(lang, labels[i]), 24, c, left + 60, cy);
    }
    y += 24 - kRowGap;
    if (info.state) drawStateCard(*info.state, left, y, width, lang);

    drawHints({{Key::A, tr(lang, Str::HintChoose)}, {Key::B, tr(lang, Str::HintBackToGame)}}, left + width,
              kScreenH - 28 - 13);
}

int MenuView::sheetLeft(int width) {
    const Uint32 now = SDL_GetTicks();
    if (now - sheetDrawnAt_ > 100) sheetOpenedAt_ = now;
    sheetDrawnAt_ = now;
    const float t = std::min(1.0f, (now - sheetOpenedAt_) / float(kSlideMs));
    const float eased = 1 - (1 - t) * (1 - t) * (1 - t);
    return kScreenW - static_cast<int>(std::lround(width * eased));
}

// Drawn once per size with 4x4 samples per pixel, white, and tinted when used.
void MenuView::drawIcon(MenuItem icon, int cx, int cy, int size, SDL_Color c) {
    const int n = screenPx(size);
    const std::string key = "icon|" + std::to_string(static_cast<int>(icon)) + "|" + std::to_string(n);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        Cached out;
        if (SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, n, n, 32, SDL_PIXELFORMAT_RGBA32)) {
            SDL_LockSurface(surf);
            for (int y = 0; y < n; ++y) {
                auto* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(surf->pixels) + y * surf->pitch);
                for (int x = 0; x < n; ++x) {
                    int hits = 0;
                    for (int j = 0; j < 4; ++j)
                        for (int i = 0; i < 4; ++i)
                            hits += iconCovers(icon, (x + (i + .5f) / 4) * 24 / n,
                                               (y + (j + .5f) / 4) * 24 / n);
                    row[x] = SDL_MapRGBA(surf->format, 255, 255, 255, static_cast<Uint8>(hits * 255 / 16));
                }
            }
            SDL_UnlockSurface(surf);
            out.tex = SDL_CreateTextureFromSurface(r_, surf);
            if (out.tex) SDL_SetTextureBlendMode(out.tex, SDL_BLENDMODE_BLEND);
            out.w = out.h = n;
            SDL_FreeSurface(surf);
        }
        it = cache_.emplace(key, out).first;
    }
    if (!it->second.tex) return;
    SDL_SetTextureColorMod(it->second.tex, c.r, c.g, c.b);
    const SDL_FRect dst{cx - size / 2.0f, cy - size / 2.0f, float(size), float(size)};
    SDL_RenderCopyF(r_, it->second.tex, nullptr, &dst);
}

void MenuView::drawValueRow(const std::string& name, const std::string& value, bool sel, bool enabled, int x,
                            int y, int w, int h, int valuePx, bool fill) {
    const int cy = y + h / 2, right = x + w - 16;
    if (sel && fill) fillRound(x, y, w, h, 14, kAmberSoft);
    drawTextMid(name, 22, enabled ? (sel ? kAmber : kText) : kDisabled, x + 16, cy);
    if (!enabled) {
        drawTextMid(value, valuePx, kDisabled, right, cy, 2);
        return;
    }
    if (!sel) {
        drawTextMid(value, valuePx, kText, right, cy, 2);
        return;
    }
    // Arrows either side of the value on the current row (drawn: the font may lack the glyphs).
    const float fy = static_cast<float>(cy);
    triangle(right - 8.0f, fy - 6, right - 8.0f, fy + 6, static_cast<float>(right), fy, kAmber);
    const int valueRight = right - 22;
    drawTextMid(value, valuePx, kText, valueRight, cy, 2);
    const float lx = static_cast<float>(valueRight - textWidth(value, valuePx) - 14);
    triangle(lx, fy - 6, lx, fy + 6, lx - 8, fy, kAmber);
}

void MenuView::drawVideoMenu(const VideoMenu& menu, Lang lang) {
    const Scaled scaled(*this);
    const int px = sheetLeft(kVideoW);
    fillRound(px, 0, kVideoW, kScreenH, 0, kPanel);
    fillRound(px, 0, 1, kScreenH, 0, kLine);
    const int left = px + kPad, width = kVideoW - 2 * kPad, right = left + width - 16;

    drawText(tr(lang, Str::VideoTitle), 30, kText, left + 12, 40);
    drawText(tr(lang, Str::VideoHint), 17, kSecondary, left + 12, 84);

    const VideoSettings& s = menu.settings();
    const VideoRow cur = menu.cursor();
    const auto sel = [&](VideoRow r) { return cur == r; };
    constexpr int kH = 56, kGap = 4;
    int y = 130;

    drawValueRow(tr(lang, Str::Size), tr(lang, label(s.size)), sel(VideoRow::Size), true, left, y, width, kH, 22);
    y += kH + kGap;

    drawValueRow(tr(lang, Str::Smoothing), tr(lang, label(s.smooth)), sel(VideoRow::Smooth), true, left, y, width, kH,
                 22);
    y += kH + kGap;

    if (sel(VideoRow::Filter)) {
        // Opened: what the filter looks like, and where it sits in the list.
        constexpr int kOpenH = 92;
        fillRound(left, y, width, kOpenH, 14, kAmberSoft);
        drawValueRow(tr(lang, Str::Filter), tr(lang, label(s.filter)), true, true, left, y + 2, width, 56, 22, false);
        const int cy2 = y + 68;
        // NTSC is the last filter; without it there are two.
        constexpr int kFilters = kNtscEnabled ? 3 : 2;
        constexpr int kDotsW = (kFilters - 1) * 8 + 22 + (kFilters - 1) * 6;
        drawTextMid(fit(tr(lang, description(s.filter)), 16, width - 32 - kDotsW - 16), 16, kDesc, left + 16, cy2);
        int dx = right - kDotsW;
        for (int i = 0; i < kFilters; ++i) {
            const bool on = i == static_cast<int>(s.filter);
            const int dw = on ? 22 : 8;
            fillRound(dx, cy2 - 4, dw, 8, 4, on ? kAmber : kDot);
            dx += dw + 6;
        }
        y += kOpenH + kGap;
    } else {
        drawValueRow(tr(lang, Str::Filter), tr(lang, label(s.filter)), false, true, left, y, width, kH, 22);
        y += kH + kGap;
    }

    const bool borderOn = menu.rowEnabled(VideoRow::Border);
    drawValueRow(tr(lang, Str::Border), borderOn ? tr(lang, label(s.border)) : "—", sel(VideoRow::Border), borderOn,
                 left, y, width, kH, 22);
    y += kH + 18;

    const int bw = (width - 12) / 2;
    const std::pair<VideoRow, Str> buttons[2] = {{VideoRow::UseDefault, Str::UseDefault}, {VideoRow::Back, Str::Back}};
    for (int i = 0; i < 2; ++i) {
        const bool here = sel(buttons[i].first);
        const int bx = left + i * (bw + 12);
        outlineRound(bx, y, bw, 52, 14, here ? kAmber : kLine, kPanel);
        if (here) fillRound(bx + 1, y + 1, bw - 2, 50, 13, kAmberSoft);
        drawTextMid(tr(lang, buttons[i].second), 19, here ? kAmber : kText, bx + bw / 2, y + 26, 1);
    }

    drawHints({{Key::LeftRight, tr(lang, Str::HintChange)}, {Key::B, tr(lang, Str::Back)}}, left + width,
              kScreenH - 28 - 13);
}

void MenuView::drawToast(const std::string& msg) {
    const Scaled scaled(*this);
    const int w = std::max(240, textWidth(msg, 28) + 64), h = 56;
    const int y = kScreenH - h - 40;
    fillRound((kScreenW - w) / 2, y, w, h, 12, SDL_Color{0x17, 0x1c, 0x28, 235});
    drawText(msg, 28, kText, kScreenW / 2, y + 8, 1);
}

void MenuView::drawError(const std::string& message, bool withBack, Lang lang) {
    const Scaled scaled(*this);
    SDL_SetRenderDrawColor(r_, 0x0d, 0x10, 0x18, 255);
    SDL_RenderClear(r_);
    const int w = 900, h = withBack ? 260 : 190;
    const int px = (kScreenW - w) / 2, py = (kScreenH - h) / 2;
    fillRound(px, py, w, h, 12, kPanel);
    drawText(fit(message, 34, w - 2 * 28), 34, kText, kScreenW / 2, py + 52, 1);
    if (withBack) {
        const std::string label = tr(lang, Str::BackToEquinox);
        const int lw = textWidth(label, 30);
        const int x0 = (kScreenW - (44 + lw)) / 2;
        badgeA(x0 + 18, py + 188, 18);
        drawText(label, 30, kAmber, x0 + 44, py + 168);
    }
}
