#include "video_layout.hpp"

#include "doctest/doctest.h"

TEST_CASE("original is 3x handheld, 4x docked") {
    CHECK(gameRect(Size::Original, 256, 224, 1280, 720) == Rect{256, 24, 768, 672});
    CHECK(gameRect(Size::Original, 256, 224, 1920, 1080) == Rect{448, 92, 1024, 896});
}

TEST_CASE("original does not jump on hi-res or interlace") {
    auto base = gameRect(Size::Original, 256, 224, 1280, 720);
    CHECK(gameRect(Size::Original, 512, 224, 1280, 720) == base);
    CHECK(gameRect(Size::Original, 512, 448, 1280, 720) == base);
}

TEST_CASE("239 lines still fit at 3x handheld") {
    auto r = gameRect(Size::Original, 256, 239, 1280, 720);
    CHECK(r.h == 717);
    CHECK(r.w == 768);
}

TEST_CASE("8:7 fills the height") {
    auto r = gameRect(Size::Aspect87, 256, 224, 1280, 720);
    CHECK(r.h == 720);
    CHECK(r.w == 823);
    CHECK(r.x == (1280 - 823) / 2);
    CHECK(gameRect(Size::Aspect87, 256, 224, 1920, 1080).w == 1234);
}

TEST_CASE("4:3 and full") {
    CHECK(gameRect(Size::Aspect43, 256, 224, 1280, 720) == Rect{160, 0, 960, 720});
    CHECK(gameRect(Size::Aspect43, 602, 224, 1920, 1080) == Rect{240, 0, 1440, 1080});
    CHECK(gameRect(Size::Full, 256, 224, 1920, 1080) == Rect{0, 0, 1920, 1080});
}

TEST_CASE("ntsc width 602 keeps the 8:7 frame") {
    CHECK(gameRect(Size::Aspect87, 602, 224, 1280, 720) == gameRect(Size::Aspect87, 256, 224, 1280, 720));
}

TEST_CASE("logical size halves hi-res and interlace, and maps ntsc 602 to 256") {
    CHECK(logicalW(256) == 256);
    CHECK(logicalW(512) == 256);
    CHECK(logicalW(602) == 256);
    CHECK(logicalH(224) == 224);
    CHECK(logicalH(239) == 239);
    CHECK(logicalH(448) == 224);
    CHECK(logicalH(478) == 239);
}

TEST_CASE("original at ntsc width 602 matches the plain frame") {
    CHECK(gameRect(Size::Original, 602, 224, 1280, 720) == gameRect(Size::Original, 256, 224, 1280, 720));
}

TEST_CASE("zero-sized frame gives an empty rect") {
    CHECK(gameRect(Size::Original, 0, 0, 1280, 720).w == 0);
}

TEST_CASE("sharp prescale is the integer below the final scale, per axis") {
    CHECK(sharpPrescale(256, 224, Rect{160, 0, 960, 720}) == Prescale{3, 3});
    CHECK(sharpPrescale(256, 224, Rect{0, 0, 1920, 1080}) == Prescale{7, 4});
    CHECK(sharpPrescale(512, 224, Rect{256, 24, 768, 672}) == Prescale{1, 3});
    CHECK(sharpPrescale(256, 448, Rect{256, 24, 768, 672}) == Prescale{3, 1});
}
