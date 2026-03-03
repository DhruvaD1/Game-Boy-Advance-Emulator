// Menu action IDs
#pragma once
#include <SDL.h>
#include <vector>
#include <cstring>

enum MenuAction {
    MENU_NONE       = -1,
    MENU_SAVE_GAME  = 1,
    MENU_QUIT       = 2,
    MENU_SPEED_1X   = 10,
    MENU_SPEED_2X   = 11,
    MENU_SPEED_4X   = 12,
    MENU_SPEED_8X   = 13,
    MENU_SPEED_MAX  = 14,
};

class MenuBar {
public:
    struct DropdownItem {
        const char* label;
        int id;
        bool checked;
        bool is_separator;
    };

    struct Menu {
        const char* label;
        std::vector<DropdownItem> items;
        int x, w;
    };

    void init(SDL_Renderer* renderer);
    void destroy();


    bool handle_event(const SDL_Event& event);


    void render(SDL_Renderer* renderer, int window_w);


    SDL_Rect game_rect(int window_w, int window_h) const;

    int bar_height() const { return BAR_H; }


    int poll_action();


    void set_speed_check(int speed_index);

private:
    static constexpr int BAR_H = 24;
    static constexpr int FONT_SCALE = 1;
    static constexpr int GLYPH_W = 8;
    static constexpr int GLYPH_H = 8;
    static constexpr int CHAR_W = GLYPH_W * FONT_SCALE;
    static constexpr int CHAR_H = GLYPH_H * FONT_SCALE;
    static constexpr int TEXT_PAD_X = 10;
    static constexpr int DROPDOWN_PAD_X = 8;
    static constexpr int DROPDOWN_ITEM_H = 22;
    static constexpr int SEPARATOR_H = 9;

    SDL_Texture* font_texture_ = nullptr;
    std::vector<Menu> menus_;
    int open_menu_ = -1;
    int hover_item_ = -1;
    int pending_action_ = MENU_NONE;

    void draw_text(SDL_Renderer* renderer, const char* text, int x, int y,
                   uint8_t r, uint8_t g, uint8_t b);
    int text_width(const char* text) const;
    int dropdown_width(const Menu& menu) const;
    int dropdown_height(const Menu& menu) const;
};
