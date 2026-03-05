#include "menu.hpp"
#include "input/input.hpp"
#include "cheat/cheat.hpp"
#include "font.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <dirent.h>

void MenuBar::init(SDL_Renderer* renderer) {
    font_texture_ = create_font_texture(renderer);

    menus_.clear();


    Menu file_menu;
    file_menu.label = "File";
    file_menu.items.push_back({"Load ROM", MENU_LOAD_ROM, false, false});
    file_menu.items.push_back({"Save Game   F6", MENU_SAVE_GAME, false, false});
    file_menu.items.push_back({"", 0, false, true});
    file_menu.items.push_back({"Change Controls", MENU_CHANGE_CONTROLS, false, false});
    file_menu.items.push_back({"Cheats", MENU_CHEATS, false, false});
    file_menu.items.push_back({"", 0, false, true});
    file_menu.items.push_back({"Quit", MENU_QUIT, false, false});
    menus_.push_back(file_menu);

    Menu state_menu;
    state_menu.label = "Save State";
    state_menu.items.push_back({"Save Slot 1  Shift+F1", MENU_SAVE_STATE_1, false, false});
    state_menu.items.push_back({"Save Slot 2  Shift+F2", MENU_SAVE_STATE_2, false, false});
    state_menu.items.push_back({"Save Slot 3  Shift+F3", MENU_SAVE_STATE_3, false, false});
    state_menu.items.push_back({"Save Slot 4  Shift+F4", MENU_SAVE_STATE_4, false, false});
    state_menu.items.push_back({"Save Slot 5  Shift+F5", MENU_SAVE_STATE_5, false, false});
    state_menu.items.push_back({"", 0, false, true});
    state_menu.items.push_back({"Load Slot 1       F1", MENU_LOAD_STATE_1, false, false});
    state_menu.items.push_back({"Load Slot 2       F2", MENU_LOAD_STATE_2, false, false});
    state_menu.items.push_back({"Load Slot 3       F3", MENU_LOAD_STATE_3, false, false});
    state_menu.items.push_back({"Load Slot 4       F4", MENU_LOAD_STATE_4, false, false});
    state_menu.items.push_back({"Load Slot 5       F5", MENU_LOAD_STATE_5, false, false});
    menus_.push_back(state_menu);


    Menu speed_menu;
    speed_menu.label = "Speed";
    speed_menu.items.push_back({"1x", MENU_SPEED_1X, true, false});
    speed_menu.items.push_back({"2x", MENU_SPEED_2X, false, false});
    speed_menu.items.push_back({"4x", MENU_SPEED_4X, false, false});
    speed_menu.items.push_back({"8x", MENU_SPEED_8X, false, false});
    speed_menu.items.push_back({"Unlimited", MENU_SPEED_MAX, false, false});
    menus_.push_back(speed_menu);

    Menu display_menu;
    display_menu.label = "Display";
    display_menu.items.push_back({"Color Correction", MENU_COLOR_CORRECT, false, false});
    menus_.push_back(display_menu);


    int x = 0;
    for (auto& m : menus_) {
        m.x = x;
        m.w = text_width(m.label) + TEXT_PAD_X * 2;
        x += m.w;
    }
}

void MenuBar::destroy() {
    if (font_texture_) {
        SDL_DestroyTexture(font_texture_);
        font_texture_ = nullptr;
    }
    menus_.clear();
}

void MenuBar::set_speed_check(int speed_index) {
    if (menus_.size() < 3) return;
    auto& items = menus_[2].items;
    for (size_t i = 0; i < items.size(); i++) {
        if (!items[i].is_separator) {
            items[i].checked = (static_cast<int>(i) == speed_index);
        }
    }
}

void MenuBar::set_color_correct_check(bool on) {
    if (menus_.size() < 4) return;
    auto& items = menus_[3].items;
    for (auto& item : items) {
        if (item.id == MENU_COLOR_CORRECT) {
            item.checked = on;
        }
    }
}

int MenuBar::poll_action() {
    int a = pending_action_;
    pending_action_ = MENU_NONE;
    return a;
}

