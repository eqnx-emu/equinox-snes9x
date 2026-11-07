#include "platform/input.hpp"

#include "libretro.h"

namespace {
// libnx SDL2 joystick button indices.
enum { kA, kB, kX, kY, kLStick, kRStick, kL, kR, kZL, kZR, kPlus, kMinus, kLeft, kUp, kRight, kDown };
constexpr int kDeadzone = 16000;
}  // namespace

void Input::init() {
    SDL_JoystickEventState(SDL_ENABLE);
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        for (auto& slot : all_)
            if (!slot) {
                slot = SDL_JoystickOpen(i);
                break;
            }
    }
    rebuild();
}

void Input::shutdown() {
    for (auto& slot : all_) {
        if (slot) SDL_JoystickClose(slot);
        slot = nullptr;
    }
    pads_[0] = pads_[1] = nullptr;
}

void Input::rebuild() {
    int n = 0;
    pads_[0] = pads_[1] = nullptr;
    for (auto* j : all_)
        if (j && SDL_JoystickGetAttached(j) && n < 2) pads_[n++] = j;
}

void Input::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_JOYDEVICEADDED) {
        SDL_Joystick* j = SDL_JoystickOpen(e.jdevice.which);
        if (!j) return;
        // init() already opened devices present at startup; SDL queues ADDED for them too.
        for (auto* known : all_)
            if (known && SDL_JoystickInstanceID(known) == SDL_JoystickInstanceID(j)) {
                SDL_JoystickClose(j);  // drop the extra reference
                return;
            }
        for (auto& slot : all_)
            if (!slot) {
                slot = j;
                rebuild();
                return;
            }
        SDL_JoystickClose(j);
    } else if (e.type == SDL_JOYDEVICEREMOVED) {
        for (auto& slot : all_)
            if (slot && SDL_JoystickInstanceID(slot) == e.jdevice.which) {
                if (slot == pads_[0]) padLost_ = true;
                SDL_JoystickClose(slot);
                slot = nullptr;
            }
        rebuild();
    }
}

int16_t Input::state(unsigned port, unsigned id) const {
    if (port > 1 || !pads_[port]) return 0;
    SDL_Joystick* j = pads_[port];
    auto btn = [&](int b) { return SDL_JoystickGetButton(j, b) != 0; };
    const int ax = SDL_JoystickGetAxis(j, 0), ay = SDL_JoystickGetAxis(j, 1);
    switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_A: return btn(kA);
        case RETRO_DEVICE_ID_JOYPAD_B: return btn(kB);
        case RETRO_DEVICE_ID_JOYPAD_X: return btn(kX);
        case RETRO_DEVICE_ID_JOYPAD_Y: return btn(kY);
        case RETRO_DEVICE_ID_JOYPAD_L: return btn(kL);
        case RETRO_DEVICE_ID_JOYPAD_R: return btn(kR);
        case RETRO_DEVICE_ID_JOYPAD_START: return btn(kPlus);
        case RETRO_DEVICE_ID_JOYPAD_SELECT: return btn(kMinus);
        case RETRO_DEVICE_ID_JOYPAD_UP: return btn(kUp) || ay < -kDeadzone;
        case RETRO_DEVICE_ID_JOYPAD_DOWN: return btn(kDown) || ay > kDeadzone;
        case RETRO_DEVICE_ID_JOYPAD_LEFT: return btn(kLeft) || ax < -kDeadzone;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT: return btn(kRight) || ax > kDeadzone;
        default: return 0;
    }
}

Input::Edges Input::poll() {
    bool up = false, down = false, a = false, b = false, combo = false;
    for (auto* j : pads_) {
        if (!j) continue;
        const int ay = SDL_JoystickGetAxis(j, 1);
        up |= SDL_JoystickGetButton(j, kUp) || ay < -kDeadzone;
        down |= SDL_JoystickGetButton(j, kDown) || ay > kDeadzone;
        a |= SDL_JoystickGetButton(j, kA) != 0;
        b |= SDL_JoystickGetButton(j, kB) != 0;
        combo |= SDL_JoystickGetButton(j, kZL) && SDL_JoystickGetButton(j, kZR);
    }
    Edges e;
    e.up = up && !prevUp_;
    e.down = down && !prevDown_;
    e.confirm = a && !prevA_;
    e.back = b && !prevB_;
    e.menuCombo = combo && !prevCombo_;
    prevUp_ = up;
    prevDown_ = down;
    prevA_ = a;
    prevB_ = b;
    prevCombo_ = combo;
    return e;
}

bool Input::takePadLost() {
    const bool lost = padLost_;
    padLost_ = false;
    return lost;
}
