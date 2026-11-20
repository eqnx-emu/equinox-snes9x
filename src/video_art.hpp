#pragma once

#include <cstdint>
#include <vector>

// Pixels are 0xAARRGGBB, row by row.
struct Image {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
};

// Dithered purple gradient with a few loose stars in the top half. Generated at low
// resolution; the caller enlarges it with nearest filtering.
Image equinoxBackdrop(int w, int h);

// Cover scaled to fill outW x outH (excess cropped), blurred and darkened.
// An empty cover or output gives an empty image.
Image blurCover(const Image& cover, int outW, int outH);
