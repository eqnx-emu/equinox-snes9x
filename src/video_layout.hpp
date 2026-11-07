#pragma once

#include "video_prefs.hpp"

struct Rect {
    int x, y, w, h;
    bool operator==(const Rect&) const = default;
};

// Logical columns and lines: 512 (and the 602-wide NTSC output) count as 256,
// 448/478 count as 224/239.
unsigned logicalW(unsigned srcW);
unsigned logicalH(unsigned srcH);

// Where the game sits on the screen, centered. An empty frame gives w == 0.
Rect gameRect(Size size, unsigned srcW, unsigned srcH, int screenW, int screenH);

// Integer upscale applied before the bilinear pass for Sharp; always >= 1.
int sharpPrescale(unsigned srcW, unsigned srcH, const Rect& dst);
