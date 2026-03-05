// PPU rendering — text/affine BGs, sprites, windowing, blending
#include "ppu/ppu.hpp"
#include "memory/bus.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>

PPU::PPU() {
    framebuffer_.fill(0);
    build_lut();
}

u16 PPU::read_palette(u32 addr) const {
    u16 v;
    memcpy(&v, &bus_->palette[addr & 0x3FF], 2);
    return v;
}

u16 PPU::read_vram16(u32 addr) const {
    u32 offset = addr & 0x1FFFF;
    if (offset >= 0x18000) offset -= 0x8000;
    u16 v;
    memcpy(&v, &bus_->vram[offset], 2);
    return v;
}

u8 PPU::read_vram8(u32 addr) const {
    u32 offset = addr & 0x1FFFF;
    if (offset >= 0x18000) offset -= 0x8000;
    return bus_->vram[offset];
}

bool PPU::save_state(FILE* f) const {
    if (fwrite(framebuffer_.data(), sizeof(framebuffer_), 1, f) != 1) return false;
    if (fwrite(&bg2_ref_x_, sizeof(bg2_ref_x_), 1, f) != 1) return false;
    if (fwrite(&bg2_ref_y_, sizeof(bg2_ref_y_), 1, f) != 1) return false;
    if (fwrite(&bg3_ref_x_, sizeof(bg3_ref_x_), 1, f) != 1) return false;
    if (fwrite(&bg3_ref_y_, sizeof(bg3_ref_y_), 1, f) != 1) return false;
    return true;
}

bool PPU::load_state(FILE* f) {
    if (fread(framebuffer_.data(), sizeof(framebuffer_), 1, f) != 1) return false;
    if (fread(&bg2_ref_x_, sizeof(bg2_ref_x_), 1, f) != 1) return false;
    if (fread(&bg2_ref_y_, sizeof(bg2_ref_y_), 1, f) != 1) return false;
    if (fread(&bg3_ref_x_, sizeof(bg3_ref_x_), 1, f) != 1) return false;
    if (fread(&bg3_ref_y_, sizeof(bg3_ref_y_), 1, f) != 1) return false;
    return true;
}

void PPU::build_lut() {
    for (int c = 0; c < 32768; c++) {
        int r5 = c & 0x1F;
        int g5 = (c >> 5) & 0x1F;
        int b5 = (c >> 10) & 0x1F;

        u8 r8, g8, b8;
        if (!color_correct_) {
            r8 = r5 << 3;
            g8 = g5 << 3;
            b8 = b5 << 3;
        } else {
            double r = r5 / 31.0;
            double g = g5 / 31.0;
            double b = b5 / 31.0;

            r = std::pow(r, 4.0);
            g = std::pow(g, 4.0);
            b = std::pow(b, 4.0);

            double nr = 0.80 * r + 0.20 * g;
            double ng = 0.15 * r + 0.75 * g + 0.10 * b;
            double nb = 0.15 * g + 0.85 * b;

            nr = std::pow(nr, 1.0 / 2.2);
            ng = std::pow(ng, 1.0 / 2.2);
            nb = std::pow(nb, 1.0 / 2.2);

            if (nr > 1.0) nr = 1.0;
            if (ng > 1.0) ng = 1.0;
            if (nb > 1.0) nb = 1.0;

            r8 = (u8)(nr * 255.0 + 0.5);
            g8 = (u8)(ng * 255.0 + 0.5);
            b8 = (u8)(nb * 255.0 + 0.5);
        }

        color_lut_[c] = r8 | (g8 << 8) | (b8 << 16) | 0xFF000000u;
    }
}

void PPU::set_color_correction(bool on) {
    color_correct_ = on;
    build_lut();
}

u32 PPU::rgb555_to_rgb888(u16 color) const {
    return color_lut_[color & 0x7FFF];
}

