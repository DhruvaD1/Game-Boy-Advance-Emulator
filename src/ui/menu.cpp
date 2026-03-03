#include "menu.hpp"
#include "font.hpp"
#include <algorithm>
#include <cstring>

void MenuBar::init(SDL_Renderer* renderer) {
    font_texture_ = create_font_texture(renderer);

    menus_.clear();


    Menu file_menu;
    file_menu.label = "File";
    file_menu.items.push_back({"Save Game", MENU_SAVE_GAME, false, false});
    file_menu.items.push_back({"", 0, false, true});
    file_menu.items.push_back({"Quit", MENU_QUIT, false, false});
    menus_.push_back(file_menu);


    Menu speed_menu;
    speed_menu.label = "Speed";
    speed_menu.items.push_back({"1x", MENU_SPEED_1X, true, false});
    speed_menu.items.push_back({"2x", MENU_SPEED_2X, false, false});
    speed_menu.items.push_back({"4x", MENU_SPEED_4X, false, false});
    speed_menu.items.push_back({"8x", MENU_SPEED_8X, false, false});
    speed_menu.items.push_back({"Unlimited", MENU_SPEED_MAX, false, false});
    menus_.push_back(speed_menu);


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
    if (menus_.size() < 2) return;
    auto& items = menus_[1].items;
    for (size_t i = 0; i < items.size(); i++) {
        if (!items[i].is_separator) {
            items[i].checked = (static_cast<int>(i) == speed_index);
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

void MenuBar::draw_text(SDL_Renderer* renderer, const char* text, int x, int y,
                         uint8_t r, uint8_t g, uint8_t b) {
    if (!font_texture_) return;

    SDL_SetTextureColorMod(font_texture_, r, g, b);

    for (int i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        if (ch < 32 || ch > 126) ch = '?';
        int glyph_index = ch - 32;

        SDL_Rect src = {glyph_index * GLYPH_W, 0, GLYPH_W, GLYPH_H};
        SDL_Rect dst = {x + i * CHAR_W, y, CHAR_W, CHAR_H};
        SDL_RenderCopy(renderer, font_texture_, &src, &dst);
    }
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
