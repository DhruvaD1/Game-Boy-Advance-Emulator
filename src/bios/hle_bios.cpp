// HLE SWI implementations — decompression, math, memory copy
#include "bios/hle_bios.hpp"
#include "cpu/arm7tdmi.hpp"
#include "memory/bus.hpp"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <vector>

void HleBios::handle_swi(u8 number) {
    switch (number) {
        case 0x00: swi_soft_reset(); break;
        case 0x01: swi_register_ram_reset(); break;
        case 0x02: swi_halt(); break;
        case 0x03:  cpu_->halted = true; break;
        case 0x04: swi_intr_wait(); break;
        case 0x05: swi_vblank_intr_wait(); break;
        case 0x06: swi_div(); break;
        case 0x07: swi_div_arm(); break;
        case 0x08: swi_sqrt(); break;
        case 0x09: swi_arctan(); break;
        case 0x0A: swi_arctan2(); break;
        case 0x0B: swi_cpu_set(); break;
        case 0x0C: swi_cpu_fast_set(); break;
        case 0x0E: swi_bg_affine_set(); break;
        case 0x0F: swi_obj_affine_set(); break;
        case 0x10: swi_bit_unpack(); break;
        case 0x11: swi_lz77_uncomp_wram(); break;
        case 0x12: swi_lz77_uncomp_vram(); break;
        case 0x13: swi_huff_uncomp(); break;
        case 0x14: swi_rl_uncomp_wram(); break;
        case 0x15: swi_rl_uncomp_vram(); break;
        case 0x16:  break;
        case 0x17:  break;
        case 0x18:  break;
        case 0x19: swi_sound_bias(); break;
        case 0x1F: swi_midi_key_to_freq(); break;
        case 0x25:  break;
        default:

            break;
    }
}

void HleBios::swi_soft_reset() {

    memset(&bus_->iwram[0x7E00], 0, 0x200);

    u8 flag = bus_->iwram[0x7FFA];
    cpu_->reg(13) = 0x03007F00;
    cpu_->reg(14) = 0;

    cpu_->pc() = flag ? 0x02000000 : 0x08000000;
    cpu_->set_cpsr((cpu_->cpsr() & ~0x3F) | static_cast<u32>(CpuMode::System));
    cpu_->flush_pipeline();
}

void HleBios::swi_register_ram_reset() {
    u8 flags = cpu_->reg(0) & 0xFF;

    bus_->io_write16(0x04000000, 0x0080);

    if (flags & 0x01) memset(bus_->ewram.data(), 0, EWRAM_SIZE);
    if (flags & 0x02) memset(bus_->iwram.data(), 0, 0x7E00);
    if (flags & 0x04) memset(bus_->palette.data(), 0, PALETTE_SIZE);
    if (flags & 0x08) memset(bus_->vram.data(), 0, VRAM_SIZE);
    if (flags & 0x10) memset(bus_->oam.data(), 0, OAM_SIZE);
    if (flags & 0x20) {

        memset(&bus_->io[0x120], 0, 0x12);
    }
    if (flags & 0x40) {

        memset(&bus_->io[0x60], 0, 0x30);
    }
    if (flags & 0x80) {

    }
}

void HleBios::swi_halt() {

    cpu_->set_cpsr(cpu_->cpsr() & ~(1 << CPSR_I));
    cpu_->halted = true;
}

void HleBios::swi_intr_wait() {

    u32 discard = cpu_->reg(0);
    u32 wait_flags = cpu_->reg(1);

    if (discard) {

        u16 check;
        memcpy(&check, &bus_->iwram[0x7FF8 - 0x0000], 2);
        check &= ~(u16)wait_flags;
        memcpy(&bus_->iwram[0x7FF8 - 0x0000], &check, 2);
    }

    cpu_->set_cpsr(cpu_->cpsr() & ~(1 << CPSR_I));

    cpu_->halted = true;
}

void HleBios::swi_vblank_intr_wait() {
    cpu_->reg(0) = 1;
    cpu_->reg(1) = 1;
    swi_intr_wait();
}

void HleBios::swi_div() {
    s32 num = (s32)cpu_->reg(0);
    s32 den = (s32)cpu_->reg(1);

    if (den == 0) {

        cpu_->reg(0) = (num >= 0) ? 1 : -1;
        cpu_->reg(1) = num;
        cpu_->reg(3) = (num >= 0) ? (u32)num : (u32)-num;
        return;
    }

    cpu_->reg(0) = (u32)(num / den);
    cpu_->reg(1) = (u32)(num % den);
    cpu_->reg(3) = (u32)(num >= 0 ? num / den : -(num / den));
}

