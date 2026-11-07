#include "platform/video.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
// 3x5 digit glyphs, one row per 3 bits.
constexpr unsigned char kDigits[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};

// The TV frame, in 720p pixels; scaled with the screen height.
constexpr float kBezelX = 34, kBezelY = 22, kBezelRadius = 30;
constexpr float kRimInset = 8, kRimRadius = 22;
constexpr float kScreenRadius = 18;
constexpr float kLedX = 16, kLedY = 4, kLedSize = 6;
constexpr float kRoomInner = 80;  // radius where the room gradient starts

struct Rgb {
    float r, g, b;
};
constexpr Rgb kRoomCenter{0x2a, 0x22, 0x33}, kRoomEdge{0x0c, 0x0a, 0x10};
constexpr Rgb kBezelTop{0x34, 0x31, 0x3b}, kBezelBottom{0x14, 0x12, 0x17};
constexpr Rgb kRim{0x0a, 0x09, 0x0c}, kLed{0xf5, 0xb5, 0x44}, kWhite{255, 255, 255};
constexpr float kBezelEdgeAlpha = 0.07f, kGlareAlpha = 0.07f;

Rgb mix(Rgb a, Rgb b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

uint32_t argb(Rgb c, float a = 1.0f) {
    auto ch = [](float v) { return static_cast<uint32_t>(std::clamp(v + 0.5f, 0.0f, 255.0f)); };
    return ch(a * 255.0f) << 24 | ch(c.r) << 16 | ch(c.g) << 8 | ch(c.b);
}

// Signed distance from (px, py) to a rounded rectangle; negative inside.
float roundBoxDistance(float px, float py, float x, float y, float w, float h, float radius) {
    const float hw = w * 0.5f, hh = h * 0.5f;
    const float qx = std::fabs(px - (x + hw)) - (hw - radius);
    const float qy = std::fabs(py - (y + hh)) - (hh - radius);
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - radius;
}

// How much of a pixel lies inside an edge `distance` away (one pixel of antialiasing).
float coverage(float distance) { return std::clamp(0.5f - distance, 0.0f, 1.0f); }

// Opaque, full screen: a dim room, the bezel and the dark rim around the game.
Image tvBack(int screenW, int screenH, const Rect& g) {
    const float s = screenH / 720.0f;
    const float bx = g.x - kBezelX * s, by = g.y - kBezelY * s;
    const float bw = g.w + 2 * kBezelX * s, bh = g.h + 2 * kBezelY * s;
    const float rx = g.x - kRimInset * s, ry = g.y - kRimInset * s;
    const float rw = g.w + 2 * kRimInset * s, rh = g.h + 2 * kRimInset * s;
    const float ledX = g.x + g.w + kLedX * s, ledY = g.y + g.h + kLedY * s, led = kLedSize * s;
    const float cx = screenW * 0.5f, cy = screenH * 0.47f;
    const float inner = kRoomInner * s, outer = screenW * 0.75f;

    Image img;
    img.w = screenW;
    img.h = screenH;
    img.px.resize(static_cast<size_t>(screenW) * screenH);
    for (int y = 0; y < screenH; ++y) {
        const float py = y + 0.5f;
        const Rgb bezel = mix(kBezelTop, kBezelBottom, std::clamp((py - by) / bh, 0.0f, 1.0f));
        for (int x = 0; x < screenW; ++x) {
            const float px = x + 0.5f;
            const float dist = std::hypot(px - cx, py - cy);
            Rgb c = mix(kRoomCenter, kRoomEdge, std::clamp((dist - inner) / (outer - inner), 0.0f, 1.0f));
            const float d = roundBoxDistance(px, py, bx, by, bw, bh, kBezelRadius * s);
            c = mix(c, bezel, coverage(d));
            c = mix(c, kWhite, kBezelEdgeAlpha * std::clamp(s + 0.5f - std::fabs(d), 0.0f, 1.0f));
            c = mix(c, kRim, coverage(roundBoxDistance(px, py, rx, ry, rw, rh, kRimRadius * s)));
            if (px >= ledX && px < ledX + led && py >= ledY && py < ledY + led) c = kLed;
            img.px[static_cast<size_t>(y) * screenW + x] = argb(c);
        }
    }
    return img;
}

// Game sized, blended over the game: rim-colored rounded corners and a faint glare
// from the top left.
Image tvFront(int screenH, const Rect& g) {
    const float s = screenH / 720.0f;
    const float gx = g.w * 0.6f, gy = g.h * 0.6f, glen2 = gx * gx + gy * gy;

    Image img;
    img.w = g.w;
    img.h = g.h;
    img.px.resize(static_cast<size_t>(g.w) * g.h);
    for (int y = 0; y < g.h; ++y) {
        const float py = y + 0.5f;
        for (int x = 0; x < g.w; ++x) {
            const float px = x + 0.5f;
            const float corner =
                1.0f - coverage(roundBoxDistance(px, py, 0, 0, static_cast<float>(g.w),
                                                 static_cast<float>(g.h), kScreenRadius * s));
            const float t = (px * gx + py * gy) / glen2;
            const float glare = t < 0.5f ? kGlareAlpha * (1.0f - t / 0.5f) : 0.0f;
            const float a = corner + glare * (1.0f - corner);
            uint32_t out = 0;
            if (a > 0.0f) out = argb(mix(kRim, kWhite, glare * (1.0f - corner) / a), a);
            img.px[static_cast<size_t>(y) * g.w + x] = out;
        }
    }
    return img;
}

void destroy(SDL_Texture*& t) {
    if (t) SDL_DestroyTexture(t);
    t = nullptr;
}

SDL_Rect toSdl(const Rect& r) { return SDL_Rect{r.x, r.y, r.w, r.h}; }
}  // namespace

