#include "input/input.hpp"
#include <cstdio>
#include <cstring>

static const char* const BUTTON_NAMES[Input::NUM_BUTTONS] = {
    "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L"
};

static const SDL_Scancode DEFAULT_KEYS[Input::NUM_BUTTONS] = {
    SDL_SCANCODE_X,
    SDL_SCANCODE_Z,
    SDL_SCANCODE_RSHIFT,
    SDL_SCANCODE_RETURN,
    SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_LEFT,
    SDL_SCANCODE_UP,
    SDL_SCANCODE_DOWN,
    SDL_SCANCODE_S,
    SDL_SCANCODE_A,
};

Input::Input() {
    for (int i = 0; i < NUM_BUTTONS; i++)
        key_map_[i] = DEFAULT_KEYS[i];
}

void Input::update(const u8* keyboard_state) {
    u16 keys = 0x03FF;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (keyboard_state[key_map_[i]])
            keys &= ~(1 << i);
    }
    keyinput_ = keys;
}

void Input::set_key(int button, SDL_Scancode sc) {
    if (button >= 0 && button < NUM_BUTTONS)
        key_map_[button] = sc;
}

SDL_Scancode Input::get_key(int button) const {
    if (button >= 0 && button < NUM_BUTTONS)
        return key_map_[button];
    return SDL_SCANCODE_UNKNOWN;
}

const char* Input::button_name(int i) {
    if (i >= 0 && i < NUM_BUTTONS)
        return BUTTON_NAMES[i];
    return "?";
}

bool Input::save_config(const std::string& path) const {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        fprintf(f, "%s=%s\n", BUTTON_NAMES[i], SDL_GetScancodeName(key_map_[i]));
    }
    fclose(f);
    return true;
}

bool Input::load_config(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* btn_name = line;
        const char* key_name = eq + 1;

        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (strcmp(btn_name, BUTTON_NAMES[i]) == 0) {
                SDL_Scancode sc = SDL_GetScancodeFromName(key_name);
                if (sc != SDL_SCANCODE_UNKNOWN)
                    key_map_[i] = sc;
                break;
            }
        }
    }
    fclose(f);
    return true;
}
