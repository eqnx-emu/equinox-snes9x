#pragma once

#include <string>

#include "video_art.hpp"

// Decodes a PNG to 0xAARRGGBB pixels. A side longer than maxSide, more than
// maxPixels in all, a missing file or any decode error gives an empty Image.
// Can throw std::bad_alloc.
Image loadPng(const std::string& path, int maxSide = 4096, long maxPixels = 2048L * 2048L);