void PPU::set_vcount(u16 v) {
    u16 dispstat;
    memcpy(&dispstat, &bus_->io[0x004], 2);

    dispstat &= ~(1 << 2);

    u8 target = (dispstat >> 8) & 0xFF;
    if ((u8)v == target) {
        dispstat |= (1 << 2);
    }

    memcpy(&bus_->io[0x004], &dispstat, 2);

    u16 vcount = v;
    memcpy(&bus_->io[0x006], &vcount, 2);
}

u16 PPU::get_vcount() const {
    u16 v;
    memcpy(&v, &bus_->io[0x006], 2);
    return v;
}

void PPU::set_hblank(bool v) {
    u16 dispstat;
    memcpy(&dispstat, &bus_->io[0x004], 2);
    if (v) dispstat |= (1 << 1); else dispstat &= ~(1 << 1);
    memcpy(&bus_->io[0x004], &dispstat, 2);
}

void PPU::set_vblank(bool v) {
    u16 dispstat;
    memcpy(&dispstat, &bus_->io[0x004], 2);
    if (v) dispstat |= (1 << 0); else dispstat &= ~(1 << 0);
    memcpy(&bus_->io[0x004], &dispstat, 2);
}

bool PPU::vblank_irq_enabled() const {
    u16 dispstat;
    memcpy(&dispstat, &bus_->io[0x004], 2);
    return dispstat & (1 << 3);
}

bool PPU::hblank_irq_enabled() const {
    u16 dispstat;
    memcpy(&dispstat, &bus_->io[0x004], 2);
    return dispstat & (1 << 4);
}

bool PPU::vcount_irq_enabled() const {
    u16 dispstat;
    memcpy(&dispstat, &bus_->io[0x004], 2);
    return dispstat & (1 << 5);
}

u8 PPU::vcount_target() const {
    u16 dispstat;
    memcpy(&dispstat, &bus_->io[0x004], 2);
    return dispstat >> 8;
}

void PPU::latch_bg2_ref() {
    u32 x_raw = 0, y_raw = 0;
    memcpy(&x_raw, &bus_->io[0x028], 4);
    memcpy(&y_raw, &bus_->io[0x02C], 4);
    bg2_ref_x_ = (s32)(x_raw << 4) >> 4;
    bg2_ref_y_ = (s32)(y_raw << 4) >> 4;
}

void PPU::latch_bg3_ref() {
    u32 x_raw = 0, y_raw = 0;
    memcpy(&x_raw, &bus_->io[0x038], 4);
    memcpy(&y_raw, &bus_->io[0x03C], 4);
    bg3_ref_x_ = (s32)(x_raw << 4) >> 4;
    bg3_ref_y_ = (s32)(y_raw << 4) >> 4;
}

