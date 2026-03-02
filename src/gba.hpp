// top-level GBA system, owns all components
#pragma once
#include "types.hpp"
#include "cpu/arm7tdmi.hpp"
#include "memory/bus.hpp"
#include "ppu/ppu.hpp"
#include "apu/apu.hpp"
#include "dma/dma.hpp"
#include "timer/timer.hpp"
#include "interrupt/interrupt.hpp"
#include "input/input.hpp"
#include "bios/hle_bios.hpp"
#include "memory/flash.hpp"
#include "rtc/rtc.hpp"
#include <string>

class GBA {
public:
    GBA();

    bool load_rom(const std::string& path);
    void run_frame();

    const u32* get_framebuffer() const { return ppu_.get_framebuffer(); }

    void update_input(const u8* keyboard_state) { input_.update(keyboard_state); }

    void save_game();

private:
    ARM7TDMI cpu_;
    Bus bus_;
    PPU ppu_;
    APU apu_;
    DMA dma_;
    Timer timer_;
    InterruptController interrupt_;
    Input input_;
    HleBios bios_;
    Flash flash_;
    RTC rtc_;

    std::string save_path_;

    void run_scanline(int line);
    void handle_hle_swi();
public:
    void dump_ppu_state() const;
    const Bus& bus() const { return bus_; }
};