bool Video::init(SDL_Window* window, bool showFps) {
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
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    screenW_ = screenH_ = 0;
    setScreen(w > 0 ? w : 1280, h > 0 ? h : 720);
    return true;
}

Video::~Video() {
    for (SDL_Texture** t : {&tex_, &sharp_, &scan_, &backdrop_, &cover_, &tvBack_, &tvFront_})
        destroy(*t);
    if (renderer_) SDL_DestroyRenderer(renderer_);
}

SDL_Texture* Video::makeTexture(const Image& img, SDL_ScaleMode scale, SDL_BlendMode blend) {
    if (!renderer_ || img.w <= 0 || img.h <= 0) return nullptr;
    SDL_Texture* t = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                       img.w, img.h);
    if (!t) {
        std::fprintf(stderr, "SDL_CreateTexture %dx%d failed: %s\n", img.w, img.h, SDL_GetError());
        return nullptr;
    }
    SDL_UpdateTexture(t, nullptr, img.px.data(), img.w * 4);
    SDL_SetTextureScaleMode(t, scale);
    SDL_SetTextureBlendMode(t, blend);
    return t;
}

void Video::setScreen(int w, int h) {
    if (w == screenW_ && h == screenH_) return;
    screenW_ = w;
    screenH_ = h;
    destroy(backdrop_);
    backdrop_ = makeTexture(equinoxBackdrop(w / 4, h / 4), SDL_ScaleModeNearest, SDL_BLENDMODE_NONE);
    destroy(tvBack_);
    destroy(tvFront_);
    tvRect_ = Rect{0, 0, 0, 0};
}

void Video::setSettings(const VideoSettings& settings) {
    settings_ = settings;
    applyGameScaleMode();
}

void Video::setCover(Image blurred) {
    destroy(cover_);
    cover_ = makeTexture(blurred, SDL_ScaleModeLinear, SDL_BLENDMODE_NONE);
}

void Video::applyGameScaleMode() {
    if (tex_)
        SDL_SetTextureScaleMode(tex_, settings_.smooth == Smooth::Soft ? SDL_ScaleModeLinear
                                                                      : SDL_ScaleModeNearest);
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
        SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_NONE);
        applyGameScaleMode();
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

