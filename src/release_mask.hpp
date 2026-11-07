#pragma once

#include <cstdint>

// Buttons that were already held when the menu closed stay reported as released to the
// game until they are physically released, so the closing A/B press never reaches it.
// Bits are libretro joypad ids (0..15).
class ReleaseMask {
public:
    void suppress(uint32_t heldMask) { mask_ |= heldMask; }
    // Returns what the game should see for button id given the physical state.
    bool filter(unsigned id, bool held) {
        const uint32_t bit = 1u << id;
        if (!(mask_ & bit)) return held;
        if (!held) mask_ &= ~bit;
        return false;
    }

private:
    uint32_t mask_ = 0;
};
