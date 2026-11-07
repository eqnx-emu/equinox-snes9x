#pragma once

#include <SDL2/SDL.h>

#include <cstdint>

#include "release_mask.hpp"

// Players 1 and 2 (libretro ports 0 and 1) from the first two joysticks.
// Switch buttons to SNES: A,B,X,Y,L,R as is; Plus=Start, Minus=Select;
// D-pad and left stick are the directions.
class Input {
public:
    void init();
    void shutdown();
    void handleEvent(const SDL_Event& e);                  // hot-plug
    int16_t state(unsigned port, unsigned retroId);  // RETRO_DEVICE_ID_JOYPAD_*, hides suppressed buttons
    // Hides every button held right now from the game until it is released (menu close).
    void suppressHeld();
    // Buttons newly pressed since the previous poll(); call once per frame.
    struct Edges {
        bool up = false, down = false, confirm = false, back = false;  // D-pad/stick, A, B
        bool menuCombo = false;                                         // ZL+ZR together
    };
    Edges poll();
    // True once if player 1's pad was unplugged since the last call.
    bool takePadLost();

private:
    int16_t rawState(unsigned port, unsigned retroId) const;
    void rebuild();
    SDL_Joystick* pads_[2] = {nullptr, nullptr};
    SDL_Joystick* all_[8] = {};
    bool prevUp_ = false, prevDown_ = false, prevA_ = false, prevB_ = false, prevCombo_ = false;
    bool padLost_ = false;
    ReleaseMask release_[2];
};
