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
#include "cheat/cheat.hpp"
#include <string>

class GBA {
public:
    GBA();

    bool load_rom(const std::string& path);
    bool reset_and_load_rom(const std::string& path);
    void run_frame();

    const u32* get_framebuffer() const { return ppu_.get_framebuffer(); }

    void update_input(const u8* keyboard_state) { input_.update(keyboard_state); }

    Input& input() { return input_; }

    void set_color_correction(bool on) { ppu_.set_color_correction(on); }
    bool color_correction() const { return ppu_.color_correction(); }

    bool load_bios(const std::string& path) { return bus_.load_bios(path); }
    bool has_real_bios() const { return bus_.has_real_bios(); }
    void set_bios_intro(bool on) { bios_intro_ = on; }
    bool bios_intro() const { return bios_intro_; }
    const std::string& rom_path() const { return rom_path_; }

    CheatEngine& cheat_engine() { return cheat_; }
    const std::string& cheats_path() const { return cheats_path_; }
    void save_cheats();

    void save_game();

    bool save_state(int slot);
    bool load_state(int slot);

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
    CheatEngine cheat_;

    std::string rom_path_;
    std::string save_path_;
    std::string cheats_path_;
    bool bios_intro_ = true;
    bool boot_anim_active_ = false;
    int boot_anim_frame_ = 0;
    int frame_num_ = 0;

    void run_scanline(int line);
    void handle_hle_swi();
    void render_boot_frame();
    std::string state_path(int slot) const;
public:
    void dump_ppu_state() const;
    const Bus& bus() const { return bus_; }
};
