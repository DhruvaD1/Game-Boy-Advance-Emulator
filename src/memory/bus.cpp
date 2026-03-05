// memory bus read/write, IO register handling, ROM loading
#include "memory/bus.hpp"
#include "ppu/ppu.hpp"
#include "apu/apu.hpp"
#include "dma/dma.hpp"
#include "timer/timer.hpp"
#include "interrupt/interrupt.hpp"
#include "input/input.hpp"
#include "memory/flash.hpp"
#include "rtc/rtc.hpp"
#include <fstream>
#include <cstring>
#include <cstdio>

Bus::Bus() {
    install_bios_stub();
}
Bus::~Bus() {}

void Bus::install_bios_stub() {
    bios.fill(0);

    auto write32 = [&](u32 addr, u32 val) {
        memcpy(&bios[addr], &val, 4);
    };

    write32(0x00, 0xEAFFFFFE);

    write32(0x04, 0xEAFFFFFE);

    write32(0x08, 0xEA00004C);

    write32(0x0C, 0xEAFFFFFE);

    write32(0x10, 0xEAFFFFFE);

    write32(0x14, 0x00000000);

    write32(0x18, 0xEA000042);

    write32(0x1C, 0xEAFFFFFE);

    u32 irq_base = 0x128;

    write32(irq_base + 0x00, 0xE92D500F);

    write32(irq_base + 0x04, 0xE3A00301);

    write32(irq_base + 0x08, 0xE28FE000);

    write32(irq_base + 0x0C, 0xE510F004);

    write32(irq_base + 0x10, 0xE3A00301);

    write32(irq_base + 0x14, 0xE5901200);

    write32(irq_base + 0x14, 0xE5B01200);

    write32(irq_base + 0x18, 0xE5902000);

    irq_base = 0x128;

    write32(0x128, 0xE92D500F);

    write32(0x12C, 0xE3A00301);

    write32(0x130, 0xE28FE000);

    write32(0x134, 0xE510F004);

    write32(0x138, 0xE3A00301);

    write32(0x13C, 0xE5901200);

    write32(0x140, 0xE0011821);

    write32(0x144, 0xE3A020C3);

    write32(0x144, 0xE3A02203);

    write32(0x144, 0xE3A02403);

    write32(0x148, 0xE2822A07);

    write32(0x14C, 0xE2822CF9);

    write32(0x128, 0xE92D500F);

    write32(0x12C, 0xE3A00301);

    write32(0x130, 0xE28FE000);

    write32(0x134, 0xE510F004);

    write32(0x138, 0xE8BD500F);

    write32(0x13C, 0xE25EF004);

    write32(0x140, 0xE1B0F00E);

    printf("BIOS: Installed minimal stub (IRQ handler at 0x128, SWI at 0x140)\n");
}

bool Bus::load_rom(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open ROM: %s\n", path.c_str());
        return false;
    }

    rom_size = (u32)file.tellg();
    file.seekg(0);

    rom.resize(rom_size);
    file.read(reinterpret_cast<char*>(rom.data()), rom_size);

    printf("ROM loaded: %s (%u bytes)\n", path.c_str(), rom_size);

    char title[13] = {};
    memcpy(title, &rom[0xA0], 12);
    printf("Game: %s\n", title);

    char code[5] = {};
    memcpy(code, &rom[0xAC], 4);
    printf("Code: %s\n", code);

    detect_save_type();
    return true;
}

void Bus::detect_save_type() {

    for (u32 i = 0; i + 16 < rom_size; i++) {
        if (memcmp(&rom[i], "FLASH_V", 7) == 0 ||
            memcmp(&rom[i], "FLASH512_V", 10) == 0 ||
            memcmp(&rom[i], "FLASH1M_V", 9) == 0) {
            has_flash_ = true;
            printf("Save type: Flash\n");
            return;
        }
        if (memcmp(&rom[i], "SRAM_V", 6) == 0) {
            has_sram_ = true;
            printf("Save type: SRAM\n");
            return;
        }
    }
    printf("Save type: None detected (defaulting to Flash for Pokemon)\n");
    has_flash_ = true;
}