bool MenuBar::handle_event(const SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x;
        int my = event.button.y;


        if (my < BAR_H) {
            for (size_t i = 0; i < menus_.size(); i++) {
                if (mx >= menus_[i].x && mx < menus_[i].x + menus_[i].w) {
                    if (open_menu_ == static_cast<int>(i)) {
                        open_menu_ = -1;
                    } else {
                        open_menu_ = static_cast<int>(i);
                        hover_item_ = -1;
                    }
                    return true;
                }
            }

            open_menu_ = -1;
            return true;
        }


        if (open_menu_ >= 0) {
            const auto& menu = menus_[open_menu_];
            int dx = menu.x;
            int dy = BAR_H;
            int dw = dropdown_width(menu);
            int dh = dropdown_height(menu);

            if (mx >= dx && mx < dx + dw && my >= dy && my < dy + dh) {

                int iy = BAR_H;
                for (size_t i = 0; i < menu.items.size(); i++) {
                    int item_h = menu.items[i].is_separator ? SEPARATOR_H : DROPDOWN_ITEM_H;
                    if (my >= iy && my < iy + item_h && !menu.items[i].is_separator) {
                        pending_action_ = menu.items[i].id;
                        open_menu_ = -1;
                        hover_item_ = -1;
                        return true;
                    }
                    iy += item_h;
                }
                return true;
            }


            open_menu_ = -1;
            hover_item_ = -1;

            return false;
        }

        return false;
    }

    if (event.type == SDL_MOUSEMOTION) {
        int mx = event.motion.x;
        int my = event.motion.y;


        if (open_menu_ >= 0 && my < BAR_H) {
            for (size_t i = 0; i < menus_.size(); i++) {
                if (mx >= menus_[i].x && mx < menus_[i].x + menus_[i].w) {
                    if (open_menu_ != static_cast<int>(i)) {
                        open_menu_ = static_cast<int>(i);
                        hover_item_ = -1;
                    }
                    return true;
                }
            }
        }


        if (open_menu_ >= 0) {
            const auto& menu = menus_[open_menu_];
            int dx = menu.x;
            int dw = dropdown_width(menu);
            hover_item_ = -1;

            if (mx >= dx && mx < dx + dw && my >= BAR_H) {
                int iy = BAR_H;
                for (size_t i = 0; i < menu.items.size(); i++) {
                    int item_h = menu.items[i].is_separator ? SEPARATOR_H : DROPDOWN_ITEM_H;
                    if (my >= iy && my < iy + item_h && !menu.items[i].is_separator) {
                        hover_item_ = static_cast<int>(i);
                    }
                    iy += item_h;
                }
            }
            return true;
        }

        return false;
    }

    return false;
}

void MenuBar::render(SDL_Renderer* renderer, int window_w) {

    SDL_SetRenderDrawColor(renderer, 0xE0, 0xE0, 0xE0, 0xFF);
    SDL_Rect bar_rect = {0, 0, window_w, BAR_H};
    SDL_RenderFillRect(renderer, &bar_rect);


    SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
    SDL_RenderDrawLine(renderer, 0, BAR_H - 1, window_w, BAR_H - 1);


    for (size_t i = 0; i < menus_.size(); i++) {
        const auto& m = menus_[i];


        if (open_menu_ == static_cast<int>(i)) {
            SDL_SetRenderDrawColor(renderer, 0x33, 0x99, 0xFF, 0xFF);
            SDL_Rect hl = {m.x, 0, m.w, BAR_H - 1};
            SDL_RenderFillRect(renderer, &hl);
            draw_text(renderer, m.label, m.x + TEXT_PAD_X, (BAR_H - CHAR_H) / 2, 0xFF, 0xFF, 0xFF);
        } else {
            draw_text(renderer, m.label, m.x + TEXT_PAD_X, (BAR_H - CHAR_H) / 2, 0x00, 0x00, 0x00);
        }
    }


    if (open_menu_ >= 0 && open_menu_ < static_cast<int>(menus_.size())) {
        const auto& menu = menus_[open_menu_];
        int dx = menu.x;
        int dy = BAR_H;
        int dw = dropdown_width(menu);
        int dh = dropdown_height(menu);


        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0x80);
        SDL_Rect shadow = {dx + 2, dy + 2, dw, dh};
        SDL_RenderFillRect(renderer, &shadow);


        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_Rect bg = {dx, dy, dw, dh};
        SDL_RenderFillRect(renderer, &bg);


        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderDrawRect(renderer, &bg);


        int iy = dy;
        for (size_t i = 0; i < menu.items.size(); i++) {
            const auto& item = menu.items[i];

            if (item.is_separator) {
                int sep_y = iy + SEPARATOR_H / 2;
                SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
                SDL_RenderDrawLine(renderer, dx + 4, sep_y, dx + dw - 4, sep_y);
                iy += SEPARATOR_H;
                continue;
            }


            if (hover_item_ == static_cast<int>(i)) {
                SDL_SetRenderDrawColor(renderer, 0x33, 0x99, 0xFF, 0xFF);
                SDL_Rect hl = {dx + 1, iy, dw - 2, DROPDOWN_ITEM_H};
                SDL_RenderFillRect(renderer, &hl);

                if (item.checked) {
                    draw_text(renderer, "*", dx + DROPDOWN_PAD_X, iy + (DROPDOWN_ITEM_H - CHAR_H) / 2, 0xFF, 0xFF, 0xFF);
                }
                draw_text(renderer, item.label, dx + DROPDOWN_PAD_X + CHAR_W * 2,
                          iy + (DROPDOWN_ITEM_H - CHAR_H) / 2, 0xFF, 0xFF, 0xFF);
            } else {
                if (item.checked) {
                    draw_text(renderer, "*", dx + DROPDOWN_PAD_X, iy + (DROPDOWN_ITEM_H - CHAR_H) / 2, 0x00, 0x00, 0x00);
                }
                draw_text(renderer, item.label, dx + DROPDOWN_PAD_X + CHAR_W * 2,
                          iy + (DROPDOWN_ITEM_H - CHAR_H) / 2, 0x00, 0x00, 0x00);
            }

            iy += DROPDOWN_ITEM_H;
        }
    }
}

