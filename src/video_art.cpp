#include "video_art.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr uint32_t kLight = 0xFF3A2F52u;
constexpr uint32_t kDark = 0xFF1B1828u;
constexpr uint32_t kStar = 0xFFB5A8E0u;

constexpr int kBayer4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

constexpr int kBlurRadius = 2;
constexpr int kBlurPasses = 3;
constexpr int kDarken = 115;  // out of 256, about 0.45

uint32_t hashXY(uint32_t x, uint32_t y) {
    uint32_t h = x * 374761393u + y * 668265263u + 0x9E3779B9u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Box blur of n pixels spaced `step` ints apart (3 channels each), edges clamped.
void boxBlurRow(const int* in, int* out, int n, int step) {
    const int span = 2 * kBlurRadius + 1;
    for (int c = 0; c < 3; c++) {
        const int* src = in + c;
        int* dst = out + c;
        int sum = 0;
        for (int i = -kBlurRadius; i <= kBlurRadius; i++) {
            sum += src[std::clamp(i, 0, n - 1) * step];
        }
        for (int i = 0; i < n; i++) {
            dst[i * step] = sum / span;
            sum += src[std::min(i + kBlurRadius + 1, n - 1) * step];
            sum -= src[std::max(i - kBlurRadius, 0) * step];
        }
    }
}

}  // namespace

Image equinoxBackdrop(int w, int h) {
    Image img;
    if (w <= 0 || h <= 0) return img;
    img.w = w;
    img.h = h;
    img.px.resize(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float f = static_cast<float>(x) / w * 0.45f + static_cast<float>(y) / h * 0.75f;
            f = std::clamp(f, 0.0f, 1.0f);
            bool dark = f * 16.0f > kBayer4[(y % 4) * 4 + (x % 4)];
            uint32_t c = dark ? kDark : kLight;
            if (y < h / 2 && hashXY(x, y) % 400 == 0) c = kStar;
            img.px[static_cast<size_t>(y) * w + x] = c;
        }
    }
    return img;
}

Image scanlineMask(unsigned rows) {
    Image img;
    if (rows == 0) return img;
    img.w = 1;
    img.h = static_cast<int>(rows * 2);
    img.px.resize(rows * 2);
    for (unsigned i = 0; i < rows * 2; i++) img.px[i] = (i & 1) ? 0x6B000000u : 0x00000000u;
    return img;
}

Image blurCover(const Image& cover, int outW, int outH) {
    Image out;
    if (cover.w <= 0 || cover.h <= 0 || outW <= 0 || outH <= 0) return out;
    if (cover.px.size() < static_cast<size_t>(cover.w) * cover.h) return out;

    // Shrink by block averaging, filling the output and cropping the excess.
    const float scale = std::max(static_cast<float>(outW) / cover.w, static_cast<float>(outH) / cover.h);
    const float cropW = outW / scale, cropH = outH / scale;
    const float x0 = (cover.w - cropW) / 2, y0 = (cover.h - cropH) / 2;

    std::vector<int> a(static_cast<size_t>(outW) * outH * 3);
    for (int y = 0; y < outH; y++) {
        int sy0 = std::clamp(static_cast<int>(std::floor(y0 + y / scale)), 0, cover.h - 1);
        int sy1 = std::clamp(static_cast<int>(std::ceil(y0 + (y + 1) / scale)), sy0 + 1, cover.h);
        for (int x = 0; x < outW; x++) {
            int sx0 = std::clamp(static_cast<int>(std::floor(x0 + x / scale)), 0, cover.w - 1);
            int sx1 = std::clamp(static_cast<int>(std::ceil(x0 + (x + 1) / scale)), sx0 + 1, cover.w);
            int r = 0, g = 0, b = 0;
            for (int sy = sy0; sy < sy1; sy++) {
                for (int sx = sx0; sx < sx1; sx++) {
                    uint32_t p = cover.px[static_cast<size_t>(sy) * cover.w + sx];
                    r += (p >> 16) & 0xFF;
                    g += (p >> 8) & 0xFF;
                    b += p & 0xFF;
                }
            }
            int n = (sy1 - sy0) * (sx1 - sx0);
            int* d = &a[(static_cast<size_t>(y) * outW + x) * 3];
            d[0] = r / n;
            d[1] = g / n;
            d[2] = b / n;
        }
    }

    // Box blur, separable, several passes.
    std::vector<int> b(a.size());
    for (int pass = 0; pass < kBlurPasses; pass++) {
        for (int y = 0; y < outH; y++) {
            boxBlurRow(&a[static_cast<size_t>(y) * outW * 3], &b[static_cast<size_t>(y) * outW * 3], outW, 3);
        }
        for (int x = 0; x < outW; x++) {
            boxBlurRow(&b[static_cast<size_t>(x) * 3], &a[static_cast<size_t>(x) * 3], outH, outW * 3);
        }
    }

    out.w = outW;
    out.h = outH;
    out.px.resize(static_cast<size_t>(outW) * outH);
    for (size_t i = 0; i < out.px.size(); i++) {
        uint32_t r = a[i * 3] * kDarken >> 8;
        uint32_t g = a[i * 3 + 1] * kDarken >> 8;
        uint32_t bl = a[i * 3 + 2] * kDarken >> 8;
        out.px[i] = 0xFF000000u | r << 16 | g << 8 | bl;
    }
    return out;
}
