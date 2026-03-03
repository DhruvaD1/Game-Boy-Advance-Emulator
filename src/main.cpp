// entry point — SDL2 window, input loop, frame timing
#include "gba.hpp"
#include "ui/menu.hpp"
#include <SDL.h>
#include <cstdio>
#include <chrono>
#include <thread>

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

            if (menu.handle_event(event)) continue;

            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                    if (event.key.keysym.sym == SDLK_F5) gba.save_game();
                    if (event.key.keysym.sym == SDLK_F2) gba.dump_ppu_state();


                    if (event.key.keysym.sym == SDLK_TAB) {
                        speed_index = (speed_index + 1) % NUM_SPEEDS;
                        menu.set_speed_check(speed_index);
                        title_dirty = true;
                    }


                    if (event.key.keysym.sym >= SDLK_1 && event.key.keysym.sym <= SDLK_5) {
                        speed_index = event.key.keysym.sym - SDLK_1;
                        menu.set_speed_check(speed_index);
                        title_dirty = true;
                    }
                    break;
            }
        }


        int action = menu.poll_action();
        switch (action) {
            case MENU_QUIT:
                running = false;
                break;
            case MENU_SAVE_GAME:
                gba.save_game();
                break;
            case MENU_SPEED_1X: case MENU_SPEED_2X: case MENU_SPEED_4X:
            case MENU_SPEED_8X: case MENU_SPEED_MAX:
                speed_index = action - MENU_SPEED_1X;
                menu.set_speed_check(speed_index);
                title_dirty = true;
                break;
            default: break;
        }

        const u8* keys = SDL_GetKeyboardState(nullptr);
        gba.update_input(keys);

        const auto& speed = SPEEDS[speed_index];


        for (int i = 0; i < speed.skip_frames; i++) {
            gba.run_frame();
        }

        gba.run_frame();

        SDL_UpdateTexture(texture, nullptr, gba.get_framebuffer(), GBA_WIDTH * sizeof(u32));

        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_Rect game_dst = menu.game_rect(win_w, win_h);

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(renderer);
        menu.render(renderer, win_w);
        SDL_RenderCopy(renderer, texture, nullptr, &game_dst);
        SDL_RenderPresent(renderer);

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

    gba.save_game();

    menu.destroy();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
