#include "gba.hpp"
#include "ui/menu.hpp"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <string>
#include <libgen.h>

struct SpeedOption {
    const char* label;
    int skip_frames;
    bool use_limiter;
};

static const SpeedOption SPEEDS[] = {
    {"1x",        0, true },
    {"2x",        1, true },
    {"4x",        3, true },
    {"8x",        7, true },
    {"Unlimited", 7, false},
};
static constexpr int NUM_SPEEDS = sizeof(SPEEDS) / sizeof(SPEEDS[0]);

static const char* CONFIG_FILE = "controls.cfg";

static std::string get_rom_dir(const char* rom_path) {
    std::string path_copy = rom_path;
    char* dir = dirname(&path_copy[0]);
    return std::string(dir);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.gba>\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "GBA Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GBA_WIDTH * 3, GBA_HEIGHT * 3 + 24,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        GBA_WIDTH, GBA_HEIGHT);

    GBA gba;
    gba.input().load_config(CONFIG_FILE);

    std::string rom_dir = get_rom_dir(argv[1]);

    if (!gba.load_rom(argv[1])) {
        fprintf(stderr, "Failed to load ROM\n");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    MenuBar menu;
    menu.init(renderer);
    menu.set_speed_check(0);
    menu.set_input(&gba.input());
    menu.set_cheat_engine(&gba.cheat_engine(), gba.cheats_path());

    bool running = true;
    int speed_index = 0;
    auto frame_start = std::chrono::high_resolution_clock::now();
    constexpr double FRAME_TIME = 1.0 / 59.7275;

    int frame_count = 0;
    auto fps_timer = std::chrono::high_resolution_clock::now();
    bool title_dirty = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {

            if (menu.rom_browser_open()) {
                if (menu.handle_rom_browser_event(event)) {
                    std::string picked = menu.selected_rom_path();
                    if (!picked.empty()) {
                        menu.clear_selected_rom();
                        if (gba.reset_and_load_rom(picked)) {
                            rom_dir = get_rom_dir(picked.c_str());
                            menu.set_cheat_engine(&gba.cheat_engine(), gba.cheats_path());
                            menu.show_notification("ROM loaded");
                            frame_count = 0;
                            fps_timer = std::chrono::high_resolution_clock::now();
                            title_dirty = true;
                        } else {
                            menu.show_notification("Failed to load ROM");
                        }
                    }
                    continue;
                }
            }

            if (menu.controls_open()) {
                if (menu.handle_controls_event(event)) {
                    if (!menu.controls_open()) {
                        gba.input().save_config(CONFIG_FILE);
                    }
                    continue;
                }
            }

            if (menu.cheats_open()) {
                if (menu.handle_cheats_event(event)) {
                    if (!menu.cheats_open()) {
                        gba.save_cheats();
                    }
                    continue;
                }
            }

            if (menu.handle_event(event)) continue;

            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                {
                    auto sym = event.key.keysym.sym;
                    bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;

                    if (sym == SDLK_ESCAPE) running = false;
                    if (sym == SDLK_F6) {
                        gba.save_game();
                        menu.show_notification("Game saved");
                    }


                    if (shift && sym >= SDLK_F1 && sym <= SDLK_F5) {
                        int slot = sym - SDLK_F1 + 1;
                        if (gba.save_state(slot)) {
                            char msg[32];
                            snprintf(msg, sizeof(msg), "State saved to slot %d", slot);
                            menu.show_notification(msg);
                        } else {
                            menu.show_notification("Save state failed");
                        }
                    }

                    else if (!shift && sym >= SDLK_F1 && sym <= SDLK_F5) {
                        int slot = sym - SDLK_F1 + 1;
                        if (gba.load_state(slot)) {
                            char msg[32];
                            snprintf(msg, sizeof(msg), "State loaded from slot %d", slot);
                            menu.show_notification(msg);
                        } else {
                            char msg[32];
                            snprintf(msg, sizeof(msg), "No state in slot %d", slot);
                            menu.show_notification(msg);
                        }
                    }

                    if (sym == SDLK_TAB) {
                        speed_index = (speed_index + 1) % NUM_SPEEDS;
                        menu.set_speed_check(speed_index);
                        title_dirty = true;
                    }

                    if (sym >= SDLK_1 && sym <= SDLK_5) {
                        speed_index = sym - SDLK_1;
                        menu.set_speed_check(speed_index);
                        title_dirty = true;
                    }
                    break;
                }
            }
        }


        int action = menu.poll_action();
        switch (action) {
            case MENU_QUIT:
                running = false;
                break;
            case MENU_SAVE_GAME:
                gba.save_game();
                menu.show_notification("Game saved");
                break;
            case MENU_LOAD_ROM:
                menu.open_rom_browser(rom_dir);
                break;
            case MENU_CHANGE_CONTROLS:
                menu.open_controls();
                break;
            case MENU_CHEATS:
                menu.open_cheats();
                break;
            case MENU_COLOR_CORRECT:
                gba.set_color_correction(!gba.color_correction());
                menu.set_color_correct_check(gba.color_correction());
                break;
            case MENU_SPEED_1X: case MENU_SPEED_2X: case MENU_SPEED_4X:
            case MENU_SPEED_8X: case MENU_SPEED_MAX:
                speed_index = action - MENU_SPEED_1X;
                menu.set_speed_check(speed_index);
                title_dirty = true;
                break;
            case MENU_SAVE_STATE_1: case MENU_SAVE_STATE_2: case MENU_SAVE_STATE_3:
            case MENU_SAVE_STATE_4: case MENU_SAVE_STATE_5:
            {
                int slot = action - MENU_SAVE_STATE_1 + 1;
                if (gba.save_state(slot)) {
                    char msg[32];
                    snprintf(msg, sizeof(msg), "State saved to slot %d", slot);
                    menu.show_notification(msg);
                } else {
                    menu.show_notification("Save state failed");
                }
                break;
            }
            case MENU_LOAD_STATE_1: case MENU_LOAD_STATE_2: case MENU_LOAD_STATE_3:
            case MENU_LOAD_STATE_4: case MENU_LOAD_STATE_5:
            {
                int slot = action - MENU_LOAD_STATE_1 + 1;
                if (gba.load_state(slot)) {
                    char msg[32];
                    snprintf(msg, sizeof(msg), "State loaded from slot %d", slot);
                    menu.show_notification(msg);
                } else {
                    char msg[32];
                    snprintf(msg, sizeof(msg), "No state in slot %d", slot);
                    menu.show_notification(msg);
                }
                break;
            }
            default: break;
        }

        bool overlay_open = menu.controls_open() || menu.rom_browser_open() || menu.cheats_open();

        if (!overlay_open) {
            const u8* keys = SDL_GetKeyboardState(nullptr);
            gba.update_input(keys);

            const auto& speed = SPEEDS[speed_index];

            for (int i = 0; i < speed.skip_frames; i++) {
                gba.run_frame();
            }

            gba.run_frame();

            SDL_UpdateTexture(texture, nullptr, gba.get_framebuffer(), GBA_WIDTH * sizeof(u32));
        }

        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_Rect game_dst = menu.game_rect(win_w, win_h);

        menu.tick_notification();

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, &game_dst);
        menu.render(renderer, win_w);
        menu.render_notification(renderer, game_dst);
        menu.render_controls(renderer, win_w, win_h);
        menu.render_rom_browser(renderer, win_w, win_h);
        menu.render_cheats(renderer, win_w, win_h);
        SDL_RenderPresent(renderer);

        if (!overlay_open) {
            const auto& speed = SPEEDS[speed_index];
            frame_count += speed.skip_frames + 1;

            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(now - fps_timer).count();
            if (elapsed >= 1.0 || title_dirty) {
                char title[128];
                snprintf(title, sizeof(title),
                         "GBA Emulator - %.1f FPS  |  Speed: %s (Tab/1-5 to change)",
                         frame_count / std::max(elapsed, 0.001), speed.label);
                SDL_SetWindowTitle(window, title);
                if (elapsed >= 1.0) {
                    frame_count = 0;
                    fps_timer = now;
                }
                title_dirty = false;
            }

            if (speed.use_limiter) {
                double target = FRAME_TIME / (speed.skip_frames + 1);
                auto frame_end = std::chrono::high_resolution_clock::now();
                double frame_elapsed = std::chrono::duration<double>(frame_end - frame_start).count();
                if (frame_elapsed < target) {
                    std::this_thread::sleep_for(
                        std::chrono::duration<double>(target - frame_elapsed));
                }
                frame_start = std::chrono::high_resolution_clock::now();
            }
        }
    }

    gba.input().save_config(CONFIG_FILE);
    gba.save_cheats();
    gba.save_game();

    menu.destroy();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
