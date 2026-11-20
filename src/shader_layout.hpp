#pragma once

#include <array>
#include <vector>

#include "shader_preset.hpp"

struct Size2 {
    unsigned w, h;
};

// The output size of every pass, for this game frame size and on-screen viewport.
// A pass with no scale type is Source x1, except the last one, which fills the viewport.
std::vector<Size2> passSizes(const Preset& p, Size2 input, Size2 viewport);

// True when the last pass does not draw straight into the viewport, so its output
// still has to be stretched there.
bool needsFinalBlit(const Preset& p);

// The quad's texture coordinates, vertices (0,0) (1,0) (0,1) (1,1). The game frame is
// uploaded with row 0 at the top, so the pass that reads it flips t; FBOs do not.
std::array<float, 8> passTexCoords(bool fromGameFrame);