void HleBios::swi_div_arm() {

    s32 den = (s32)cpu_->reg(0);
    s32 num = (s32)cpu_->reg(1);

    if (den == 0) {
        cpu_->reg(0) = (num >= 0) ? 1 : -1;
        cpu_->reg(1) = num;
        cpu_->reg(3) = (num >= 0) ? (u32)num : (u32)-num;
        return;
    }

    cpu_->reg(0) = (u32)(num / den);
    cpu_->reg(1) = (u32)(num % den);
    cpu_->reg(3) = (u32)(num >= 0 ? num / den : -(num / den));
}

void HleBios::swi_sqrt() {
    u32 val = cpu_->reg(0);
    cpu_->reg(0) = (u32)std::sqrt((double)val);
}

void HleBios::swi_arctan() {
    s16 tan = (s16)cpu_->reg(0);
    double t = tan / 16384.0;
    double a = std::atan(t);
    s16 result = (s16)(a * (32768.0 / M_PI));
    cpu_->reg(0) = (u32)(u16)result;
}

void HleBios::swi_arctan2() {
    s16 x = (s16)cpu_->reg(0);
    s16 y = (s16)cpu_->reg(1);
    double angle = std::atan2((double)y, (double)x);

    u16 result = (u16)(angle * (32768.0 / M_PI));
    cpu_->reg(0) = result;
}

void HleBios::swi_cpu_set() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);
    u32 cnt = cpu_->reg(2);

    u32 count = cnt & 0x1FFFFF;
    bool fill = cnt & (1 << 24);
    bool word = cnt & (1 << 26);

    if (word) {
        u32 val = bus_->read32(src & ~3u);
        for (u32 i = 0; i < count; i++) {
            bus_->write32(dst, val);
            dst += 4;
            if (!fill) { src += 4; val = bus_->read32(src & ~3u); }
        }
    } else {
        u16 val = bus_->read16(src & ~1u);
        for (u32 i = 0; i < count; i++) {
            bus_->write16(dst, val);
            dst += 2;
            if (!fill) { src += 2; val = bus_->read16(src & ~1u); }
        }
    }
}

void HleBios::swi_cpu_fast_set() {
    u32 src = cpu_->reg(0) & ~3u;
    u32 dst = cpu_->reg(1) & ~3u;
    u32 cnt = cpu_->reg(2);

    u32 count = cnt & 0x1FFFFF;
    bool fill = cnt & (1 << 24);

    count = (count + 7) & ~7u;

    u32 val = bus_->read32(src);
    for (u32 i = 0; i < count; i++) {
        bus_->write32(dst, val);
        dst += 4;
        if (!fill) { src += 4; val = bus_->read32(src); }
    }
}

void HleBios::swi_bg_affine_set() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);
    u32 count = cpu_->reg(2);

    for (u32 i = 0; i < count; i++) {
        s32 cx = (s32)bus_->read32(src);
        s32 cy = (s32)bus_->read32(src + 4);
        s16 dx = (s16)bus_->read16(src + 8);
        s16 dy = (s16)bus_->read16(src + 10);
        s16 sx = (s16)bus_->read16(src + 12);
        s16 sy = (s16)bus_->read16(src + 14);
        u16 angle = bus_->read16(src + 16);

        double a = angle * 2.0 * M_PI / 65536.0;
        double cos_a = std::cos(a);
        double sin_a = std::sin(a);

        s16 pa = (s16)(cos_a * 256.0 * 256.0 / sx);
        s16 pb = (s16)(sin_a * 256.0 * 256.0 / sx);
        s16 pc = (s16)(-sin_a * 256.0 * 256.0 / sy);
        s16 pd = (s16)(cos_a * 256.0 * 256.0 / sy);

        s32 start_x = cx - (pa * dx + pb * dy);
        s32 start_y = cy - (pc * dx + pd * dy);

        bus_->write16(dst + 0, (u16)pa);
        bus_->write16(dst + 2, (u16)pb);
        bus_->write16(dst + 4, (u16)pc);
        bus_->write16(dst + 6, (u16)pd);
        bus_->write32(dst + 8, (u32)start_x);
        bus_->write32(dst + 12, (u32)start_y);

        src += 20;
        dst += 16;
    }
}

