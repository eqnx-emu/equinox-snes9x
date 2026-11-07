#include "platform/video.hpp"

#include <cstdio>
#include <cstring>

namespace {
// 3x5 digit glyphs, one row per 3 bits.
constexpr unsigned char kDigits[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};
}

bool Video::init(SDL_Window* window, bool showFps) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    renderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    showFps_ = showFps;
    lastTick_ = SDL_GetTicks();
    return renderer_ != nullptr;
}

Video::~Video() {
    if (tex_) SDL_DestroyTexture(tex_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
}

void Video::upload(const void* px, unsigned w, unsigned h, size_t pitch) {
    if (!renderer_ || !px) return;
    if (!tex_ || w != w_ || h != h_) {
        if (tex_) SDL_DestroyTexture(tex_);
        tex_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                 static_cast<int>(w), static_cast<int>(h));
        w_ = w;
        h_ = h;
        if (!tex_) return;
    }
    SDL_UpdateTexture(tex_, nullptr, px, static_cast<int>(pitch));
}

void Video::drawFps() {
    ++frames_;
    const Uint32 now = SDL_GetTicks();
    if (now - lastTick_ >= 1000) {
        fps_ = static_cast<int>(frames_ * 1000.0 / (now - lastTick_) + 0.5);
        frames_ = 0;
        lastTick_ = now;
    }
    char buf[8];
    std::snprintf(buf, sizeof buf, "%d", fps_);
    const int scale = 6, x0 = 16, y0 = 16;
    const int digits = static_cast<int>(std::strlen(buf));
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 200);
    const SDL_Rect bg{x0 - 6, y0 - 6, digits * 4 * scale + 8, 5 * scale + 12};
    SDL_RenderFillRect(renderer_, &bg);
    SDL_SetRenderDrawColor(renderer_, 0x40, 0xff, 0x40, 255);
    int x = x0;
    for (const char* c = buf; *c; ++c, x += 4 * scale) {
        for (int r = 0; r < 5; ++r)
            for (int b = 0; b < 3; ++b)
                if (kDigits[*c - '0'][r] & (4 >> b)) {
                    const SDL_Rect px{x + b * scale, y0 + r * scale, scale, scale};
                    SDL_RenderFillRect(renderer_, &px);
                }
    }
}

void Video::present() {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    if (tex_) {
        const SDL_Rect dst{160, 0, 960, 720};
        SDL_RenderCopy(renderer_, tex_, nullptr, &dst);
    }
    if (showFps_) drawFps();
    SDL_RenderPresent(renderer_);
}