bool Video::tvFrameShown() const {
    return settings_.size == Size::Original && effectiveBorder(settings_, hasCover()) == Border::Tv;
}

void Video::ensureTv(const Rect& game) {
    if (tvBack_ && tvFront_ && game == tvRect_) return;
    destroy(tvBack_);
    destroy(tvFront_);
    tvRect_ = game;
    tvBack_ = makeTexture(tvBack(screenW_, screenH_, game), SDL_ScaleModeNearest, SDL_BLENDMODE_NONE);
    tvFront_ = makeTexture(tvFront(screenH_, game), SDL_ScaleModeNearest, SDL_BLENDMODE_BLEND);
}

void Video::drawBackground(const Rect& game) {
    if (settings_.size == Size::Full) return;
    if (tvFrameShown()) {
        if (game.w <= 0) return;
        ensureTv(game);
        if (tvBack_) SDL_RenderCopy(renderer_, tvBack_, nullptr, nullptr);
        return;
    }
    switch (effectiveBorder(settings_, hasCover())) {
        case Border::Equinox:
            if (backdrop_) SDL_RenderCopy(renderer_, backdrop_, nullptr, nullptr);
            break;
        case Border::Cover: SDL_RenderCopy(renderer_, cover_, nullptr, nullptr); break;
        case Border::Tv:  // the bezel only fits around Original; elsewhere the sides stay black
        case Border::Black: break;
    }
}

void Video::drawFrame(const Rect& game) {
    const SDL_Rect dst = toSdl(game);
    if (settings_.smooth == Smooth::Sharp) {
        // Enlarge k times with nearest into a target, then the target to the screen with
        // linear: only the last, fractional step blends neighbours.
        const unsigned k = static_cast<unsigned>(sharpPrescale(w_, h_, game));
        if (!sharp_ || sharpW_ != w_ * k || sharpH_ != h_ * k) {
            destroy(sharp_);
            sharpW_ = w_ * k;
            sharpH_ = h_ * k;
            sharp_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                       static_cast<int>(sharpW_), static_cast<int>(sharpH_));
            if (sharp_) {
                SDL_SetTextureScaleMode(sharp_, SDL_ScaleModeLinear);
                SDL_SetTextureBlendMode(sharp_, SDL_BLENDMODE_NONE);
            } else {
                std::fprintf(stderr, "sharp target %ux%u failed: %s\n", sharpW_, sharpH_,
                             SDL_GetError());
            }
        }
        if (sharp_ && SDL_SetRenderTarget(renderer_, sharp_) == 0) {
            SDL_RenderCopy(renderer_, tex_, nullptr, nullptr);
            SDL_SetRenderTarget(renderer_, nullptr);
            SDL_RenderCopy(renderer_, sharp_, nullptr, &dst);
            return;
        }
    }
    SDL_RenderCopy(renderer_, tex_, nullptr, &dst);
}

void Video::drawScanlines(const Rect& game) {
    const unsigned rows = logicalH(h_);
    if (!scan_ || rows != scanRows_) {
        destroy(scan_);
        scanRows_ = rows;
        scan_ = makeTexture(scanlineMask(rows), SDL_ScaleModeNearest, SDL_BLENDMODE_BLEND);
    }
    if (!scan_) return;
    const SDL_Rect dst = toSdl(game);
    SDL_RenderCopy(renderer_, scan_, nullptr, &dst);
}

void Video::drawGame() {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    const Rect game = tex_ ? gameRect(settings_.size, w_, h_, screenW_, screenH_) : Rect{0, 0, 0, 0};
    drawBackground(game);
    if (game.w > 0) {
        drawFrame(game);
        if (settings_.filter == Filter::Scanlines) drawScanlines(game);
        if (tvFrameShown() && tvFront_) {
            const SDL_Rect dst = toSdl(game);
            SDL_RenderCopy(renderer_, tvFront_, nullptr, &dst);
        }
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