SDL_Rect MenuBar::game_rect(int window_w, int window_h) const {
    int available_h = window_h - BAR_H;
    if (available_h < 1) available_h = 1;

    double scale_x = static_cast<double>(window_w) / 240.0;
    double scale_y = static_cast<double>(available_h) / 160.0;
    double scale = std::min(scale_x, scale_y);

    int game_w = static_cast<int>(240.0 * scale);
    int game_h = static_cast<int>(160.0 * scale);
    int x = (window_w - game_w) / 2;
    int y = BAR_H + (available_h - game_h) / 2;

    return {x, y, game_w, game_h};
}

void MenuBar::show_notification(const char* text) {
    strncpy(notify_text_, text, sizeof(notify_text_) - 1);
    notify_text_[sizeof(notify_text_) - 1] = '\0';
    notify_timer_ = NOTIFY_DURATION;
}

void MenuBar::tick_notification() {
    if (notify_timer_ > 0) notify_timer_--;
}

void MenuBar::render_notification(SDL_Renderer* renderer, const SDL_Rect& game_rect) {
    if (notify_timer_ <= 0) return;

    uint8_t alpha = 255;
    if (notify_timer_ <= NOTIFY_FADE_FRAMES) {
        alpha = (uint8_t)(255 * notify_timer_ / NOTIFY_FADE_FRAMES);
    }

    int tw = text_width(notify_text_);
    int pad_x = 12;
    int pad_y = 6;
    int box_w = tw + pad_x * 2;
    int box_h = CHAR_H + pad_y * 2;
    int box_x = game_rect.x + (game_rect.w - box_w) / 2;
    int box_y = game_rect.y + game_rect.h - box_h - 10;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, (uint8_t)(180 * alpha / 255));
    SDL_Rect bg = {box_x, box_y, box_w, box_h};
    SDL_RenderFillRect(renderer, &bg);

    draw_text(renderer, notify_text_, box_x + pad_x, box_y + pad_y, 0xFF, 0xFF, 0xFF, alpha);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void MenuBar::open_controls() {
    controls_open_ = true;
    controls_selected_ = -1;
    controls_waiting_ = false;
    controls_hover_ = -1;
}

void MenuBar::close_controls() {
    controls_open_ = false;
    controls_selected_ = -1;
    controls_waiting_ = false;
    controls_hover_ = -1;
}

