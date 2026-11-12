#include "thumbnail.hpp"

#include <algorithm>

#include "miniz/miniz.h"

std::vector<uint8_t> thumbnailRgb(const uint16_t* frame, unsigned w, unsigned h, unsigned outW, unsigned outH) {
    if (!frame || w == 0 || h == 0 || outW == 0 || outH == 0) return {};
    std::vector<uint8_t> out(static_cast<size_t>(outW) * outH * 3);
    for (unsigned oy = 0; oy < outH; ++oy) {
        const unsigned y0 = oy * h / outH, y1 = std::max(y0 + 1, (oy + 1) * h / outH);
        for (unsigned ox = 0; ox < outW; ++ox) {
            const unsigned x0 = ox * w / outW, x1 = std::max(x0 + 1, (ox + 1) * w / outW);
            unsigned long r = 0, g = 0, b = 0, n = 0;
            for (unsigned y = y0; y < y1 && y < h; ++y)
                for (unsigned x = x0; x < x1 && x < w; ++x) {
                    const uint16_t p = frame[static_cast<size_t>(y) * w + x];
                    // 5/6-bit channels widened to 8 bits with the top bits repeated.
                    const unsigned r5 = p >> 11, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
                    r += (r5 << 3) | (r5 >> 2);
                    g += (g6 << 2) | (g6 >> 4);
                    b += (b5 << 3) | (b5 >> 2);
                    ++n;
                }
            uint8_t* o = &out[(static_cast<size_t>(oy) * outW + ox) * 3];
            o[0] = static_cast<uint8_t>((r + n / 2) / n);
            o[1] = static_cast<uint8_t>((g + n / 2) / n);
            o[2] = static_cast<uint8_t>((b + n / 2) / n);
        }
    }
    return out;
}

std::vector<uint8_t> encodePng(const std::vector<uint8_t>& rgb, unsigned w, unsigned h) {
    if (w == 0 || h == 0 || rgb.size() < static_cast<size_t>(w) * h * 3) return {};
    size_t len = 0;
    void* png = tdefl_write_image_to_png_file_in_memory_ex(rgb.data(), static_cast<int>(w), static_cast<int>(h), 3,
                                                           &len, MZ_DEFAULT_LEVEL, MZ_FALSE);
    if (!png) return {};
    std::vector<uint8_t> out(static_cast<uint8_t*>(png), static_cast<uint8_t*>(png) + len);
    mz_free(png);
    return out;
}