u8 Bus::read8(u32 addr) {
    u32 region = addr >> 24;
    switch (region) {
        case 0x00:
            if (addr < BIOS_SIZE) return bios[addr];
            return 0;
        case 0x02:
            return ewram[addr & 0x3FFFF];
        case 0x03:
            return iwram[addr & 0x7FFF];
        case 0x04:
            return io_read8(addr);
        case 0x05:
            return palette[addr & 0x3FF];
        case 0x06: {
            u32 offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            return vram[offset];
        }
        case 0x07:
            return oam[addr & 0x3FF];
        case 0x08: case 0x09:
        case 0x0A: case 0x0B:
        case 0x0C: case 0x0D:
        {
            u32 offset = addr & 0x1FFFFFF;

            if (has_rtc && rtc_ && offset >= 0xC4 && offset <= 0xC8) {
                return rtc_->read(offset);
            }
            if (offset < rom_size) return rom[offset];
            return (offset >> 1) & 0xFF;
        }
        case 0x0E: case 0x0F:
            if (flash_) return flash_->read(addr & 0xFFFF);
            return 0xFF;
        default:
            return (u8)(last_read);
    }
}

u16 Bus::read16(u32 addr) {
    addr &= ~1u;
    u32 region = addr >> 24;
    switch (region) {
        case 0x00: { if (addr < BIOS_SIZE) { u16 v; memcpy(&v, &bios[addr], 2); return v; } return 0; }
        case 0x02: { u16 v; memcpy(&v, &ewram[addr & 0x3FFFF], 2); return v; }
        case 0x03: { u16 v; memcpy(&v, &iwram[addr & 0x7FFF], 2); return v; }
        case 0x04: return io_read16(addr);
        case 0x05: { u16 v; memcpy(&v, &palette[addr & 0x3FF], 2); return v; }
        case 0x06: {
            u32 offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            u16 v; memcpy(&v, &vram[offset], 2); return v;
        }
        case 0x07: { u16 v; memcpy(&v, &oam[addr & 0x3FF], 2); return v; }
        case 0x08: case 0x09:
        case 0x0A: case 0x0B:
        case 0x0C: case 0x0D: {
            u32 offset = addr & 0x1FFFFFF;
            if (has_rtc && rtc_ && offset >= 0xC4 && offset <= 0xC8) {
                return rtc_->read(offset);
            }
            if (offset + 1 < rom_size) { u16 v; memcpy(&v, &rom[offset], 2); return v; }
            return (offset >> 1) & 0xFFFF;
        }
        case 0x0E: case 0x0F:
            return flash_ ? flash_->read(addr & 0xFFFF) * 0x0101 : 0xFFFF;
        default: return (u16)last_read;
    }
}

u32 Bus::read32(u32 addr) {
    addr &= ~3u;
    u32 region = addr >> 24;
    switch (region) {
        case 0x00: { if (addr < BIOS_SIZE) { u32 v; memcpy(&v, &bios[addr], 4); return v; } return 0; }
        case 0x02: { u32 v; memcpy(&v, &ewram[addr & 0x3FFFF], 4); return v; }
        case 0x03: { u32 v; memcpy(&v, &iwram[addr & 0x7FFF], 4); return v; }
        case 0x04: return (u32)io_read16(addr) | ((u32)io_read16(addr + 2) << 16);
        case 0x05: { u32 v; memcpy(&v, &palette[addr & 0x3FF], 4); return v; }
        case 0x06: {
            u32 offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            u32 v; memcpy(&v, &vram[offset], 4); return v;
        }
        case 0x07: { u32 v; memcpy(&v, &oam[addr & 0x3FF], 4); return v; }
        case 0x08: case 0x09:
        case 0x0A: case 0x0B:
        case 0x0C: case 0x0D: {
            u32 offset = addr & 0x1FFFFFF;
            if (offset + 3 < rom_size) { u32 v; memcpy(&v, &rom[offset], 4); return v; }
            u16 lo = (offset >> 1) & 0xFFFF;
            u16 hi = ((offset + 2) >> 1) & 0xFFFF;
            return lo | (hi << 16);
        }
        case 0x0E: case 0x0F:
            return flash_ ? flash_->read(addr & 0xFFFF) * 0x01010101 : 0xFFFFFFFF;
        default: return last_read;
    }
}

