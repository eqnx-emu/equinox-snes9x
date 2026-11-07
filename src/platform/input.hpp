#pragma once

#include <SDL2/SDL.h>

#include <cstdint>

// Players 1 and 2 (libretro ports 0 and 1) from the first two joysticks.
// Switch buttons to SNES: A,B,X,Y,L,R as is; Plus=Start, Minus=Select;
// D-pad and left stick are the directions.
class Input {
public:
    void init();
    void shutdown();
    void handleEvent(const SDL_Event& e);                  // hot-plug
    int16_t state(unsigned port, unsigned retroId) const;  // RETRO_DEVICE_ID_JOYPAD_*
    // True once Plus+Minus have been held on any pad for ms.
    bool comboHeld(Uint32 ms);

private:
    void rebuild();
    SDL_Joystick* pads_[2] = {nullptr, nullptr};
    SDL_Joystick* all_[8] = {};
    Uint32 comboSince_ = 0;
};