bool MenuBar::handle_controls_event(const SDL_Event& event) {
    if (!controls_open_ || !input_) return false;

    if (controls_waiting_) {
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                controls_waiting_ = false;
                controls_selected_ = -1;
            } else {
                input_->set_key(controls_selected_, event.key.keysym.scancode);
                controls_waiting_ = false;
                controls_selected_ = -1;
            }
            return true;
        }
        return true;
    }

    if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        close_controls();
        return true;
    }

    if (event.type == SDL_MOUSEMOTION) {
        controls_hover_ = -1;
        int mx = event.motion.x;
        int my = event.motion.y;

        int row_h = CHAR_H + 12;
        int panel_w = 280;
        int title_h = CHAR_H + 16;

        SDL_Window* win = SDL_GetWindowFromID(event.motion.windowID);
        if (!win) return true;
        int win_w, win_h;
        SDL_GetWindowSize(win, &win_w, &win_h);

        int panel_h = title_h + Input::NUM_BUTTONS * row_h + 12;
        int px = (win_w - panel_w) / 2;
        int py = (win_h - panel_h) / 2;

        int row_y_start = py + title_h;
        if (mx >= px && mx < px + panel_w && my >= row_y_start && my < row_y_start + Input::NUM_BUTTONS * row_h) {
            controls_hover_ = (my - row_y_start) / row_h;
        }
        return true;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x;
        int my = event.button.y;

        int row_h = CHAR_H + 12;
        int panel_w = 280;
        int title_h = CHAR_H + 16;

        SDL_Window* win = SDL_GetWindowFromID(event.button.windowID);
        if (!win) return true;
        int win_w, win_h;
        SDL_GetWindowSize(win, &win_w, &win_h);

        int panel_h = title_h + Input::NUM_BUTTONS * row_h + 12;
        int px = (win_w - panel_w) / 2;
        int py = (win_h - panel_h) / 2;

        int row_y_start = py + title_h;
        if (mx >= px && mx < px + panel_w && my >= row_y_start && my < row_y_start + Input::NUM_BUTTONS * row_h) {
            int row = (my - row_y_start) / row_h;
            if (row >= 0 && row < Input::NUM_BUTTONS) {
                controls_selected_ = row;
                controls_waiting_ = true;
            }
        }
        return true;
    }

    return true;
}

void MenuBar::render_controls(SDL_Renderer* renderer, int win_w, int win_h) {
    if (!controls_open_ || !input_) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xC0);
    SDL_Rect full = {0, 0, win_w, win_h};
    SDL_RenderFillRect(renderer, &full);

    int row_h = CHAR_H + 12;
    int panel_w = 280;
    int title_h = CHAR_H + 16;
    int panel_h = title_h + Input::NUM_BUTTONS * row_h + 12;
    int px = (win_w - panel_w) / 2;
    int py = (win_h - panel_h) / 2;

    SDL_SetRenderDrawColor(renderer, 0x2A, 0x2A, 0x2A, 0xF0);
    SDL_Rect panel = {px, py, panel_w, panel_h};
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xFF);
    SDL_RenderDrawRect(renderer, &panel);

    const char* title = "Controls (ESC to close)";
    int tw = text_width(title);
    draw_text(renderer, title, px + (panel_w - tw) / 2, py + 8, 0xFF, 0xFF, 0xFF);

    int ry = py + title_h;
    for (int i = 0; i < Input::NUM_BUTTONS; i++) {
        if (i == controls_hover_ && !controls_waiting_) {
            SDL_SetRenderDrawColor(renderer, 0x44, 0x44, 0x44, 0xFF);
            SDL_Rect row_bg = {px + 2, ry, panel_w - 4, row_h};
            SDL_RenderFillRect(renderer, &row_bg);
        }

        if (i == controls_selected_ && controls_waiting_) {
            SDL_SetRenderDrawColor(renderer, 0x33, 0x99, 0xFF, 0x80);
            SDL_Rect row_bg = {px + 2, ry, panel_w - 4, row_h};
            SDL_RenderFillRect(renderer, &row_bg);
        }

        const char* btn = Input::button_name(i);
        draw_text(renderer, btn, px + 16, ry + (row_h - CHAR_H) / 2, 0xCC, 0xCC, 0xCC);

        if (i == controls_selected_ && controls_waiting_) {
            draw_text(renderer, "Press a key...", px + panel_w - 16 - text_width("Press a key..."),
                      ry + (row_h - CHAR_H) / 2, 0xFF, 0xCC, 0x00);
        } else {
            const char* key_name = SDL_GetScancodeName(input_->get_key(i));
            int kw = text_width(key_name);
            draw_text(renderer, key_name, px + panel_w - 16 - kw,
                      ry + (row_h - CHAR_H) / 2, 0xFF, 0xFF, 0xFF);
        }

        ry += row_h;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static bool ends_with_gba(const std::string& s) {
    if (s.size() < 4) return false;
    std::string ext = s.substr(s.size() - 4);
    return ext == ".gba" || ext == ".GBA";
}

void MenuBar::open_rom_browser(const std::string& rom_dir) {
    rom_dir_ = rom_dir;
    rom_files_.clear();
    rom_hover_ = -1;
    rom_scroll_ = 0;
    selected_rom_.clear();

    DIR* dir = opendir(rom_dir.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (ends_with_gba(name)) {
            rom_files_.push_back(name);
        }
    }
    closedir(dir);

    std::sort(rom_files_.begin(), rom_files_.end());
    rom_browser_open_ = true;
}

void MenuBar::close_rom_browser() {
    rom_browser_open_ = false;
    rom_hover_ = -1;
    rom_scroll_ = 0;
}

std::string MenuBar::selected_rom_path() const {
    return selected_rom_;
}

void MenuBar::clear_selected_rom() {
    selected_rom_.clear();
}

bool MenuBar::handle_rom_browser_event(const SDL_Event& event) {
    if (!rom_browser_open_) return false;

    if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        close_rom_browser();
        return true;
    }

    if (event.type == SDL_MOUSEWHEEL) {
        rom_scroll_ -= event.wheel.y * 2;
        if (rom_scroll_ < 0) rom_scroll_ = 0;
        int max_scroll = std::max(0, (int)rom_files_.size() - 8);
        if (rom_scroll_ > max_scroll) rom_scroll_ = max_scroll;
        return true;
    }

    if (event.type == SDL_MOUSEMOTION) {
        rom_hover_ = -1;
        int mx = event.motion.x;
        int my = event.motion.y;

        int row_h = CHAR_H + 12;
        int panel_w = 400;
        int title_h = CHAR_H + 16;
        int max_visible = 8;
        int visible = std::min((int)rom_files_.size(), max_visible);
        int panel_h = title_h + visible * row_h + 12;

        SDL_Window* win = SDL_GetWindowFromID(event.motion.windowID);
        if (!win) return true;
        int win_w, win_h;
        SDL_GetWindowSize(win, &win_w, &win_h);

        int px = (win_w - panel_w) / 2;
        int py = (win_h - panel_h) / 2;
        int row_y_start = py + title_h;

        if (mx >= px && mx < px + panel_w && my >= row_y_start && my < row_y_start + visible * row_h) {
            int row = (my - row_y_start) / row_h;
            int idx = row + rom_scroll_;
            if (idx >= 0 && idx < (int)rom_files_.size()) {
                rom_hover_ = idx;
            }
        }
        return true;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x;
        int my = event.button.y;

        int row_h = CHAR_H + 12;
        int panel_w = 400;
        int title_h = CHAR_H + 16;
        int max_visible = 8;
        int visible = std::min((int)rom_files_.size(), max_visible);
        int panel_h = title_h + visible * row_h + 12;

        SDL_Window* win = SDL_GetWindowFromID(event.button.windowID);
        if (!win) return true;
        int win_w, win_h;
        SDL_GetWindowSize(win, &win_w, &win_h);

        int px = (win_w - panel_w) / 2;
        int py = (win_h - panel_h) / 2;
        int row_y_start = py + title_h;

        if (mx >= px && mx < px + panel_w && my >= row_y_start && my < row_y_start + visible * row_h) {
            int row = (my - row_y_start) / row_h;
            int idx = row + rom_scroll_;
            if (idx >= 0 && idx < (int)rom_files_.size()) {
                selected_rom_ = rom_dir_ + "/" + rom_files_[idx];
                close_rom_browser();
            }
        }
        return true;
    }

    return true;
}

