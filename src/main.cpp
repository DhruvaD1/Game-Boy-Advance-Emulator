// entry point — SDL2 window, input loop, frame timing
#include "gba.hpp"
#include <SDL.h>
#include <cstdio>
#include <chrono>
#include <thread>

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
        GBA_WIDTH * 3, GBA_HEIGHT * 3,
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

    SDL_RenderSetLogicalSize(renderer, GBA_WIDTH, GBA_HEIGHT);

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

    bool running = true;
    bool turbo = false;
    auto frame_start = std::chrono::high_resolution_clock::now();
    constexpr double FRAME_TIME = 1.0 / 59.7275;

    int frame_count = 0;
    auto fps_timer = std::chrono::high_resolution_clock::now();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                    if (event.key.keysym.sym == SDLK_TAB) turbo = true;
                    if (event.key.keysym.sym == SDLK_F5) gba.save_game();
                    if (event.key.keysym.sym == SDLK_F2) gba.dump_ppu_state();
                    break;
                case SDL_KEYUP:
                    if (event.key.keysym.sym == SDLK_TAB) turbo = false;
                    break;
            }
        }

        const u8* keys = SDL_GetKeyboardState(nullptr);
        gba.update_input(keys);

        gba.run_frame();

        SDL_UpdateTexture(texture, nullptr, gba.get_framebuffer(), GBA_WIDTH * sizeof(u32));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        frame_count++;

        auto now = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(now - fps_timer).count();
        if (elapsed >= 1.0) {
            char title[64];
            snprintf(title, sizeof(title), "GBA Emulator v2 - %.1f FPS", frame_count / elapsed);
            SDL_SetWindowTitle(window, title);
            frame_count = 0;
            fps_timer = now;
        }

        if (!turbo) {
            auto frame_end = std::chrono::high_resolution_clock::now();
            double frame_elapsed = std::chrono::duration<double>(frame_end - frame_start).count();
            if (frame_elapsed < FRAME_TIME) {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(FRAME_TIME - frame_elapsed));
            }
            frame_start = std::chrono::high_resolution_clock::now();
        }
    }

    gba.save_game();

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
