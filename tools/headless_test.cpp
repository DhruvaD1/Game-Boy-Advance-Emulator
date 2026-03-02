#include "gba.hpp"
#include <cstdio>

static void dump_ppm(const char* path, const u32* fb) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n240 160\n255\n");
    for (int i = 0; i < 240 * 160; i++) {
        u8 rgb[3] = {
            (u8)(fb[i] & 0xFF),
            (u8)((fb[i] >> 8) & 0xFF),
            (u8)((fb[i] >> 16) & 0xFF)
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.gba>\n", argv[0]);
        return 1;
    }

    GBA gba;
    if (!gba.load_rom(argv[1])) {
        fprintf(stderr, "Failed to load ROM\n");
        return 1;
    }

    int dump_frames[] = {100, 300, 500, 800, 1200, 1500};
    int num_dumps = 6;
    int dump_idx = 0;

    for (int f = 0; f < 1600; f++) {
        gba.run_frame();
        if (dump_idx < num_dumps && f == dump_frames[dump_idx]) {
            char path[64];
            snprintf(path, sizeof(path), "frame_%04d.ppm", f);
            dump_ppm(path, gba.get_framebuffer());
            printf("Dumped %s\n", path);
            dump_idx++;
        }
    }

    printf("Headless test complete: 1600 frames\n");
    return 0;
}