void MenuBar::render_rom_browser(SDL_Renderer* renderer, int win_w, int win_h) {
    if (!rom_browser_open_) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xC0);
    SDL_Rect full = {0, 0, win_w, win_h};
    SDL_RenderFillRect(renderer, &full);

    int row_h = CHAR_H + 12;
    int panel_w = 400;
    int title_h = CHAR_H + 16;
    int max_visible = 8;
    int visible = std::min((int)rom_files_.size(), max_visible);
    if (visible == 0) visible = 1;
    int panel_h = title_h + visible * row_h + 12;
    int px = (win_w - panel_w) / 2;
    int py = (win_h - panel_h) / 2;

    SDL_SetRenderDrawColor(renderer, 0x2A, 0x2A, 0x2A, 0xF0);
    SDL_Rect panel = {px, py, panel_w, panel_h};
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xFF);
    SDL_RenderDrawRect(renderer, &panel);

    const char* title = rom_files_.empty() ? "No ROMs found (ESC to close)" : "Select ROM (ESC to close)";
    int tw = text_width(title);
    draw_text(renderer, title, px + (panel_w - tw) / 2, py + 8, 0xFF, 0xFF, 0xFF);

    int ry = py + title_h;
    for (int i = 0; i < visible && (i + rom_scroll_) < (int)rom_files_.size(); i++) {
        int idx = i + rom_scroll_;

        if (idx == rom_hover_) {
            SDL_SetRenderDrawColor(renderer, 0x44, 0x44, 0x44, 0xFF);
            SDL_Rect row_bg = {px + 2, ry, panel_w - 4, row_h};
            SDL_RenderFillRect(renderer, &row_bg);
        }

        const std::string& name = rom_files_[idx];
        std::string display = name;
        int max_chars = (panel_w - 32) / CHAR_W;
        if ((int)display.size() > max_chars) {
            display = display.substr(0, max_chars - 3) + "...";
        }
        draw_text(renderer, display.c_str(), px + 16, ry + (row_h - CHAR_H) / 2, 0xFF, 0xFF, 0xFF);

        ry += row_h;
    }

    if ((int)rom_files_.size() > max_visible) {
        int sb_x = px + panel_w - 6;
        int sb_y = py + title_h;
        int sb_h = visible * row_h;

        SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 0xFF);
        SDL_Rect sb_bg = {sb_x, sb_y, 4, sb_h};
        SDL_RenderFillRect(renderer, &sb_bg);

        int total = (int)rom_files_.size();
        int thumb_h = std::max(10, sb_h * max_visible / total);
        int thumb_y = sb_y + (sb_h - thumb_h) * rom_scroll_ / std::max(1, total - max_visible);

        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xFF);
        SDL_Rect thumb = {sb_x, thumb_y, 4, thumb_h};
        SDL_RenderFillRect(renderer, &thumb);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void MenuBar::set_cheat_engine(CheatEngine* ce, const std::string& path) {
    cheat_engine_ = ce;
    cheats_path_ = path;
}