void Bus::write8(u32 addr, u8 val) {
    u32 region = addr >> 24;
    switch (region) {
        case 0x02: ewram[addr & 0x3FFFF] = val; return;
        case 0x03: iwram[addr & 0x7FFF] = val; return;
        case 0x04: io_write8(addr, val); return;
        case 0x05:
        {
            u32 a = addr & 0x3FE;
            palette[a] = val;
            palette[a + 1] = val;
            return;
        }
        case 0x06:
        {
            u32 offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;

            if (offset < 0x10000) {
                offset &= ~1u;
                vram[offset] = val;
                vram[offset + 1] = val;
            }
            return;
        }
        case 0x07: return;
        case 0x0E: case 0x0F:
            if (flash_) flash_->write(addr & 0xFFFF, val);
            return;
        default: return;
    }
}

void Bus::write16(u32 addr, u16 val) {
    addr &= ~1u;
    u32 region = addr >> 24;
    switch (region) {
        case 0x02: memcpy(&ewram[addr & 0x3FFFF], &val, 2); return;
        case 0x03: memcpy(&iwram[addr & 0x7FFF], &val, 2); return;
        case 0x04: io_write16(addr, val); return;
        case 0x05: memcpy(&palette[addr & 0x3FF], &val, 2); return;
        case 0x06: {
            u32 offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            memcpy(&vram[offset], &val, 2);
            return;
        }
        case 0x07: memcpy(&oam[addr & 0x3FF], &val, 2); return;
        case 0x08: case 0x09: {
            u32 offset = addr & 0x1FFFFFF;
            if (has_rtc && rtc_ && offset >= 0xC4 && offset <= 0xC8) {
                rtc_->write(offset, (u8)val);
            }
            return;
        }
        case 0x0E: case 0x0F:
            if (flash_) {
                flash_->write(addr & 0xFFFF, (u8)val);
                flash_->write((addr & 0xFFFF) + 1, (u8)(val >> 8));
            }
            return;
        default: return;
    }
}

void Bus::write32(u32 addr, u32 val) {
    addr &= ~3u;
    u32 region = addr >> 24;
    switch (region) {
        case 0x02: memcpy(&ewram[addr & 0x3FFFF], &val, 4); return;
        case 0x03: memcpy(&iwram[addr & 0x7FFF], &val, 4); return;
        case 0x04:
            io_write16(addr, (u16)val);
            io_write16(addr + 2, (u16)(val >> 16));
            return;
        case 0x05: memcpy(&palette[addr & 0x3FF], &val, 4); return;
        case 0x06: {
            u32 offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            memcpy(&vram[offset], &val, 4);
            return;
        }
        case 0x07: memcpy(&oam[addr & 0x3FF], &val, 4); return;
        case 0x08: case 0x09: {
            u32 offset = addr & 0x1FFFFFF;
            if (has_rtc && rtc_ && offset >= 0xC4 && offset <= 0xC8) {
                rtc_->write(offset, (u8)val);
            }
            return;
        }
        default: return;
    }
}

