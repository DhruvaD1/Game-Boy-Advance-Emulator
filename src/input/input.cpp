// SDL2 keyboard state to GBA KEYINPUT mapping
#include "input/input.hpp"

void Input::update(const u8* keyboard_state) {
    u16 keys = 0x03FF;

    if (keyboard_state[SDL_SCANCODE_X])      keys &= ~BTN_A;
    if (keyboard_state[SDL_SCANCODE_Z])      keys &= ~BTN_B;
    if (keyboard_state[SDL_SCANCODE_RSHIFT] || keyboard_state[SDL_SCANCODE_BACKSPACE])
                                              keys &= ~BTN_SELECT;
    if (keyboard_state[SDL_SCANCODE_RETURN]) keys &= ~BTN_START;
    if (keyboard_state[SDL_SCANCODE_RIGHT])  keys &= ~BTN_RIGHT;
    if (keyboard_state[SDL_SCANCODE_LEFT])   keys &= ~BTN_LEFT;
    if (keyboard_state[SDL_SCANCODE_UP])     keys &= ~BTN_UP;
    if (keyboard_state[SDL_SCANCODE_DOWN])   keys &= ~BTN_DOWN;
    if (keyboard_state[SDL_SCANCODE_S])      keys &= ~BTN_R;
    if (keyboard_state[SDL_SCANCODE_A])      keys &= ~BTN_L;

    keyinput_ = keys;
}