void MenuBar::open_cheats() {
    cheats_open_ = true;
    cheats_hover_ = -1;
    cheats_scroll_ = 0;
    cheats_adding_ = false;
    cheats_add_step_ = 0;
    cheats_add_name_.clear();
    cheats_add_codes_.clear();
}

void MenuBar::close_cheats() {
    if (cheats_adding_) {
        SDL_StopTextInput();
    }
    cheats_open_ = false;
    cheats_hover_ = -1;
    cheats_scroll_ = 0;
    cheats_adding_ = false;
}

bool MenuBar::handle_cheats_event(const SDL_Event& event) {
    if (!cheats_open_ || !cheat_engine_) return false;

    if (cheats_adding_) {
        if (event.type == SDL_KEYDOWN) {
            auto sym = event.key.keysym.sym;
            if (sym == SDLK_ESCAPE) {
                cheats_adding_ = false;
                SDL_StopTextInput();
                return true;
            }
            if (sym == SDLK_RETURN) {
                if (cheats_add_step_ == 0) {
                    if (!cheats_add_name_.empty()) {
                        cheats_add_step_ = 1;
                        cheats_add_codes_.clear();
                    }
                } else {
                    if (cheats_add_codes_.empty() || cheats_add_codes_.back() == '\n') {
                        SDL_StopTextInput();
                        cheats_adding_ = false;
                        if (!cheats_add_codes_.empty()) {
                            cheat_engine_->add(cheats_add_name_, cheats_add_codes_);
                        }
                    } else {
                        cheats_add_codes_ += '\n';
                    }
                }
                return true;
            }
            if (sym == SDLK_BACKSPACE) {
                if (cheats_add_step_ == 0) {
                    if (!cheats_add_name_.empty()) cheats_add_name_.pop_back();
                } else {
                    if (!cheats_add_codes_.empty()) cheats_add_codes_.pop_back();
                }
                return true;
            }
        }
        if (event.type == SDL_TEXTINPUT) {
            if (cheats_add_step_ == 0) {
                cheats_add_name_ += event.text.text;
            } else {
                cheats_add_codes_ += event.text.text;
            }
            return true;
        }
        return true;
    }

    if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        close_cheats();
        return true;
    }

    if (event.type == SDL_MOUSEWHEEL) {
        cheats_scroll_ -= event.wheel.y * 2;
        if (cheats_scroll_ < 0) cheats_scroll_ = 0;
        int max_scroll = std::max(0, cheat_engine_->count() - 6);
        if (cheats_scroll_ > max_scroll) cheats_scroll_ = max_scroll;
        return true;
    }

    if (event.type == SDL_MOUSEMOTION) {
        cheats_hover_ = -1;
        int mx = event.motion.x;
        int my = event.motion.y;

        int row_h = CHAR_H + 14;
        int panel_w = 420;
        int title_h = CHAR_H + 16;
        int max_visible = 6;
        int total = cheat_engine_->count();
        int visible = std::min(total, max_visible);
        int btn_h = CHAR_H + 14;
        int panel_h = title_h + visible * row_h + btn_h + 16;

        SDL_Window* win = SDL_GetWindowFromID(event.motion.windowID);
        if (!win) return true;
        int win_w, win_h;
        SDL_GetWindowSize(win, &win_w, &win_h);

        int px = (win_w - panel_w) / 2;
        int py = (win_h - panel_h) / 2;
        int row_y_start = py + title_h;

        if (mx >= px && mx < px + panel_w && my >= row_y_start && my < row_y_start + visible * row_h) {
            int row = (my - row_y_start) / row_h;
            int idx = row + cheats_scroll_;
            if (idx >= 0 && idx < total) {
                cheats_hover_ = idx;
            }
        }
        return true;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x;
        int my = event.button.y;

        int row_h = CHAR_H + 14;
        int panel_w = 420;
        int title_h = CHAR_H + 16;
        int max_visible = 6;
        int total = cheat_engine_->count();
        int visible = std::min(total, max_visible);
        int btn_h = CHAR_H + 14;
        int panel_h = title_h + std::max(visible, 1) * row_h + btn_h + 16;

        SDL_Window* win = SDL_GetWindowFromID(event.button.windowID);
        if (!win) return true;
        int win_w, win_h;
        SDL_GetWindowSize(win, &win_w, &win_h);

        int px = (win_w - panel_w) / 2;
        int py = (win_h - panel_h) / 2;
        int row_y_start = py + title_h;

        if (mx >= px && mx < px + panel_w && my >= row_y_start && my < row_y_start + visible * row_h) {
            int row = (my - row_y_start) / row_h;
            int idx = row + cheats_scroll_;
            if (idx >= 0 && idx < total) {
                int del_x = px + panel_w - 30;
                if (mx >= del_x) {
                    cheat_engine_->remove(idx);
                } else {
                    cheat_engine_->toggle(idx);
                }
            }
            return true;
        }

        int add_y = row_y_start + std::max(visible, 1) * row_h + 4;
        int add_w = text_width("+ Add Cheat") + 20;
        int add_x = px + (panel_w - add_w) / 2;
        if (mx >= add_x && mx < add_x + add_w && my >= add_y && my < add_y + btn_h) {
            cheats_adding_ = true;
            cheats_add_step_ = 0;
            cheats_add_name_.clear();
            cheats_add_codes_.clear();
            SDL_StartTextInput();
            return true;
        }

        return true;
    }

    return true;
}