u8* Bus::get_ptr(u32 addr) {
    u32 region = addr >> 24;
    switch (region) {
        case 0x02: return &ewram[addr & 0x3FFFF];
        case 0x03: return &iwram[addr & 0x7FFF];
        case 0x05: return &palette[addr & 0x3FF];
        case 0x06: {
            u32 offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            return &vram[offset];
        }
        case 0x07: return &oam[addr & 0x3FF];
        case 0x08: case 0x09:
        case 0x0A: case 0x0B:
        case 0x0C: case 0x0D:
            return &rom[addr & 0x1FFFFFF];
        default: return nullptr;
    }
}

u8 Bus::io_read8(u32 addr) {
    u16 val = io_read16(addr & ~1u);
    return (addr & 1) ? (val >> 8) : (val & 0xFF);
}

u16 Bus::io_read16(u32 addr) {
    u32 reg = addr & 0x3FF;

    switch (reg) {

        case 0x000:
        case 0x002:
        case 0x004:
        case 0x006:
        case 0x008: case 0x00A: case 0x00C: case 0x00E:
        case 0x048:
        case 0x04A:
        case 0x050:
        case 0x052:
        {
            u16 v; memcpy(&v, &io[reg], 2); return v;
        }

        case 0x130:
            return input_ ? input_->read_keyinput() : 0x03FF;
        case 0x132:
        {
            u16 v; memcpy(&v, &io[reg], 2); return v;
        }

        case 0x200:
        case 0x202:
        case 0x208:
        {
            u16 v; memcpy(&v, &io[reg], 2); return v;
        }

        case 0x100: case 0x102:
        case 0x104: case 0x106:
        case 0x108: case 0x10A:
        case 0x10C: case 0x10E:
            if (timer_) return timer_->read(reg);
            { u16 v; memcpy(&v, &io[reg], 2); return v; }

        case 0x0BA: case 0x0C6: case 0x0D2: case 0x0DE:
        {
            u16 v; memcpy(&v, &io[reg], 2); return v;
        }

        case 0x060: case 0x062: case 0x064: case 0x066:
        case 0x068: case 0x06A: case 0x06C: case 0x06E:
        case 0x070: case 0x072: case 0x074: case 0x076:
        case 0x078: case 0x07A: case 0x07C: case 0x07E:
        case 0x080: case 0x082: case 0x084: case 0x088:
        case 0x090: case 0x092: case 0x094: case 0x096:
        case 0x098: case 0x09A: case 0x09C: case 0x09E:
        {
            u16 v; memcpy(&v, &io[reg], 2); return v;
        }

        case 0x204:
        {
            u16 v; memcpy(&v, &io[reg], 2); return v;
        }

        case 0x300:
            return io[0x300];

        default:
        {
            if (reg < IO_SIZE - 1) {
                u16 v; memcpy(&v, &io[reg], 2); return v;
            }
            return 0;
        }
    }
}

bool Bus::save_state(FILE* f) const {
    if (fwrite(ewram.data(), sizeof(ewram), 1, f) != 1) return false;
    if (fwrite(iwram.data(), sizeof(iwram), 1, f) != 1) return false;
    if (fwrite(palette.data(), sizeof(palette), 1, f) != 1) return false;
    if (fwrite(vram.data(), sizeof(vram), 1, f) != 1) return false;
    if (fwrite(oam.data(), sizeof(oam), 1, f) != 1) return false;
    if (fwrite(io.data(), sizeof(io), 1, f) != 1) return false;
    if (fwrite(&last_read, sizeof(last_read), 1, f) != 1) return false;
    return true;
}

bool Bus::load_state(FILE* f) {
    if (fread(ewram.data(), sizeof(ewram), 1, f) != 1) return false;
    if (fread(iwram.data(), sizeof(iwram), 1, f) != 1) return false;
    if (fread(palette.data(), sizeof(palette), 1, f) != 1) return false;
    if (fread(vram.data(), sizeof(vram), 1, f) != 1) return false;
    if (fread(oam.data(), sizeof(oam), 1, f) != 1) return false;
    if (fread(io.data(), sizeof(io), 1, f) != 1) return false;
    if (fread(&last_read, sizeof(last_read), 1, f) != 1) return false;
    return true;
}

