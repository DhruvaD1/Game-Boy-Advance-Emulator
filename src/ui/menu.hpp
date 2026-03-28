#pragma once
#include <SDL.h>
#include <vector>
#include <string>
#include <cstring>

class Input;
class CheatEngine;

enum MenuAction {
    MENU_NONE       = -1,
    MENU_SAVE_GAME  = 1,
    MENU_QUIT       = 2,
    MENU_LOAD_ROM   = 3,
    MENU_CHANGE_CONTROLS = 4,
    MENU_SPEED_1X   = 10,
    MENU_SPEED_2X   = 11,
    MENU_SPEED_4X   = 12,
    MENU_SPEED_8X   = 13,
    MENU_SPEED_MAX  = 14,
    MENU_SAVE_STATE_1 = 20,
    MENU_SAVE_STATE_2 = 21,
    MENU_SAVE_STATE_3 = 22,
    MENU_SAVE_STATE_4 = 23,
    MENU_SAVE_STATE_5 = 24,
    MENU_LOAD_STATE_1 = 30,
    MENU_LOAD_STATE_2 = 31,
    MENU_LOAD_STATE_3 = 32,
    MENU_LOAD_STATE_4 = 33,
    MENU_LOAD_STATE_5 = 34,
    MENU_CHEATS = 40,
    MENU_COLOR_CORRECT = 50,
    MENU_BIOS_INTRO = 51,
    MENU_SCALE_1X = 60,
    MENU_SCALE_2X = 61,
    MENU_SCALE_3X = 62,
    MENU_SCALE_4X = 63,
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
    void set_color_correct_check(bool on);
    void set_bios_intro_check(bool on);
    void set_scale_check(int scale);

    void show_notification(const char* text);
    void tick_notification();
    void render_notification(SDL_Renderer* renderer, const SDL_Rect& game_rect);

    void set_input(Input* input) { input_ = input; }
    void open_controls();
    void close_controls();
    bool controls_open() const { return controls_open_; }
    bool handle_controls_event(const SDL_Event& event);
    void render_controls(SDL_Renderer* renderer, int win_w, int win_h);

    void open_rom_browser(const std::string& rom_dir);
    void close_rom_browser();
    bool rom_browser_open() const { return rom_browser_open_; }
    bool handle_rom_browser_event(const SDL_Event& event);
    void render_rom_browser(SDL_Renderer* renderer, int win_w, int win_h);
    std::string selected_rom_path() const;
    void clear_selected_rom();

    void set_cheat_engine(CheatEngine* ce, const std::string& path);
    void open_cheats();
    void close_cheats();
    bool cheats_open() const { return cheats_open_; }
    bool handle_cheats_event(const SDL_Event& event);
    void render_cheats(SDL_Renderer* renderer, int win_w, int win_h);

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
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    int text_width(const char* text) const;
    int dropdown_width(const Menu& menu) const;
    int dropdown_height(const Menu& menu) const;

    static constexpr int NOTIFY_DURATION = 120;
    static constexpr int NOTIFY_FADE_FRAMES = 30;
    char notify_text_[64] = {};
    int notify_timer_ = 0;

    Input* input_ = nullptr;
    bool controls_open_ = false;
    int controls_selected_ = -1;
    bool controls_waiting_ = false;
    int controls_hover_ = -1;

    bool rom_browser_open_ = false;
    int rom_hover_ = -1;
    int rom_scroll_ = 0;
    std::string rom_dir_;
    std::vector<std::string> rom_files_;
    std::string selected_rom_;

    CheatEngine* cheat_engine_ = nullptr;
    std::string cheats_path_;
    bool cheats_open_ = false;
    int cheats_hover_ = -1;
    int cheats_scroll_ = 0;
    bool cheats_adding_ = false;
    int cheats_add_step_ = 0;
    std::string cheats_add_name_;
    std::string cheats_add_codes_;
};