void PPU::render_bg_text(int bg, int line) {
    u16 bgcnt;
    memcpy(&bgcnt, &bus_->io[0x008 + bg * 2], 2);

    u16 hofs_val, vofs_val;
    memcpy(&hofs_val, &bus_->io[0x010 + bg * 4], 2);
    memcpy(&vofs_val, &bus_->io[0x012 + bg * 4], 2);
    int hofs = hofs_val & 0x1FF;
    int vofs = vofs_val & 0x1FF;

    int tile_base = ((bgcnt >> 2) & 3) * 0x4000;
    int map_base = ((bgcnt >> 8) & 0x1F) * 0x800;
    bool color_256 = bgcnt & (1 << 7);
    int screen_size = (bgcnt >> 14) & 3;
    u8 priority = bgcnt & 3;

    int map_w = (screen_size & 1) ? 64 : 32;
    int map_h = (screen_size & 2) ? 64 : 32;

    int y = (line + vofs) % (map_h * 8);
    int tile_row = y / 8;
    int pixel_y = y % 8;

    for (int px = 0; px < 240; px++) {
        int x = (px + hofs) % (map_w * 8);
        int tile_col = x / 8;
        int pixel_x = x % 8;

        int screen_block = 0;
        int tx = tile_col, ty = tile_row;
        if (tile_col >= 32) { screen_block += 1; tx -= 32; }
        if (tile_row >= 32) { screen_block += (screen_size & 1) ? 2 : 1; ty -= 32; }

        u32 map_addr = map_base + screen_block * 0x800 + (ty * 32 + tx) * 2;
        u16 map_entry = read_vram16(0x06000000 + map_addr);

        int tile_num = map_entry & 0x3FF;
        bool flip_h = map_entry & (1 << 10);
        bool flip_v = map_entry & (1 << 11);
        int palette_num = (map_entry >> 12) & 0xF;

        int fy = flip_v ? (7 - pixel_y) : pixel_y;
        int fx = flip_h ? (7 - pixel_x) : pixel_x;

        u8 color_idx;
        if (color_256) {
            u32 tile_addr = tile_base + tile_num * 64 + fy * 8 + fx;
            color_idx = read_vram8(0x06000000 + tile_addr);
            if (color_idx == 0) {
                bg_transparent_[bg][px] = true;
            } else {
                bg_line_[bg][px] = read_palette(color_idx * 2);
                bg_transparent_[bg][px] = false;
            }
        } else {
            u32 tile_addr = tile_base + tile_num * 32 + fy * 4 + (fx / 2);
            u8 byte = read_vram8(0x06000000 + tile_addr);
            color_idx = (fx & 1) ? (byte >> 4) : (byte & 0xF);
            if (color_idx == 0) {
                bg_transparent_[bg][px] = true;
            } else {
                bg_line_[bg][px] = read_palette(palette_num * 32 + color_idx * 2);
                bg_transparent_[bg][px] = false;
            }
        }
        bg_priority_[bg][px] = priority;
    }
}

void PPU::render_bg_affine(int bg, int line) {
    u16 bgcnt;
    memcpy(&bgcnt, &bus_->io[0x008 + bg * 2], 2);

    int tile_base = ((bgcnt >> 2) & 3) * 0x4000;
    int map_base = ((bgcnt >> 8) & 0x1F) * 0x800;
    int screen_size = (bgcnt >> 14) & 3;
    bool wrap = bgcnt & (1 << 13);
    u8 priority = bgcnt & 3;

    int map_dim = 16 << screen_size;
    int pixel_dim = map_dim * 8;

    s32 *ref_x, *ref_y;
    int pa_reg, pb_reg;
    if (bg == 2) {
        ref_x = &bg2_ref_x_; ref_y = &bg2_ref_y_;
        pa_reg = 0x020; pb_reg = 0x022;
    } else {
        ref_x = &bg3_ref_x_; ref_y = &bg3_ref_y_;
        pa_reg = 0x030; pb_reg = 0x032;
    }

    s16 pa, pb, pc_val, pd;
    memcpy(&pa, &bus_->io[pa_reg], 2);
    memcpy(&pb, &bus_->io[pa_reg + 2], 2);
    int pc_reg = pa_reg + 4;
    int pd_reg = pa_reg + 6;
    memcpy(&pc_val, &bus_->io[pc_reg], 2);
    memcpy(&pd, &bus_->io[pd_reg], 2);

    s32 cx = *ref_x;
    s32 cy = *ref_y;

    for (int px = 0; px < 240; px++) {
        int tx = cx >> 8;
        int ty = cy >> 8;

        bool valid = true;
        if (wrap) {
            tx = ((tx % pixel_dim) + pixel_dim) % pixel_dim;
            ty = ((ty % pixel_dim) + pixel_dim) % pixel_dim;
        } else if (tx < 0 || tx >= pixel_dim || ty < 0 || ty >= pixel_dim) {
            valid = false;
        }

        if (valid) {
            int tile_x = tx / 8;
            int tile_y = ty / 8;
            u32 map_addr = map_base + tile_y * map_dim + tile_x;
            u8 tile_num = read_vram8(0x06000000 + map_addr);

            int pixel_x = tx % 8;
            int pixel_y = ty % 8;
            u32 tile_addr = tile_base + tile_num * 64 + pixel_y * 8 + pixel_x;
            u8 color_idx = read_vram8(0x06000000 + tile_addr);

            if (color_idx == 0) {
                bg_transparent_[bg][px] = true;
            } else {
                bg_line_[bg][px] = read_palette(color_idx * 2);
                bg_transparent_[bg][px] = false;
            }
        } else {
            bg_transparent_[bg][px] = true;
        }
        bg_priority_[bg][px] = priority;

        cx += pa;
        cy += pc_val;
    }

    *ref_x += pb;
    *ref_y += pd;
}

