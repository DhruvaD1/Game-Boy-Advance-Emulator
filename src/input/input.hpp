#pragma once
#include "types.hpp"
#include <SDL.h>
#include <string>

class Input {
public:
    static constexpr int NUM_BUTTONS = 10;

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

    Input();

    void update(const u8* keyboard_state);
    u16 read_keyinput() const { return keyinput_; }

    void set_key(int button, SDL_Scancode sc);
    SDL_Scancode get_key(int button) const;
    static const char* button_name(int i);

    bool save_config(const std::string& path) const;
    bool load_config(const std::string& path);

private:
    u16 keyinput_ = 0x03FF;
    SDL_Scancode key_map_[NUM_BUTTONS];
};
