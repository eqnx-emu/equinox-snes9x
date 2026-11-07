#include "video_layout.hpp"

#include <algorithm>

unsigned logicalW(unsigned srcW) {
    if (srcW >= 600) return 256;
    if (srcW >= 512) return srcW / 2;
    return srcW;
}

unsigned logicalH(unsigned srcH) {
    return srcH >= 448 ? srcH / 2 : srcH;
}

Rect gameRect(Size size, unsigned srcW, unsigned srcH, int screenW, int screenH) {
    if (srcW == 0 || srcH == 0 || screenW <= 0 || screenH <= 0) return Rect{0, 0, 0, 0};

    int w = screenW;
    int h = screenH;
    switch (size) {
    case Size::Original: {
        int lw = static_cast<int>(logicalW(srcW));
        int lh = static_cast<int>(logicalH(srcH));
        int k = std::max(1, screenH / lh);
        w = k * lw;
        h = k * lh;
        break;
    }
    case Size::Aspect87:
        w = (screenH * 8 + 3) / 7;
        break;
    case Size::Aspect43:
        w = screenH * 4 / 3;
        break;
    case Size::Full:
        break;
    }
    return Rect{(screenW - w) / 2, (screenH - h) / 2, w, h};
}

Prescale sharpPrescale(unsigned srcW, unsigned srcH, const Rect& dst) {
    if (srcW == 0 || srcH == 0) return Prescale{1, 1};
    return Prescale{std::max(1, dst.w / static_cast<int>(srcW)), std::max(1, dst.h / static_cast<int>(srcH))};
}