void PPU::render_sprites(int line) {
    u16 dispcnt;
    memcpy(&dispcnt, &bus_->io[0], 2);
    bool obj_1d = dispcnt & (1 << 6);

    obj_transparent_.fill(true);

    for (int i = 127; i >= 0; i--) {
        u16 attr0, attr1, attr2;
        memcpy(&attr0, &bus_->oam[i * 8 + 0], 2);
        memcpy(&attr1, &bus_->oam[i * 8 + 2], 2);
        memcpy(&attr2, &bus_->oam[i * 8 + 4], 2);

        bool affine = attr0 & (1 << 8);
        bool obj_disable = !affine && (attr0 & (1 << 9));
        if (obj_disable) continue;

        bool double_size = affine && (attr0 & (1 << 9));

        int obj_mode = (attr0 >> 10) & 3;
        if (obj_mode == 2) continue;

        int shape = (attr0 >> 14) & 3;
        int size = (attr1 >> 14) & 3;

        static const int widths[3][4]  = {{8,16,32,64}, {16,32,32,64}, {8,8,16,32}};
        static const int heights[3][4] = {{8,16,32,64}, {8,8,16,32}, {16,32,32,64}};

        if (shape == 3) continue;
        int w = widths[shape][size];
        int h = heights[shape][size];

        int bound_w = double_size ? w * 2 : w;
        int bound_h = double_size ? h * 2 : h;

        int obj_y = attr0 & 0xFF;
        if (obj_y >= 160) obj_y -= 256;

        if (line < obj_y || line >= obj_y + bound_h) continue;

        int obj_x = attr1 & 0x1FF;
        if (obj_x >= 240) obj_x -= 512;

        int tile_num = attr2 & 0x3FF;
        int palette_num = (attr2 >> 12) & 0xF;
        u8 priority = (attr2 >> 10) & 3;
        bool color_256 = attr0 & (1 << 13);
        bool semi_transparent = (obj_mode == 1);

        int bg_mode = dispcnt & 7;
        if (bg_mode >= 3 && bg_mode <= 5 && tile_num < 512) continue;

        if (color_256) tile_num &= ~1;

        bool flip_h = !affine && (attr1 & (1 << 12));
        bool flip_v = !affine && (attr1 & (1 << 13));

        int local_y = line - obj_y;

        for (int local_x = 0; local_x < bound_w; local_x++) {
            int screen_x = obj_x + local_x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int tex_x, tex_y;

            if (affine) {
                int affine_idx = (attr1 >> 9) & 0x1F;
                s16 aff_pa, aff_pb, aff_pc, aff_pd;
                memcpy(&aff_pa, &bus_->oam[affine_idx * 32 + 0x06], 2);
                memcpy(&aff_pb, &bus_->oam[affine_idx * 32 + 0x0E], 2);
                memcpy(&aff_pc, &bus_->oam[affine_idx * 32 + 0x16], 2);
                memcpy(&aff_pd, &bus_->oam[affine_idx * 32 + 0x1E], 2);

                int cx = bound_w / 2;
                int cy = bound_h / 2;
                int dx = local_x - cx;
                int dy = local_y - cy;

                tex_x = ((aff_pa * dx + aff_pb * dy) >> 8) + w / 2;
                tex_y = ((aff_pc * dx + aff_pd * dy) >> 8) + h / 2;

                if (tex_x < 0 || tex_x >= w || tex_y < 0 || tex_y >= h) continue;
            } else {
                tex_x = flip_h ? (w - 1 - local_x) : local_x;
                tex_y = flip_v ? (h - 1 - local_y) : local_y;
            }

            u8 color_idx;
            int tile_x = tex_x / 8;
            int tile_y = tex_y / 8;
            int px_x = tex_x % 8;
            int px_y = tex_y % 8;

            int actual_tile;
            if (obj_1d) {
                actual_tile = tile_num + tile_y * (color_256 ? w / 4 : w / 8) + (color_256 ? tile_x * 2 : tile_x);
            } else {
                actual_tile = tile_num + tile_y * 32 + (color_256 ? tile_x * 2 : tile_x);
            }

            u32 base = 0x06010000;
            if (color_256) {
                u32 addr = base + actual_tile * 32 + px_y * 8 + px_x;
                color_idx = read_vram8(addr);
                if (color_idx == 0) continue;
                u16 color = read_palette(0x200 + color_idx * 2);
                obj_line_[screen_x] = color;
            } else {
                u32 addr = base + actual_tile * 32 + px_y * 4 + px_x / 2;
                u8 byte = read_vram8(addr);
                color_idx = (px_x & 1) ? (byte >> 4) : (byte & 0xF);
                if (color_idx == 0) continue;
                u16 color = read_palette(0x200 + palette_num * 32 + color_idx * 2);
                obj_line_[screen_x] = color;
            }

            obj_transparent_[screen_x] = false;
            obj_priority_[screen_x] = priority;
            obj_semi_transparent_[screen_x] = semi_transparent;
        }
    }
}

