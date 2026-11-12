#pragma once

#include <cstdint>
#include <vector>

// The picture kept beside a savestate, shown on the pause menu's state card.

// Shrinks a packed RGB565 frame (w * h pixels) to outW x outH RGB24 by averaging each
// output pixel's block. Empty for an empty frame.
std::vector<uint8_t> thumbnailRgb(const uint16_t* frame, unsigned w, unsigned h, unsigned outW, unsigned outH);

// RGB24 pixels as a PNG file. Empty on an empty image or an encoder failure.
std::vector<uint8_t> encodePng(const std::vector<uint8_t>& rgb, unsigned w, unsigned h);
