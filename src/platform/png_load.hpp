#pragma once

#include <string>

#include "video_art.hpp"

// Decodes a PNG to 0xAARRGGBB pixels. A side longer than maxSide, a missing file or
// any decode error gives an empty Image.
Image loadPng(const std::string& path, int maxSide = 4096);
