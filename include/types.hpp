// common typedefs and bit helpers
#pragma once
#include <cstdint>
#include <cstddef>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

constexpr u32 bit(u32 val, int n) { return (val >> n) & 1; }
constexpr u32 bits(u32 val, int hi, int lo) { return (val >> lo) & ((1u << (hi - lo + 1)) - 1); }
constexpr bool test_bit(u32 val, int n) { return (val >> n) & 1; }
constexpr u32 set_bit(u32 val, int n) { return val | (1u << n); }
constexpr u32 clear_bit(u32 val, int n) { return val & ~(1u << n); }
constexpr u32 sign_extend(u32 val, int bits) {
    u32 mask = 1u << (bits - 1);
    return (val ^ mask) - mask;
}

constexpr u32 GBA_WIDTH  = 240;
constexpr u32 GBA_HEIGHT = 160;
constexpr u32 CPU_FREQ   = 16'777'216;

constexpr u32 CYCLES_PER_SCANLINE = 1232;
constexpr u32 VISIBLE_LINES = 160;
constexpr u32 VBLANK_LINES  = 68;
constexpr u32 TOTAL_LINES   = 228;
constexpr u32 CYCLES_PER_FRAME = CYCLES_PER_SCANLINE * TOTAL_LINES;
constexpr u32 HDRAW_CYCLES  = 960;
constexpr u32 HBLANK_CYCLES = 272;

constexpr u32 BIOS_SIZE    = 0x4000;
constexpr u32 EWRAM_SIZE   = 0x40000;
constexpr u32 IWRAM_SIZE   = 0x8000;
constexpr u32 PALETTE_SIZE = 0x400;
constexpr u32 VRAM_SIZE    = 0x18000;
constexpr u32 OAM_SIZE     = 0x400;
constexpr u32 IO_SIZE      = 0x400;
constexpr u32 MAX_ROM_SIZE = 0x2000000;
constexpr u32 FLASH_SIZE   = 0x20000;
