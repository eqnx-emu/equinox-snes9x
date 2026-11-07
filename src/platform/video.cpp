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
constexpr int kRoomDiv = 8;       // the room texture is screen / kRoomDiv, drawn linear
constexpr int kArcSteps = 12;     // segments per rounded corner
constexpr int kOutlinePoints = 4 * (kArcSteps + 1);
constexpr float kPi = 3.14159265f;

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

Uint8 channel(float v) { return static_cast<Uint8>(std::clamp(v + 0.5f, 0.0f, 255.0f)); }

SDL_Color color(Rgb c, float a = 1.0f) {
    return SDL_Color{channel(c.r), channel(c.g), channel(c.b), channel(a * 255.0f)};
}

// The dim room behind the TV, at 1/kRoomDiv of the screen.
Image tvRoom(int screenW, int screenH) {
    Image img;
    img.w = std::max(1, screenW / kRoomDiv);
    img.h = std::max(1, screenH / kRoomDiv);
    img.px.resize(static_cast<size_t>(img.w) * img.h);
    const float cx = img.w * 0.5f, cy = img.h * 0.47f;
    const float inner = kRoomInner * screenH / 720.0f / kRoomDiv, outer = img.w * 0.75f;
    for (int y = 0; y < img.h; ++y)
        for (int x = 0; x < img.w; ++x) {
            const float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
            const SDL_Color c =
                color(mix(kRoomCenter, kRoomEdge, std::clamp((d - inner) / (outer - inner), 0.0f, 1.0f)));
            img.px[static_cast<size_t>(y) * img.w + x] =
                0xFF000000u | uint32_t{c.r} << 16 | uint32_t{c.g} << 8 | c.b;
        }
    return img;
}

// Triangles with per-vertex colors, drawn in one SDL_RenderGeometry call.
struct MeshBuilder {
    std::vector<SDL_Vertex>& v;
    std::vector<int>& idx;

    int add(float x, float y, SDL_Color c) {
        v.push_back(SDL_Vertex{SDL_FPoint{x, y}, c, SDL_FPoint{0, 0}});
        return static_cast<int>(v.size()) - 1;
    }
    void tri(int a, int b, int c) { idx.insert(idx.end(), {a, b, c}); }
    // Quads between two runs of n points that start at a and b.
    void strip(int a, int b, int n, bool closed) {
        for (int i = 0; i < n - 1 + (closed ? 1 : 0); ++i) {
            const int j = (i + 1) % n;
            tri(a + i, b + i, b + j);
            tri(a + i, b + j, a + j);
        }
    }
};

// Corner k of a rounded rectangle (TL, TR, BR, BL): arc center and start angle.
void cornerArc(int k, float x, float y, float w, float h, float r, float& cx, float& cy, float& a0) {
    cx = (k == 1 || k == 2) ? x + w - r : x + r;
    cy = (k >= 2) ? y + h - r : y + r;
    a0 = kPi + k * kPi * 0.5f;
}

// Points of corner k's arc at radius `radius`, from one edge to the other.
template <class ColorAt>
int arc(MeshBuilder& m, int k, float x, float y, float w, float h, float r, float radius, ColorAt colorAt) {
    float cx, cy, a0;
    cornerArc(k, x, y, w, h, r, cx, cy, a0);
    const int first = static_cast<int>(m.v.size());
    for (int i = 0; i <= kArcSteps; ++i) {
        const float a = a0 + kPi * 0.5f * i / kArcSteps;
        const float py = cy + std::sin(a) * radius;
        m.add(cx + std::cos(a) * radius, py, colorAt(py));
    }
    return first;
}

// Outline of a rounded rectangle pushed `offset` pixels outwards, clockwise.
template <class ColorAt>
int outline(MeshBuilder& m, float x, float y, float w, float h, float r, float offset, ColorAt colorAt) {
    const int first = static_cast<int>(m.v.size());
    for (int k = 0; k < 4; ++k) arc(m, k, x, y, w, h, r, r + offset, colorAt);
    return first;
}

// Rounded rectangle filled with a vertical gradient over its own height, with a
// one-pixel antialiased edge. Colors are linear in y, so the fan interpolates exactly.
void fillRound(MeshBuilder& m, float x, float y, float w, float h, float r, Rgb top, Rgb bottom) {
    auto at = [&](float py, float a) {
        return color(mix(top, bottom, std::clamp((py - y) / h, 0.0f, 1.0f)), a);
    };
    const int c = m.add(x + w * 0.5f, y + h * 0.5f, at(y + h * 0.5f, 1.0f));
    const int in = outline(m, x, y, w, h, r, -0.5f, [&](float py) { return at(py, 1.0f); });
    const int out = outline(m, x, y, w, h, r, 0.5f, [&](float py) { return at(py, 0.0f); });
    for (int i = 0; i < kOutlinePoints; ++i) m.tri(c, in + i, in + (i + 1) % kOutlinePoints);
    m.strip(in, out, kOutlinePoints, true);
}

// A band of constant color along a rounded rectangle's edge, from offset a to b.
void ring(MeshBuilder& m, float x, float y, float w, float h, float r, float a, float b, SDL_Color c) {
    auto same = [c](float) { return c; };
    const int in = outline(m, x, y, w, h, r, a, same);
    const int out = outline(m, x, y, w, h, r, b, same);
    m.strip(in, out, kOutlinePoints, true);
}