void MenuBar::render_cheats(SDL_Renderer* renderer, int win_w, int win_h) {
    if (!cheats_open_ || !cheat_engine_) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xC0);
    SDL_Rect full = {0, 0, win_w, win_h};
    SDL_RenderFillRect(renderer, &full);

    int row_h = CHAR_H + 14;
    int panel_w = 420;
    int title_h = CHAR_H + 16;
    int max_visible = 6;
    int total = cheat_engine_->count();
    int visible = std::min(total, max_visible);
    if (visible == 0) visible = 1;
    int btn_h = CHAR_H + 14;
    int panel_h = title_h + visible * row_h + btn_h + 16;
    int px = (win_w - panel_w) / 2;
    int py = (win_h - panel_h) / 2;

    SDL_SetRenderDrawColor(renderer, 0x2A, 0x2A, 0x2A, 0xF0);
    SDL_Rect panel = {px, py, panel_w, panel_h};
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xFF);
    SDL_RenderDrawRect(renderer, &panel);

    const char* title = total == 0 ? "Cheats - None loaded (ESC to close)" : "Cheats (ESC to close)";
    int tw = text_width(title);
    draw_text(renderer, title, px + (panel_w - tw) / 2, py + 8, 0xFF, 0xFF, 0xFF);

    int ry = py + title_h;
    for (int i = 0; i < visible && (i + cheats_scroll_) < total; i++) {
        int idx = i + cheats_scroll_;
        const auto& entry = cheat_engine_->entry(idx);

        if (idx == cheats_hover_) {
            SDL_SetRenderDrawColor(renderer, 0x44, 0x44, 0x44, 0xFF);
            SDL_Rect row_bg = {px + 2, ry, panel_w - 4, row_h};
            SDL_RenderFillRect(renderer, &row_bg);
        }

        const char* status = entry.enabled ? "[ON]" : "[OFF]";
        u8 sr = entry.enabled ? 0x00 : 0xCC;
        u8 sg = entry.enabled ? 0xFF : 0x66;
        u8 sb = entry.enabled ? 0x00 : 0x66;
        draw_text(renderer, status, px + 8, ry + (row_h - CHAR_H) / 2, sr, sg, sb);

        int name_x = px + 8 + text_width("[OFF]") + 8;
        std::string display = entry.name;
        int max_chars = (panel_w - 70 - text_width("[OFF]")) / CHAR_W;
        if ((int)display.size() > max_chars) {
            display = display.substr(0, max_chars - 3) + "...";
        }
        draw_text(renderer, display.c_str(), name_x, ry + (row_h - CHAR_H) / 2, 0xFF, 0xFF, 0xFF);

        draw_text(renderer, "X", px + panel_w - 24, ry + (row_h - CHAR_H) / 2, 0xFF, 0x44, 0x44);

        ry += row_h;
    }

    if (total == 0 && !cheats_adding_) {
        draw_text(renderer, "No cheats loaded", px + 16, ry + (row_h - CHAR_H) / 2, 0x88, 0x88, 0x88);
        ry += row_h;
    }

    int add_y = py + title_h + visible * row_h + 4;

    if (cheats_adding_) {
        SDL_SetRenderDrawColor(renderer, 0x33, 0x33, 0x55, 0xFF);
        SDL_Rect input_bg = {px + 8, add_y, panel_w - 16, btn_h};
        SDL_RenderFillRect(renderer, &input_bg);

        if (cheats_add_step_ == 0) {
            std::string prompt = "Name: " + cheats_add_name_ + "_";
            draw_text(renderer, prompt.c_str(), px + 12, add_y + (btn_h - CHAR_H) / 2, 0xFF, 0xFF, 0x00);
        } else {
            std::string last_line = cheats_add_codes_;
            auto nl = last_line.rfind('\n');
            if (nl != std::string::npos) last_line = last_line.substr(nl + 1);
            std::string prompt = "Code: " + last_line + "_";
            draw_text(renderer, prompt.c_str(), px + 12, add_y + (btn_h - CHAR_H) / 2, 0xFF, 0xFF, 0x00);
        }
    } else {
        int add_w = text_width("+ Add Cheat") + 20;
        int add_x = px + (panel_w - add_w) / 2;

        SDL_SetRenderDrawColor(renderer, 0x33, 0x66, 0x99, 0xFF);
        SDL_Rect btn_rect = {add_x, add_y, add_w, btn_h};
        SDL_RenderFillRect(renderer, &btn_rect);
        SDL_SetRenderDrawColor(renderer, 0x55, 0x88, 0xBB, 0xFF);
        SDL_RenderDrawRect(renderer, &btn_rect);

        draw_text(renderer, "+ Add Cheat", add_x + 10, add_y + (btn_h - CHAR_H) / 2, 0xFF, 0xFF, 0xFF);
    }

    if (total > max_visible) {
        int sb_x = px + panel_w - 6;
        int sb_y = py + title_h;
        int sb_h = visible * row_h;

        SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 0xFF);
        SDL_Rect sb_bg = {sb_x, sb_y, 4, sb_h};
        SDL_RenderFillRect(renderer, &sb_bg);

        int thumb_h = std::max(10, sb_h * max_visible / total);
        int thumb_y = sb_y + (sb_h - thumb_h) * cheats_scroll_ / std::max(1, total - max_visible);

        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xFF);
        SDL_Rect thumb = {sb_x, thumb_y, 4, thumb_h};
        SDL_RenderFillRect(renderer, &thumb);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void MenuBar::draw_text(SDL_Renderer* renderer, const char* text, int x, int y,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!font_texture_) return;

    SDL_SetTextureColorMod(font_texture_, r, g, b);
    SDL_SetTextureAlphaMod(font_texture_, a);

    for (int i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        if (ch < 32 || ch > 126) ch = '?';
        int glyph_index = ch - 32;

        SDL_Rect src = {glyph_index * GLYPH_W, 0, GLYPH_W, GLYPH_H};
        SDL_Rect dst = {x + i * CHAR_W, y, CHAR_W, CHAR_H};
        SDL_RenderCopy(renderer, font_texture_, &src, &dst);
    }

    SDL_SetTextureAlphaMod(font_texture_, 255);
}

int MenuBar::text_width(const char* text) const {
    return static_cast<int>(strlen(text)) * CHAR_W;
}

int MenuBar::dropdown_width(const Menu& menu) const {
    int max_w = 0;
    for (const auto& item : menu.items) {
        if (!item.is_separator) {
            int w = text_width(item.label) + CHAR_W * 2;
            if (w > max_w) max_w = w;
        }
    }
    return max_w + DROPDOWN_PAD_X * 2 + 8;
}

int MenuBar::dropdown_height(const Menu& menu) const {
    int h = 0;
    for (const auto& item : menu.items) {
        h += item.is_separator ? SEPARATOR_H : DROPDOWN_ITEM_H;
    }
    return h;
}