void Bus::io_write8(u32 addr, u8 val) {
    u32 reg = addr & 0x3FF;

    if (reg == 0x301) {
        io[reg] = val;
        return;
    }

    if (reg == 0x300) {
        io[reg] = val;
        return;
    }

    u16 old;
    if (reg < IO_SIZE - 1) {
        memcpy(&old, &io[reg & ~1u], 2);
    } else {
        old = 0;
    }

    u16 val16;
    if (addr & 1) {
        val16 = (old & 0x00FF) | ((u16)val << 8);
    } else {
        val16 = (old & 0xFF00) | val;
    }
    io_write16(addr & ~1u, val16);
}

void Bus::io_write16(u32 addr, u16 val) {
    u32 reg = addr & 0x3FF;

    if (reg == 0x004) {

        u16 old_dispstat;
        memcpy(&old_dispstat, &io[0x004], 2);
        u16 new_dispstat = (old_dispstat & 0x0007) | (val & 0xFFF8);
        memcpy(&io[0x004], &new_dispstat, 2);
        return;
    }

    if (reg != 0x202 && reg < IO_SIZE - 1) {
        memcpy(&io[reg], &val, 2);
    }

    switch (reg) {

        case 0x000:
        case 0x002:
        case 0x008: case 0x00A: case 0x00C: case 0x00E:
        case 0x010: case 0x012: case 0x014: case 0x016:
        case 0x018: case 0x01A: case 0x01C: case 0x01E:
        case 0x020: case 0x022: case 0x024: case 0x026:
        case 0x030: case 0x032: case 0x034: case 0x036:
        case 0x028: case 0x02A:
        case 0x02C: case 0x02E:
            if (reg >= 0x028 && reg <= 0x02E && ppu_)
                ppu_->latch_bg2_ref();
            break;
        case 0x038: case 0x03A:
        case 0x03C: case 0x03E:
            if (reg >= 0x038 && reg <= 0x03E && ppu_)
                ppu_->latch_bg3_ref();
        case 0x040: case 0x042: case 0x044: case 0x046:
        case 0x048: case 0x04A:
        case 0x04C:
        case 0x050: case 0x052: case 0x054:
            break;

        case 0x0B0: case 0x0B2:
        case 0x0B4: case 0x0B6:
        case 0x0B8:
        case 0x0BA:
        case 0x0BC: case 0x0BE:
        case 0x0C0: case 0x0C2:
        case 0x0C4:
        case 0x0C6:
        case 0x0C8: case 0x0CA:
        case 0x0CC: case 0x0CE:
        case 0x0D0:
        case 0x0D2:
        case 0x0D4: case 0x0D6:
        case 0x0D8: case 0x0DA:
        case 0x0DC:
        case 0x0DE:
            if (dma_) dma_->write_register(reg, val);
            break;

        case 0x100: case 0x102:
        case 0x104: case 0x106:
        case 0x108: case 0x10A:
        case 0x10C: case 0x10E:
            if (timer_) timer_->write(reg, val);
            break;

        case 0x200:
            break;
        case 0x202:
        {
            u16 old_if; memcpy(&old_if, &io[0x202], 2);
            u16 new_if = old_if & ~val;
            memcpy(&io[0x202], &new_if, 2);
            break;
        }
        case 0x208:
            break;

        case 0x060: case 0x062: case 0x064: case 0x066:
        case 0x068: case 0x06A: case 0x06C: case 0x06E:
        case 0x070: case 0x072: case 0x074: case 0x076:
        case 0x078: case 0x07A: case 0x07C: case 0x07E:
        case 0x080: case 0x082: case 0x084:
        case 0x0A0: case 0x0A2: case 0x0A4: case 0x0A6:
            if (apu_) apu_->write_register(reg, val);
            break;

        case 0x132:
            break;

        case 0x204:
            break;

        default:
            break;
    }
}