void HleBios::swi_obj_affine_set() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);
    u32 count = cpu_->reg(2);
    u32 offset = cpu_->reg(3);

    for (u32 i = 0; i < count; i++) {
        s16 sx = (s16)bus_->read16(src);
        s16 sy = (s16)bus_->read16(src + 2);
        u16 angle = bus_->read16(src + 4);

        double a = angle * 2.0 * M_PI / 65536.0;
        double cos_a = std::cos(a);
        double sin_a = std::sin(a);

        s16 pa = (s16)(cos_a * 256.0 / (sx / 256.0));
        s16 pb = (s16)(sin_a * 256.0 / (sx / 256.0));
        s16 pc = (s16)(-sin_a * 256.0 / (sy / 256.0));
        s16 pd = (s16)(cos_a * 256.0 / (sy / 256.0));

        bus_->write16(dst + offset * 0, (u16)pa);
        bus_->write16(dst + offset * 1, (u16)pb);
        bus_->write16(dst + offset * 2, (u16)pc);
        bus_->write16(dst + offset * 3, (u16)pd);

        src += 8;
        dst += offset * 4;
    }
}

void HleBios::swi_bit_unpack() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);
    u32 info = cpu_->reg(2);

    u16 src_len = bus_->read16(info);
    u8 src_width = bus_->read8(info + 2);
    u8 dst_width = bus_->read8(info + 3);
    u32 data_offset = bus_->read32(info + 4);
    bool zero_flag = data_offset & (1u << 31);
    data_offset &= ~(1u << 31);

    u32 buffer = 0;
    int bits_written = 0;
    u8 src_byte = 0;
    int src_bits_left = 0;
    u32 src_mask = (1 << src_width) - 1;

    for (u16 i = 0; i < src_len; ) {
        if (src_bits_left == 0) {
            src_byte = bus_->read8(src + i);
            src_bits_left = 8;
            i++;
        }

        u32 val = src_byte & src_mask;
        src_byte >>= src_width;
        src_bits_left -= src_width;

        if (val != 0 || zero_flag) {
            val += (u32)data_offset;
        }

        buffer |= (val << bits_written);
        bits_written += dst_width;

        if (bits_written >= 32) {
            bus_->write32(dst, buffer);
            dst += 4;
            buffer = 0;
            bits_written = 0;
        }
    }

    if (bits_written > 0) {
        bus_->write32(dst, buffer);
    }
}

void HleBios::swi_lz77_uncomp_wram() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);

    u32 header = bus_->read32(src);
    u32 decomp_size = header >> 8;
    src += 4;

    u32 written = 0;
    while (written < decomp_size) {
        u8 flags = bus_->read8(src++);

        for (int i = 7; i >= 0 && written < decomp_size; i--) {
            if (flags & (1 << i)) {
                u8 byte1 = bus_->read8(src++);
                u8 byte2 = bus_->read8(src++);
                u32 length = ((byte1 >> 4) & 0xF) + 3;
                u32 offset = ((byte1 & 0xF) << 8) | byte2;
                offset++;

                for (u32 j = 0; j < length && written < decomp_size; j++) {
                    u8 val = bus_->read8(dst - offset + j);
                    bus_->write8(dst, val);
                    dst++;
                    written++;
                }
            } else {
                bus_->write8(dst, bus_->read8(src++));
                dst++;
                written++;
            }
        }
    }
}

void HleBios::swi_lz77_uncomp_vram() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);

    u32 header = bus_->read32(src);
    u32 decomp_size = header >> 8;
    src += 4;

    std::vector<u8> buffer(decomp_size);
    u32 written = 0;
    while (written < decomp_size) {
        u8 flags = bus_->read8(src++);

        for (int i = 7; i >= 0 && written < decomp_size; i--) {
            if (flags & (1 << i)) {
                u8 byte1 = bus_->read8(src++);
                u8 byte2 = bus_->read8(src++);
                u32 length = ((byte1 >> 4) & 0xF) + 3;
                u32 offset = ((byte1 & 0xF) << 8) | byte2;
                offset++;

                for (u32 j = 0; j < length && written < decomp_size; j++) {
                    buffer[written] = buffer[written - offset];
                    written++;
                }
            } else {
                buffer[written] = bus_->read8(src++);
                written++;
            }
        }
    }

    for (u32 i = 0; i + 1 < decomp_size; i += 2) {
        u16 val = buffer[i] | (buffer[i + 1] << 8);
        bus_->write16(dst + i, val);
    }

    if (decomp_size & 1) {
        bus_->write16(dst + decomp_size - 1, buffer[decomp_size - 1]);
    }
}

