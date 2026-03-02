// memory bus — address decoding, IO dispatch
#pragma once
#include "types.hpp"
#include <array>
#include <vector>
#include <string>

class PPU;
class APU;
class DMA;
class Timer;
class InterruptController;
class Input;
class Flash;
class RTC;

class Bus {
public:
    Bus();
    ~Bus();

    bool load_rom(const std::string& path);

    u8  read8(u32 addr);
    u16 read16(u32 addr);
    u32 read32(u32 addr);
    void write8(u32 addr, u8 val);
    void write16(u32 addr, u16 val);
    void write32(u32 addr, u32 val);

    u8*  get_ptr(u32 addr);

    void set_ppu(PPU* ppu) { ppu_ = ppu; }
    void set_apu(APU* apu) { apu_ = apu; }
    void set_dma(DMA* dma) { dma_ = dma; }
    void set_timer(Timer* timer) { timer_ = timer; }
    void set_interrupt(InterruptController* ic) { interrupt_ = ic; }
    void set_input(Input* input) { input_ = input; }
    void set_flash(Flash* flash) { flash_ = flash; }
    void set_rtc(RTC* rtc) { rtc_ = rtc; }

    u8  io_read8(u32 addr);
    u16 io_read16(u32 addr);
    void io_write8(u32 addr, u8 val);
    void io_write16(u32 addr, u16 val);

    std::array<u8, IO_SIZE> io{};

    std::array<u8, BIOS_SIZE>    bios{};
    std::array<u8, EWRAM_SIZE>   ewram{};
    std::array<u8, IWRAM_SIZE>   iwram{};
    std::array<u8, PALETTE_SIZE> palette{};
    std::array<u8, VRAM_SIZE>    vram{};
    std::array<u8, OAM_SIZE>     oam{};

    void install_bios_stub();

    std::vector<u8> rom;
    u32 rom_size = 0;

    u32 last_read = 0;

    bool has_rtc = false;

private:
    PPU* ppu_ = nullptr;
    APU* apu_ = nullptr;
    DMA* dma_ = nullptr;
    Timer* timer_ = nullptr;
    InterruptController* interrupt_ = nullptr;
    Input* input_ = nullptr;
    Flash* flash_ = nullptr;
    RTC* rtc_ = nullptr;

    void detect_save_type();
    bool has_flash_ = false;
    bool has_sram_ = false;
};