void PPU::composite_scanline(int line) {
    u16 dispcnt;
    memcpy(&dispcnt, &bus_->io[0], 2);
    int mode = dispcnt & 7;

    u16 bldcnt;
    memcpy(&bldcnt, &bus_->io[0x050], 2);
    int blend_mode = (bldcnt >> 6) & 3;

    u16 bldalpha;
    memcpy(&bldalpha, &bus_->io[0x052], 2);
    int eva = std::min(16, (int)(bldalpha & 0x1F));
    int evb = std::min(16, (int)((bldalpha >> 8) & 0x1F));

    u16 bldy;
    memcpy(&bldy, &bus_->io[0x054], 2);
    int evy = std::min(16, (int)(bldy & 0x1F));

    u16 backdrop = read_palette(0);

    bool bg_enabled[4];
    bg_enabled[0] = (dispcnt & (1 << 8)) != 0;
    bg_enabled[1] = (dispcnt & (1 << 9)) != 0;
    bg_enabled[2] = (dispcnt & (1 << 10)) != 0;
    bg_enabled[3] = (dispcnt & (1 << 11)) != 0;

    switch (mode) {
        case 0: break;
        case 1: bg_enabled[3] = false; break;
        case 2: bg_enabled[0] = false; bg_enabled[1] = false; break;
        case 3: case 4: case 5:
            bg_enabled[0] = false; bg_enabled[1] = false;
            bg_enabled[3] = false; break;
        default: break;
    }

    bool obj_enabled = (dispcnt & (1 << 12)) != 0;

    bool win0_on = (dispcnt & (1 << 13)) != 0;
    bool win1_on = (dispcnt & (1 << 14)) != 0;
    bool objwin_on = (dispcnt & (1 << 15)) != 0;
    bool any_window = win0_on || win1_on || objwin_on;

    u8 win0_x1 = 0, win0_x2 = 0, win0_y1 = 0, win0_y2 = 0;
    u8 win1_x1 = 0, win1_x2 = 0, win1_y1 = 0, win1_y2 = 0;
    u16 winin = 0, winout = 0;

    if (any_window) {
        u16 win0h, win1h, win0v, win1v;
        memcpy(&win0h, &bus_->io[0x040], 2);
        memcpy(&win1h, &bus_->io[0x042], 2);
        memcpy(&win0v, &bus_->io[0x044], 2);
        memcpy(&win1v, &bus_->io[0x046], 2);
        win0_x2 = win0h & 0xFF; win0_x1 = win0h >> 8;
        win1_x2 = win1h & 0xFF; win1_x1 = win1h >> 8;
        win0_y2 = win0v & 0xFF; win0_y1 = win0v >> 8;
        win1_y2 = win1v & 0xFF; win1_y1 = win1v >> 8;
        memcpy(&winin, &bus_->io[0x048], 2);
        memcpy(&winout, &bus_->io[0x04A], 2);
    }

    bool win0_y_in = false, win1_y_in = false;
    if (win0_on) {
        if (win0_y1 <= win0_y2)
            win0_y_in = (line >= win0_y1 && line < win0_y2);
        else
            win0_y_in = (line >= win0_y1 || line < win0_y2);
    }
    if (win1_on) {
        if (win1_y1 <= win1_y2)
            win1_y_in = (line >= win1_y1 && line < win1_y2);
        else
            win1_y_in = (line >= win1_y1 || line < win1_y2);
    }

    for (int x = 0; x < 240; x++) {
        u8 win_flags = 0x3F;

        if (any_window) {
            win_flags = winout & 0x3F;

            if (objwin_on && !obj_transparent_[x]) {
                win_flags = (winout >> 8) & 0x3F;
            }

            if (win1_on && win1_y_in) {
                bool x_in;
                if (win1_x1 <= win1_x2)
                    x_in = (x >= win1_x1 && x < win1_x2);
                else
                    x_in = (x >= win1_x1 || x < win1_x2);
                if (x_in) win_flags = (winin >> 8) & 0x3F;
            }

            if (win0_on && win0_y_in) {
                bool x_in;
                if (win0_x1 <= win0_x2)
                    x_in = (x >= win0_x1 && x < win0_x2);
                else
                    x_in = (x >= win0_x1 || x < win0_x2);
                if (x_in) win_flags = winin & 0x3F;
            }
        }

        bool win_blend = win_flags & (1 << 5);

        struct LayerEntry {
            int sort_key;
            u16 color;
            int layer;
        };
        LayerEntry layers[6];
        int layer_count = 0;

        for (int bg = 0; bg < 4; bg++) {
            if (!bg_enabled[bg]) continue;
            if (!(win_flags & (1 << bg))) continue;
            if (bg_transparent_[bg][x]) continue;
            layers[layer_count++] = {bg_priority_[bg][x] * 8 + bg, bg_line_[bg][x], bg};
        }

        bool obj_visible = obj_enabled && !obj_transparent_[x] && (win_flags & (1 << 4));
        if (obj_visible) {
            int obj_sort = obj_priority_[x] * 8 - 1;
            layers[layer_count++] = {obj_sort, obj_line_[x], 4};
        }

        for (int i = 1; i < layer_count; i++) {
            LayerEntry tmp = layers[i];
            int j = i - 1;
            while (j >= 0 && layers[j].sort_key > tmp.sort_key) {
                layers[j + 1] = layers[j];
                j--;
            }
            layers[j + 1] = tmp;
        }

        u16 top_color = (layer_count > 0) ? layers[0].color : backdrop;
        int top_layer = (layer_count > 0) ? layers[0].layer : 5;

        u16 second_color = backdrop;
        int second_layer = 5;
        if (layer_count > 1) {
            second_color = layers[1].color;
            second_layer = layers[1].layer;
        }

        u16 final_color = top_color;

        if (win_blend) {
            bool force_alpha = obj_visible && top_layer == 4 && obj_semi_transparent_[x];

            if (force_alpha || (blend_mode == 1 && (bldcnt & (1 << top_layer)))) {
                bool second_is_target = (bldcnt & (1 << (8 + second_layer))) != 0;
                if (second_is_target) {
                    int r1 = top_color & 0x1F;
                    int g1 = (top_color >> 5) & 0x1F;
                    int b1 = (top_color >> 10) & 0x1F;
                    int r2 = second_color & 0x1F;
                    int g2 = (second_color >> 5) & 0x1F;
                    int b2 = (second_color >> 10) & 0x1F;
                    int r = std::min(31, (r1 * eva + r2 * evb) / 16);
                    int g = std::min(31, (g1 * eva + g2 * evb) / 16);
                    int b = std::min(31, (b1 * eva + b2 * evb) / 16);
                    final_color = r | (g << 5) | (b << 10);
                }
            } else if (blend_mode == 2 && (bldcnt & (1 << top_layer))) {
                int r = final_color & 0x1F;
                int g = (final_color >> 5) & 0x1F;
                int b = (final_color >> 10) & 0x1F;
                r += (31 - r) * evy / 16;
                g += (31 - g) * evy / 16;
                b += (31 - b) * evy / 16;
                final_color = r | (g << 5) | (b << 10);
            } else if (blend_mode == 3 && (bldcnt & (1 << top_layer))) {
                int r = final_color & 0x1F;
                int g = (final_color >> 5) & 0x1F;
                int b = (final_color >> 10) & 0x1F;
                r -= r * evy / 16;
                g -= g * evy / 16;
                b -= b * evy / 16;
                final_color = r | (g << 5) | (b << 10);
            }
        }

        framebuffer_[line * 240 + x] = rgb555_to_rgb888(final_color);
    }
}