// The bezel, its lit edge, the dark rim and the LED around the game.
void tvBackMesh(MeshBuilder& m, const Rect& g, float s) {
    const float bx = g.x - kBezelX * s, by = g.y - kBezelY * s;
    const float bw = g.w + 2 * kBezelX * s, bh = g.h + 2 * kBezelY * s;
    fillRound(m, bx, by, bw, bh, kBezelRadius * s, kBezelTop, kBezelBottom);
    ring(m, bx, by, bw, bh, kBezelRadius * s, -s, s, color(kWhite, kBezelEdgeAlpha));
    const float in = kRimInset * s;
    fillRound(m, g.x - in, g.y - in, g.w + 2 * in, g.h + 2 * in, kRimRadius * s, kRim, kRim);
    const float lx = g.x + g.w + kLedX * s, ly = g.y + g.h + kLedY * s, l = kLedSize * s;
    const SDL_Color led = color(kLed);
    const int a = m.add(lx, ly, led), b = m.add(lx + l, ly, led);
    const int c = m.add(lx + l, ly + l, led), d = m.add(lx, ly + l, led);
    m.tri(a, b, c);
    m.tri(a, c, d);
}

// Over the game, clipped to it: rim-colored rounded screen corners and a faint glare
// from the top left.
void tvFrontMesh(MeshBuilder& m, const Rect& g, float s) {
    const float r = kScreenRadius * s;
    const float x = static_cast<float>(g.x), y = static_cast<float>(g.y);
    const float w = static_cast<float>(g.w), h = static_cast<float>(g.h);
    const float kx[4] = {x, x + w, x + w, x}, ky[4] = {y, y, y + h, y + h};
    const SDL_Color solid = color(kRim), clear = color(kRim, 0.0f);
    for (int k = 0; k < 4; ++k) {
        const int corner = m.add(kx[k], ky[k], solid);
        const int edge = arc(m, k, x, y, w, h, r, r + 0.5f, [&](float) { return solid; });
        const int fade = arc(m, k, x, y, w, h, r, r - 0.5f, [&](float) { return clear; });
        for (int i = 0; i < kArcSteps; ++i) m.tri(corner, edge + i, edge + i + 1);
        m.strip(edge, fade, kArcSteps + 1, false);
    }
    // Glare: alpha falls linearly along (0.6w, 0.6h) and reaches 0 halfway there,
    // which is the line through the two far points.
    const float reach = 0.3f * (w * w + h * h);
    const int o = m.add(x, y, color(kWhite, kGlareAlpha));
    const int ex = m.add(x + reach / w, y, color(kWhite, 0.0f));
    const int ey = m.add(x, y + reach / h, color(kWhite, 0.0f));
    m.tri(o, ex, ey);
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
    if (SDL_GetRendererOutputSize(renderer_, &w, &h) != 0) SDL_GetWindowSize(window, &w, &h);
    screenW_ = screenH_ = 0;
    setScreen(w > 0 ? w : 1280, h > 0 ? h : 720);
    return true;
}

Video::~Video() {
    for (SDL_Texture** t : {&tex_, &sharp_, &scan_, &backdrop_, &cover_, &tvRoom_})
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
    destroy(tvRoom_);
    tvRoomFailed_ = false;
    tvRect_ = Rect{0, 0, 0, 0};
}

void Video::setSettings(const VideoSettings& settings) {
    settings_ = settings;
    if (settings_.smooth != Smooth::Sharp) {  // the prescale target is only for Sharp
        destroy(sharp_);
        sharpW_ = sharpH_ = 0;
    }
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

// The room is a tiny texture made once per screen; the frame is a few hundred vertices
// remade when the game rectangle moves, so 224/239-line switches cost next to nothing.
void Video::ensureTv(const Rect& game) {
    if (!tvRoom_ && !tvRoomFailed_) {
        tvRoom_ = makeTexture(tvRoom(screenW_, screenH_), SDL_ScaleModeLinear, SDL_BLENDMODE_NONE);
        tvRoomFailed_ = !tvRoom_;  // retried only for a new screen size
    }
    if (game == tvRect_) return;
    tvRect_ = game;
    const float s = screenH_ / 720.0f;
    tvBackV_.clear();
    tvBackI_.clear();
    MeshBuilder back{tvBackV_, tvBackI_};
    tvBackMesh(back, game, s);
    tvFrontV_.clear();
    tvFrontI_.clear();
    MeshBuilder front{tvFrontV_, tvFrontI_};
    tvFrontMesh(front, game, s);
}

void Video::drawMesh(const std::vector<SDL_Vertex>& v, const std::vector<int>& idx) {
    if (v.empty()) return;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer_, nullptr, v.data(), static_cast<int>(v.size()), idx.data(),
                       static_cast<int>(idx.size()));
}

void Video::drawBackground(const Rect& game) {
    if (settings_.size == Size::Full) return;
    if (tvFrameShown()) {
        if (game.w <= 0) return;
        ensureTv(game);
        if (tvRoom_) SDL_RenderCopy(renderer_, tvRoom_, nullptr, nullptr);
        drawMesh(tvBackV_, tvBackI_);
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
        // Enlarge by whole factors per axis with nearest into a target, then the target to
        // the screen with linear: only the last, fractional step blends neighbours.
        const Prescale k = sharpPrescale(w_, h_, game);
        const unsigned tw = w_ * static_cast<unsigned>(k.x), th = h_ * static_cast<unsigned>(k.y);
        if (sharpW_ != tw || sharpH_ != th) {  // a failed size is not retried every frame
            destroy(sharp_);
            sharpW_ = tw;
            sharpH_ = th;
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
    if (rows != scanRows_) {  // a failed mask is not retried every frame
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
        if (tvFrameShown()) {
            const SDL_Rect clip = toSdl(game);
            SDL_RenderSetClipRect(renderer_, &clip);
            drawMesh(tvFrontV_, tvFrontI_);
            SDL_RenderSetClipRect(renderer_, nullptr);
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