void HleBios::swi_huff_uncomp() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);

    u32 header = bus_->read32(src);
    u8 bit_size = header & 0xF;
    u32 decomp_size = header >> 8;
    src += 4;

    u32 tree_size = (bus_->read8(src) + 1) * 2;
    u32 tree_start = src + 1;
    src = tree_start + tree_size - 1;

    u32 written = 0;
    u32 buffer = 0;
    int buffer_bits = 0;
    u32 data = 0;
    int bits_left = 0;

    while (written < decomp_size) {

        if (bits_left == 0) {
            data = bus_->read32(src);
            src += 4;
            bits_left = 32;
        }

        u32 node_offset = tree_start;

        while (true) {
            if (bits_left == 0) {
                data = bus_->read32(src);
                src += 4;
                bits_left = 32;
            }

            u8 node = bus_->read8(node_offset);

            bool right = (data >> 31) & 1;
            data <<= 1;
            bits_left--;

            u32 child_offset;
            if (right) {
                child_offset = (node_offset & ~1u) + (node & 0x3F) * 2 + 4;
            } else {
                child_offset = (node_offset & ~1u) + (node & 0x3F) * 2 + 2;
            }

            bool is_leaf = right ? (node & 0x80) : (node & 0x40);
            if (is_leaf) {
                u8 val = bus_->read8(child_offset);
                buffer |= val << buffer_bits;
                buffer_bits += bit_size;
                if (buffer_bits >= 32) {
                    bus_->write32(dst, buffer);
                    dst += 4;
                    written += 4;
                    buffer = 0;
                    buffer_bits = 0;
                }
                break;
            } else {
                node_offset = child_offset;
            }
        }
    }
}

void HleBios::swi_rl_uncomp_wram() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);

    u32 header = bus_->read32(src);
    u32 decomp_size = header >> 8;
    src += 4;

    u32 written = 0;
    while (written < decomp_size) {
        u8 flag = bus_->read8(src++);
        if (flag & 0x80) {
            u32 length = (flag & 0x7F) + 3;
            u8 val = bus_->read8(src++);
            for (u32 i = 0; i < length && written < decomp_size; i++) {
                bus_->write8(dst++, val);
                written++;
            }
        } else {
            u32 length = (flag & 0x7F) + 1;
            for (u32 i = 0; i < length && written < decomp_size; i++) {
                bus_->write8(dst++, bus_->read8(src++));
                written++;
            }
        }
    }
}

void HleBios::swi_rl_uncomp_vram() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);

    u32 header = bus_->read32(src);
    u32 decomp_size = header >> 8;
    src += 4;

    std::vector<u8> buffer(decomp_size);
    u32 written = 0;
    while (written < decomp_size) {
        u8 flag = bus_->read8(src++);
        if (flag & 0x80) {
            u32 length = (flag & 0x7F) + 3;
            u8 val = bus_->read8(src++);
            for (u32 i = 0; i < length && written < decomp_size; i++) {
                buffer[written++] = val;
            }
        } else {
            u32 length = (flag & 0x7F) + 1;
            for (u32 i = 0; i < length && written < decomp_size; i++) {
                buffer[written++] = bus_->read8(src++);
            }
        }
    }

    for (u32 i = 0; i + 1 < decomp_size; i += 2) {
        u16 val = buffer[i] | (buffer[i + 1] << 8);
        bus_->write16(dst + i, val);
    }
    if (decomp_size & 1) {
        bus_->write16(dst + decomp_size - 1, buffer[decomp_size - 1]);
    }
}

void HleBios::swi_diff_8bit_unfilter_vram() {
    u32 src = cpu_->reg(0);
    u32 dst = cpu_->reg(1);

    u32 header = bus_->read32(src);
    u32 size = header >> 8;
    src += 4;

    std::vector<u8> buffer(size);
    u8 prev = 0;
    for (u32 i = 0; i < size; i++) {
        u8 val = bus_->read8(src++) + prev;
        buffer[i] = val;
        prev = val;
    }

    for (u32 i = 0; i + 1 < size; i += 2) {
        u16 val = buffer[i] | (buffer[i + 1] << 8);
        bus_->write16(dst + i, val);
    }
    if (size & 1) {
        bus_->write16(dst + size - 1, buffer[size - 1]);
    }
}

void HleBios::swi_sound_bias() {

    bus_->write16(0x04000088, 0x0200);
}

void HleBios::swi_midi_key_to_freq() {

    u32 wave = cpu_->reg(0);
    u32 mk = cpu_->reg(1);
    u32 fp = cpu_->reg(2);

    u32 freq = bus_->read32(wave + 4);
    double result = freq * std::pow(2.0, ((double)mk - 180.0) / 12.0 + (double)fp / (12.0 * 64.0));
    cpu_->reg(0) = (u32)(result + 0.5);
}

void HleBios::swi_sound_driver_init() {

}