void PPU::render_scanline(int line) {
    if (line >= 160) return;

    u16 dispcnt;
    memcpy(&dispcnt, &bus_->io[0], 2);

    bool forced_blank = dispcnt & (1 << 7);
    if (forced_blank) {
        for (int x = 0; x < 240; x++) {
            framebuffer_[line * 240 + x] = 0xFFFFFFFF;
        }
        return;
    }

    int mode = dispcnt & 7;

    for (int bg = 0; bg < 4; bg++) {
        bg_transparent_[bg].fill(true);
    }
    obj_transparent_.fill(true);

    switch (mode) {
        case 0:
            if (dispcnt & (1 << 8))  render_bg_text(0, line);
            if (dispcnt & (1 << 9))  render_bg_text(1, line);
            if (dispcnt & (1 << 10)) render_bg_text(2, line);
            if (dispcnt & (1 << 11)) render_bg_text(3, line);
            break;
        case 1:
            if (dispcnt & (1 << 8))  render_bg_text(0, line);
            if (dispcnt & (1 << 9))  render_bg_text(1, line);
            if (dispcnt & (1 << 10)) render_bg_affine(2, line);
            break;
        case 2:
            if (dispcnt & (1 << 10)) render_bg_affine(2, line);
            if (dispcnt & (1 << 11)) render_bg_affine(3, line);
            break;
        case 3:
            if (dispcnt & (1 << 10)) {
                for (int x = 0; x < 240; x++) {
                    u16 color = read_vram16(0x06000000 + (line * 240 + x) * 2);
                    bg_line_[2][x] = color;
                    bg_transparent_[2][x] = false;
                    bg_priority_[2][x] = 0;
                }
            }
            break;
        case 4:
        {
            bool frame = dispcnt & (1 << 4);
            u32 base = frame ? 0x0600A000 : 0x06000000;
            if (dispcnt & (1 << 10)) {
                for (int x = 0; x < 240; x++) {
                    u8 idx = read_vram8(base + line * 240 + x);
                    if (idx == 0) {
                        bg_transparent_[2][x] = true;
                    } else {
                        bg_line_[2][x] = read_palette(idx * 2);
                        bg_transparent_[2][x] = false;
                    }
                    bg_priority_[2][x] = 0;
                }
            }
            break;
        }
        case 5:
        {
            bool frame = dispcnt & (1 << 4);
            u32 base = frame ? 0x0600A000 : 0x06000000;
            if (dispcnt & (1 << 10)) {
                for (int x = 0; x < 240; x++) {
                    if (x < 160 && line < 128) {
                        u16 color = read_vram16(base + (line * 160 + x) * 2);
                        bg_line_[2][x] = color;
                        bg_transparent_[2][x] = false;
                    } else {
                        bg_transparent_[2][x] = true;
                    }
                    bg_priority_[2][x] = 0;
                }
            }
            break;
        }
        default:
            break;
    }

    if (dispcnt & (1 << 12)) {
        render_sprites(line);
    }

    composite_scanline(line);
}
