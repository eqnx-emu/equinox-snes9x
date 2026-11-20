#pragma once

#include <cstdint>

// Minus and Plus (libretro Select = 2, Start = 3) held together open the pause menu.
inline constexpr uint32_t kMenuComboMask = (1u << 2) | (1u << 3);
inline bool isMenuCombo(uint32_t heldMask) { return (heldMask & kMenuComboMask) == kMenuComboMask; }

// Buttons that were already held when the menu closed stay reported as released to the
// game until they are physically released, so the closing A/B press never reaches it.
// Bits are libretro joypad ids (0..15).
class ReleaseMask {
public:
    void suppress(uint32_t heldMask) { mask_ |= heldMask; }
    // With the menu combo held, the game sees neither Select nor Start until each is
    // released, so it never gets the two together. Either one alone passes.
    void hideMenuCombo(uint32_t heldMask) {
        if (isMenuCombo(heldMask)) suppress(kMenuComboMask);
    }
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
