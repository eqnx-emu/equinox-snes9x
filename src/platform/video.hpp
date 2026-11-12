#pragma once

#include <SDL2/SDL.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "video_art.hpp"
#include "video_layout.hpp"
#include "video_prefs.hpp"

// Streaming RGB565 game texture drawn per the video settings: background, game,
// scanlines. Every texture is rebuilt only when what it depends on changes (source
// size, screen size, settings), never per frame. Optional FPS counter.
class Video {
public:
    // The screen starts at the renderer output size.
    bool init(SDL_Window* window, bool showFps);
    ~Video();

    void setScreen(int w, int h);
    void setSettings(const VideoSettings& settings);
    // Takes an already blurred cover; an empty image removes it.
    void setCover(Image blurred);
    bool hasCover() const { return cover_ != nullptr; }
    int screenW() const { return screenW_; }
    int screenH() const { return screenH_; }

    void upload(const void* px, unsigned w, unsigned h, size_t pitch);
    void present();  // draws the last frame (and the counter) and flips on vsync
    // present() split in two, so the menu can draw over the game frame.
    void drawGame();
    void flip();
    SDL_Renderer* renderer() const { return renderer_; }
    // The last frame the core sent, RGB565 rows packed; empty before the first one.
    const std::vector<uint16_t>& lastFrame() const { return lastFrame_; }
    unsigned frameW() const { return w_; }
    unsigned frameH() const { return h_; }

private:
    void drawBackground(const Rect& game);
    void drawFrame(const Rect& game);
    void drawScanlines(const Rect& game);
    bool tvFrameShown() const;
    void ensureTv(const Rect& game);
    void drawMesh(const std::vector<SDL_Vertex>& v, const std::vector<int>& idx);
    void drawFps();
    SDL_Texture* makeTexture(const Image& img, SDL_ScaleMode scale, SDL_BlendMode blend);
    void applyGameScaleMode();

    SDL_Renderer* renderer_ = nullptr;
    int screenW_ = 1280, screenH_ = 720;
    VideoSettings settings_;

    SDL_Texture* tex_ = nullptr;  // the game frame
    unsigned w_ = 0, h_ = 0;
    std::vector<uint16_t> lastFrame_;  // a copy of the last frame, rows packed, for state pictures
    SDL_Texture* sharp_ = nullptr;  // Sharp: the frame enlarged by whole factors with nearest
    unsigned sharpW_ = 0, sharpH_ = 0;
    SDL_Texture* scan_ = nullptr;
    unsigned scanRows_ = 0;
    SDL_Texture* backdrop_ = nullptr;  // Equinox border, for the current screen
    SDL_Texture* cover_ = nullptr;
    // TV frame: the room behind it, then meshes under and over the game, built for tvRect_.
    SDL_Texture* tvRoom_ = nullptr;
    std::vector<SDL_Vertex> tvBackV_, tvFrontV_;
    std::vector<int> tvBackI_, tvFrontI_;
    Rect tvRect_{0, 0, 0, 0};
    bool tvRoomFailed_ = false;

    bool showFps_ = false;
    bool native565_ = true;
    std::vector<uint32_t> lut_;  // RGB565 to ARGB8888 when the renderer lacks RGB565
    Uint32 lastTick_ = 0;
    int frames_ = 0;
    int fps_ = 0;
};
