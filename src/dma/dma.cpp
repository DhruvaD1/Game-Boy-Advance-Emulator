// DMA transfers — immediate, HBlank, VBlank, special timing
#include "dma/dma.hpp"
#include "memory/bus.hpp"
#include "interrupt/interrupt.hpp"
#include <cstring>

void DMA::write_register(u32 reg, u16 val) {

    int ch = -1;
    u32 base_reg;

    if (reg >= 0x0B0 && reg < 0x0BC) { ch = 0; base_reg = 0x0B0; }
    else if (reg >= 0x0BC && reg < 0x0C8) { ch = 1; base_reg = 0x0BC; }
    else if (reg >= 0x0C8 && reg < 0x0D4) { ch = 2; base_reg = 0x0C8; }
    else if (reg >= 0x0D4 && reg < 0x0E0) { ch = 3; base_reg = 0x0D4; }
    else return;

    u32 offset = reg - base_reg;
    auto& c = channels_[ch];

    memcpy(&bus_->io[reg], &val, 2);

    switch (offset) {
        case 0x0:
            c.src = (c.src & 0xFFFF0000) | val;
            break;
        case 0x2:
            c.src = (c.src & 0x0000FFFF) | ((u32)val << 16);
            if (ch < 3) c.src &= 0x07FFFFFF;
            else c.src &= 0x0FFFFFFF;
            break;
        case 0x4:
            c.dst = (c.dst & 0xFFFF0000) | val;
            break;
        case 0x6:
            c.dst = (c.dst & 0x0000FFFF) | ((u32)val << 16);
            if (ch < 3) c.dst &= 0x07FFFFFF;
            else c.dst &= 0x0FFFFFFF;
            break;
        case 0x8:
            c.count = val;
            if (ch == 3) c.count &= 0xFFFF;
            else c.count &= 0x3FFF;
            break;
        case 0xA: {
            bool was_enabled = c.enabled;
            c.control = val;
            c.enabled = val & (1 << 15);

            if (!was_enabled && c.enabled) {

                c.internal_src = c.src;
                c.internal_dst = c.dst;
                c.internal_count = c.count;
                if (c.internal_count == 0) {
                    c.internal_count = (ch == 3) ? 0x10000 : 0x4000;
                }

                int timing = (val >> 12) & 3;
                if (timing == 0) {
                    run_channel(ch);
                }
            }
            break;
        }
    }
}

void DMA::trigger(DmaTiming timing) {
    for (int ch = 0; ch < 4; ch++) {
        auto& c = channels_[ch];
        if (!c.enabled) continue;

        int ch_timing = (c.control >> 12) & 3;
        if (static_cast<DmaTiming>(ch_timing) == timing) {
            run_channel(ch);
        }
    }
}

void DMA::trigger_fifo(int fifo_id) {

    int ch = fifo_id + 1;
    auto& c = channels_[ch];
    if (!c.enabled) return;
    int timing = (c.control >> 12) & 3;
    if (timing != 3) return;

    active_ = true;

    int src_mode = (c.control >> 7) & 3;
    int src_step;
    switch (src_mode) {
        case 0: case 3: src_step = 4; break;
        case 1: src_step = -4; break;
        case 2: src_step = 0; break;
        default: src_step = 4; break;
    }

    u32 src = c.internal_src;
    u32 dst = c.internal_dst;


    for (int i = 0; i < 4; i++) {
        u32 val = bus_->read32(src & ~3u);
        bus_->write32(dst & ~3u, val);
        src += src_step;
    }

    c.internal_src = src;

    active_ = false;
}

void DMA::check_immediate() {
    for (int ch = 0; ch < 4; ch++) {
        auto& c = channels_[ch];
        if (!c.enabled) continue;
        int timing = (c.control >> 12) & 3;
        if (timing == 0) run_channel(ch);
    }
}

void DMA::run_channel(int ch) {
    auto& c = channels_[ch];
    active_ = true;

    bool word_transfer = c.control & (1 << 10);
    int dst_mode = (c.control >> 5) & 3;
    int src_mode = (c.control >> 7) & 3;

    int step = word_transfer ? 4 : 2;
    int src_step = 0, dst_step = 0;

    switch (src_mode) {
        case 0: src_step = step; break;
        case 1: src_step = -step; break;
        case 2: src_step = 0; break;
        case 3: src_step = step; break;
    }

    switch (dst_mode) {
        case 0: case 3: dst_step = step; break;
        case 1: dst_step = -step; break;
        case 2: dst_step = 0; break;
    }

    u32 src = c.internal_src;
    u32 dst = c.internal_dst;

    for (u32 i = 0; i < c.internal_count; i++) {
        if (word_transfer) {
            u32 val = bus_->read32(src & ~3u);
            bus_->write32(dst & ~3u, val);
        } else {
            u16 val = bus_->read16(src & ~1u);
            bus_->write16(dst & ~1u, val);
        }
        src += src_step;
        dst += dst_step;
    }

    c.internal_src = src;
    c.internal_dst = dst;

    if (dst_mode == 3) {
        c.internal_dst = c.dst;
    }

    if (c.control & (1 << 14)) {
        if (interrupt_) {
            interrupt_->request_irq(IRQ_DMA0 << ch);
        }
    }

    if (c.control & (1 << 9)) {

        c.internal_count = c.count;
        if (c.internal_count == 0) {
            c.internal_count = (ch == 3) ? 0x10000 : 0x4000;
        }
    } else {
        c.enabled = false;
        c.control &= ~(1 << 15);
        memcpy(&bus_->io[0x0BA + ch * 12], &c.control, 2);
    }

    active_ = false;
}
