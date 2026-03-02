// joypad input — KEYINPUT register
#pragma once
#include "types.hpp"
#include <SDL.h>

class Input {
public:
    void update(const u8* keyboard_state);
    u16 read_keyinput() const { return keyinput_; }

    enum Button : u16 {
        BTN_A      = 1 << 0,
        BTN_B      = 1 << 1,
        BTN_SELECT = 1 << 2,
        BTN_START  = 1 << 3,
        BTN_RIGHT  = 1 << 4,
        BTN_LEFT   = 1 << 5,
        BTN_UP     = 1 << 6,
        BTN_DOWN   = 1 << 7,
        BTN_R      = 1 << 8,
        BTN_L      = 1 << 9,
    };

private:
    u16 keyinput_ = 0x03FF;
};
