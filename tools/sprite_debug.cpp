#include "gba.hpp"
#include <cstdio>
#include <cstring>

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

    for (int f = 0; f < 1500; f++) {
        gba.run_frame();
    }

    printf("=== Frame 1500 OAM Dump (visible sprites) ===\n");

    u16 dispcnt;
    memcpy(&dispcnt, &gba.bus().io[0], 2);
    printf("DISPCNT=0x%04X mode=%d obj_1d=%d obj_en=%d\n",
           dispcnt, dispcnt & 7, (dispcnt >> 6) & 1, (dispcnt >> 12) & 1);

    bool obj_1d = dispcnt & (1 << 6);

    static const int widths[3][4]  = {{8,16,32,64}, {16,32,32,64}, {8,8,16,32}};
    static const int heights[3][4] = {{8,16,32,64}, {8,8,16,32}, {16,32,32,64}};

    int visible_count = 0;
    for (int i = 0; i < 128; i++) {
        u16 attr0, attr1, attr2;
        memcpy(&attr0, &gba.bus().oam[i * 8 + 0], 2);
        memcpy(&attr1, &gba.bus().oam[i * 8 + 2], 2);
        memcpy(&attr2, &gba.bus().oam[i * 8 + 4], 2);

        bool affine = attr0 & (1 << 8);
        bool obj_disable = !affine && (attr0 & (1 << 9));
        if (obj_disable) continue;

        int obj_mode = (attr0 >> 10) & 3;
        if (obj_mode == 2) continue;

        int shape = (attr0 >> 14) & 3;
        int size = (attr1 >> 14) & 3;
        if (shape == 3) continue;

        int w = widths[shape][size];
        int h = heights[shape][size];

        int obj_y = attr0 & 0xFF;
        int obj_x = attr1 & 0x1FF;
        if (obj_y >= 160) obj_y -= 256;
        if (obj_x >= 240) obj_x -= 512;

        int tile_num = attr2 & 0x3FF;
        int palette_num = (attr2 >> 12) & 0xF;
        u8 priority = (attr2 >> 10) & 3;
        bool color_256 = attr0 & (1 << 13);

        if (color_256) tile_num &= ~1;

        printf("OBJ[%3d]: pos=(%4d,%4d) size=%dx%d tile=%d pal=%d pri=%d 8bpp=%d mode=%d affine=%d\n",
               i, obj_x, obj_y, w, h, tile_num, palette_num, priority, color_256, obj_mode, affine);

        // Dump first 32 bytes of tile data
        u32 base = 0x06010000;
        u32 vram_offset = (base + tile_num * 32) & 0x1FFFF;
        if (vram_offset >= 0x18000) vram_offset -= 0x8000;

        printf("  Tile data @VRAM[0x%05X]: ", vram_offset);
        for (int b = 0; b < 32 && b < 64; b++) {
            printf("%02X", gba.bus().vram[vram_offset + b]);
        }
        printf("\n");

        visible_count++;
        if (visible_count > 30) break;
    }

    printf("\nTotal visible sprites shown: %d\n", visible_count);

    // Dump OBJ VRAM usage stats
    int nonzero_bytes = 0;
    for (int i = 0x10000; i < 0x18000; i++) {
        if (gba.bus().vram[i] != 0) nonzero_bytes++;
    }
    printf("OBJ VRAM (0x10000-0x17FFF): %d non-zero bytes out of %d\n", nonzero_bytes, 0x8000);

    // Dump OBJ palette
    printf("\nOBJ Palette (first 16 entries of palette 0):\n");
    for (int i = 0; i < 16; i++) {
        u16 color;
        memcpy(&color, &gba.bus().palette[0x200 + i * 2], 2);
        int r = (color & 0x1F) << 3;
        int g = ((color >> 5) & 0x1F) << 3;
        int b = ((color >> 10) & 0x1F) << 3;
        printf("  [%2d] 0x%04X -> RGB(%3d,%3d,%3d)\n", i, color, r, g, b);
    }

    return 0;
}
