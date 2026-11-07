#include "platform/video.hpp"

#include <cstdint>
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
    if (!renderer_) return false;
    // The Switch (GLES2) renderer usually lacks RGB565 textures: convert on upload then.
    SDL_RendererInfo info{};
    SDL_GetRendererInfo(renderer_, &info);
    native565_ = false;
    for (Uint32 i = 0; i < info.num_texture_formats; ++i)
        if (info.texture_formats[i] == SDL_PIXELFORMAT_RGB565) native565_ = true;
    std::fprintf(stderr, "renderer %s, RGB565 textures: %s\n", info.name ? info.name : "?",
                 native565_ ? "native" : "converted to ARGB8888");
    if (!native565_) {
        lut_.resize(65536);
        for (unsigned v = 0; v < 65536; ++v) {
            const unsigned r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
            lut_[v] = 0xFF000000u | (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) |
                      ((b << 3) | (b >> 2));
        }
    }
    return true;
}

Video::~Video() {
    if (tex_) SDL_DestroyTexture(tex_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
}

void Video::upload(const void* px, unsigned w, unsigned h, size_t pitch) {
    if (!renderer_ || !px) return;
    if (!tex_ || w != w_ || h != h_) {
        if (tex_) SDL_DestroyTexture(tex_);
        tex_ = SDL_CreateTexture(renderer_,
                                 native565_ ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, static_cast<int>(w),
                                 static_cast<int>(h));
        w_ = w;
        h_ = h;
        if (!tex_) {
            std::fprintf(stderr, "SDL_CreateTexture %ux%u failed: %s\n", w, h, SDL_GetError());
            return;
        }
    }
    if (native565_) {
        SDL_UpdateTexture(tex_, nullptr, px, static_cast<int>(pitch));
        return;
    }
    void* dst = nullptr;
    int dpitch = 0;
    if (SDL_LockTexture(tex_, nullptr, &dst, &dpitch) != 0) {
        std::fprintf(stderr, "SDL_LockTexture failed: %s\n", SDL_GetError());
        return;
    }
    for (unsigned y = 0; y < h; ++y) {
        const auto* src = reinterpret_cast<const uint16_t*>(static_cast<const char*>(px) + y * pitch);
        auto* out = reinterpret_cast<uint32_t*>(static_cast<char*>(dst) + y * dpitch);
        for (unsigned x = 0; x < w; ++x) out[x] = lut_[src[x]];
    }
    SDL_UnlockTexture(tex_);
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

void Video::drawGame() {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    if (tex_) {
        const SDL_Rect dst{160, 0, 960, 720};
        SDL_RenderCopy(renderer_, tex_, nullptr, &dst);
    }
    if (showFps_) drawFps();
}

void Video::flip() {
    if (renderer_) SDL_RenderPresent(renderer_);
}

void Video::present() {
    drawGame();
    flip();
}
