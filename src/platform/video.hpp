#pragma once

#include <SDL2/SDL.h>

#include <cstddef>

// Streaming RGB565 texture, drawn centred in a 1280x720 window at 4:3 (960x720)
// with linear filtering. Optional FPS counter.
class Video {
public:
    bool init(SDL_Window* window, bool showFps);
    ~Video();

    void upload(const void* px, unsigned w, unsigned h, size_t pitch);
    void present();  // draws the last frame (and the counter) and flips on vsync

private:
    void drawFps();

    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* tex_ = nullptr;
    unsigned w_ = 0, h_ = 0;
    bool showFps_ = false;
    Uint32 lastTick_ = 0;
    int frames_ = 0;
    int fps_ = 0;
};
