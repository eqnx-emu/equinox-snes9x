#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "filters.hpp"
#include "platform/gl.hpp"
#include "shader_layout.hpp"
#include "shader_preset.hpp"

// A RetroArch .glslp preset turned into GL programs and render targets: the game
// frame goes in, each pass draws into the next one's input, the last draws on screen.
// Every GL call expects the caller to hold a gl::StateGuard.
class ShaderChain {
public:
    ShaderChain() = default;
    ~ShaderChain();
    ShaderChain(const ShaderChain&) = delete;
    ShaderChain& operator=(const ShaderChain&) = delete;

    // Reads and compiles the preset. False (logged, nothing kept) when a file is
    // missing, the preset is not supported, or a stage does not compile or link.
    bool load(const std::string& presetFile, std::span<const ParamOverride> overrides, const gl::Info& info);
    void unload();
    bool loaded() const { return !passes_.empty(); }

    // The game frame, RGB565 rows packed.
    void upload(const uint16_t* px565, unsigned w, unsigned h);
    // Draws into the framebuffer bound when called, at this viewport (GL coordinates,
    // origin at the bottom left). False when nothing was drawn (no frame yet, or a
    // render target the driver refuses).
    bool draw(int vx, int vy, int vw, int vh, unsigned frameCount);

private:
    struct Uniform {
        GLint location;
        GLenum type;
        int kind;     // what it is fed with, see shader_chain.cpp
        float value;  // parameters only
    };
    struct Pass {
        GLuint program = 0;
        std::vector<Uniform> uniforms;  // only the ones the program uses
        GLuint fbo = 0, target = 0;  // this pass's output; none for the one drawing on screen
    };

    bool ensureTargets(Size2 viewport);
    void freeTargets();
    void drawPass(const Pass& pass, GLuint input, Size2 inSize, Size2 outSize, bool fromGameFrame,
                  std::optional<bool> filterLinear, Wrap wrap, unsigned frameCount);

    Preset preset_;
    std::vector<Pass> passes_;
    Pass blit_;  // the final copy when the last pass does not draw on screen
    bool needsBlit_ = false;
    gl::Info info_;

    GLuint frame_ = 0;  // the game frame texture
    Size2 frameSize_{0, 0};
    std::vector<Size2> sizes_;
    Size2 targetsFor_{0, 0}, targetsView_{0, 0};
    bool floatWarned_ = false;
};
