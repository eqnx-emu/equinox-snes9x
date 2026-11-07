#include "platform/png_load.hpp"

#include <png.h>

#include <cstdio>
#include <cstring>

Image loadPng(const std::string& path, int maxSide, long maxPixels) {
    png_image png;
    std::memset(&png, 0, sizeof png);
    png.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&png, path.c_str())) {
        std::fprintf(stderr, "png %s: %s\n", path.c_str(), png.message);
        return {};
    }
    if (png.width == 0 || png.height == 0 || png.width > static_cast<png_uint_32>(maxSide) ||
        png.height > static_cast<png_uint_32>(maxSide) ||
        static_cast<long long>(png.width) * png.height > maxPixels) {
        std::fprintf(stderr, "png %s: %ux%u is out of bounds\n", path.c_str(), png.width, png.height);
        png_image_free(&png);
        return {};
    }
    // BGRA bytes are 0xAARRGGBB words on a little-endian CPU.
    png.format = PNG_FORMAT_BGRA;
    Image img;
    img.w = static_cast<int>(png.width);
    img.h = static_cast<int>(png.height);
    try {
        img.px.resize(static_cast<size_t>(img.w) * img.h);
    } catch (...) {
        png_image_free(&png);  // closes the file; the caller handles the bad_alloc
        throw;
    }
    if (!png_image_finish_read(&png, nullptr, img.px.data(), 0, nullptr)) {
        std::fprintf(stderr, "png %s: %s\n", path.c_str(), png.message);
        png_image_free(&png);
        return {};
    }
    return img;
}
