// PPU — scanline renderer, layer compositing
#pragma once
#include "types.hpp"
#include <array>
#include <cstdio>

class Bus;

class PPU {
public:
    PPU();
    void set_bus(Bus* bus) { bus_ = bus; }

    void render_scanline(int line);

    const u32* get_framebuffer() const { return framebuffer_.data(); }

    void set_vcount(u16 v);
    u16  get_vcount() const;
    void set_hblank(bool v);
    void set_vblank(bool v);
    void latch_bg2_ref();
    void latch_bg3_ref();
    bool vblank_irq_enabled() const;
    bool hblank_irq_enabled() const;
    bool vcount_irq_enabled() const;
    u8   vcount_target() const;

    void set_color_correction(bool on);
    bool color_correction() const { return color_correct_; }

    bool save_state(FILE* f) const;
    bool load_state(FILE* f);

private:
    Bus* bus_ = nullptr;
    std::array<u32, GBA_WIDTH * GBA_HEIGHT> framebuffer_{};

    u32 color_lut_[32768];
    bool color_correct_ = false;
    void build_lut();

    std::array<u16, 240> bg_line_[4];
    std::array<u8, 240>  bg_priority_[4];
    std::array<bool, 240> bg_transparent_[4];
    std::array<u16, 240> obj_line_;
    std::array<u8, 240>  obj_priority_;
    std::array<bool, 240> obj_transparent_;
    std::array<bool, 240> obj_semi_transparent_;

    void render_bg_text(int bg, int line);
    void render_bg_affine(int bg, int line);
    void render_sprites(int line);
    void composite_scanline(int line);

    s32 bg2_ref_x_ = 0, bg2_ref_y_ = 0;
    s32 bg3_ref_x_ = 0, bg3_ref_y_ = 0;

    u32 rgb555_to_rgb888(u16 color) const;

    u16 read_palette(u32 addr) const;
    u16 read_vram16(u32 addr) const;
    u8  read_vram8(u32 addr) const;
};
