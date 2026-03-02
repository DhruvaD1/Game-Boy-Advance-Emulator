// GBA system — wires components, runs scanlines per frame
#include "gba.hpp"
#include <cstring>
#include <cstdio>

GBA::GBA() {

    cpu_.set_bus(&bus_);
    bus_.set_ppu(&ppu_);
    bus_.set_apu(&apu_);
    bus_.set_dma(&dma_);
    bus_.set_timer(&timer_);
    bus_.set_interrupt(&interrupt_);
    bus_.set_input(&input_);
    bus_.set_flash(&flash_);
    bus_.set_rtc(&rtc_);

    ppu_.set_bus(&bus_);
    apu_.set_bus(&bus_);
    dma_.set_bus(&bus_);
    dma_.set_interrupt(&interrupt_);
    timer_.set_bus(&bus_);
    timer_.set_interrupt(&interrupt_);
    timer_.set_apu(&apu_);
    interrupt_.set_bus(&bus_);
    bios_.set_cpu(&cpu_);
    bios_.set_bus(&bus_);

    bus_.has_rtc = true;
}

bool GBA::load_rom(const std::string& path) {
    if (!bus_.load_rom(path)) return false;

    save_path_ = path;
    auto dot = save_path_.rfind('.');
    if (dot != std::string::npos) save_path_ = save_path_.substr(0, dot);
    save_path_ += ".sav";

    flash_.set_save_path(save_path_);
    flash_.load(save_path_);

    cpu_.reset();

    u16 vcount = 0x7E;
    memcpy(&bus_.io[0x006], &vcount, 2);

    bus_.io[0x300] = 1;

    apu_.init_audio();

    printf("Save file: %s\n", save_path_.c_str());
    printf("Controls: Arrows=D-pad, X=A, Z=B, Enter=Start, RShift=Select, A=L, S=R\n");
    return true;
}

void GBA::handle_hle_swi() {
    u32 spsr = cpu_.spsr();
    bool was_thumb = spsr & (1 << CPSR_T);

    u8 swi_num;
    if (was_thumb) {
        u32 swi_addr = cpu_.reg(14) - 2;
        u16 instr = bus_.read16(swi_addr);
        swi_num = instr & 0xFF;
    } else {
        u32 swi_addr = cpu_.reg(14) - 4;
        u32 instr = bus_.read32(swi_addr);
        swi_num = (instr >> 16) & 0xFF;
    }

    bios_.handle_swi(swi_num);

    cpu_.set_cpsr(cpu_.spsr());
    cpu_.pc() = cpu_.reg(14);
    cpu_.flush_pipeline();
}

void GBA::run_scanline(int line) {
    int cycles_target = CYCLES_PER_SCANLINE;
    int cycles_run = 0;

    ppu_.set_vcount(line);

    u16 dispstat;
    memcpy(&dispstat, &bus_.io[0x004], 2);
    if ((dispstat & (1 << 2)) && (dispstat & (1 << 5))) {
        interrupt_.request_irq(IRQ_VCOUNT);
    }

    ppu_.set_hblank(false);

    while (cycles_run < (int)HDRAW_CYCLES) {
        u32 cur_pc = cpu_.pc();
        if (cur_pc == 0x140) {
            handle_hle_swi();
        }

        if (interrupt_.irq_pending()) {
            cpu_.check_irq();
        }

        int cycles = cpu_.step();
        cycles_run += cycles;
        timer_.tick(cycles);
        apu_.tick(cycles);
    }

    ppu_.set_hblank(true);

    if (line < VISIBLE_LINES) {
        ppu_.render_scanline(line);
    }

    if (ppu_.hblank_irq_enabled()) {
        interrupt_.request_irq(IRQ_HBLANK);
    }

    if (line < VISIBLE_LINES) {
        dma_.trigger(DmaTiming::HBlank);
    }

    while (cycles_run < cycles_target) {
        if (cpu_.pc() == 0x140) {
            handle_hle_swi();
        }

        if (interrupt_.irq_pending()) {
            cpu_.check_irq();
        }

        int cycles = cpu_.step();
        cycles_run += cycles;
        timer_.tick(cycles);
        apu_.tick(cycles);
    }
}

void GBA::run_frame() {

    static int frame_num = 0;
    frame_num++;
    if (frame_num == 1) {
        ppu_.set_vblank(true);
        for (int line = 0x7E; line < (int)TOTAL_LINES; line++) {
            run_scanline(line);
        }
        return;
    }

    ppu_.set_vblank(false);

    ppu_.latch_bg2_ref();
    ppu_.latch_bg3_ref();
    for (int line = 0; line < (int)VISIBLE_LINES; line++) {
        run_scanline(line);
    }

    ppu_.set_vblank(true);

    if (ppu_.vblank_irq_enabled()) {
        interrupt_.request_irq(IRQ_VBLANK);
    }

    dma_.trigger(DmaTiming::VBlank);

    for (int line = (int)VISIBLE_LINES; line < (int)TOTAL_LINES; line++) {
        run_scanline(line);
    }
}

void GBA::save_game() {
    flash_.save(save_path_);
    printf("Game saved to %s\n", save_path_.c_str());
}

void GBA::dump_ppu_state() const {
    u16 dispcnt;
    memcpy(&dispcnt, &bus_.io[0], 2);
    printf("DISPCNT=0x%04X mode=%d BG0=%d BG1=%d BG2=%d BG3=%d OBJ=%d WIN0=%d WIN1=%d OBJWIN=%d\n",
           dispcnt, dispcnt & 7,
           (dispcnt >> 8) & 1, (dispcnt >> 9) & 1, (dispcnt >> 10) & 1, (dispcnt >> 11) & 1,
           (dispcnt >> 12) & 1, (dispcnt >> 13) & 1, (dispcnt >> 14) & 1, (dispcnt >> 15) & 1);

    for (int bg = 0; bg < 4; bg++) {
        u16 bgcnt, hofs, vofs;
        memcpy(&bgcnt, &bus_.io[0x008 + bg * 2], 2);
        memcpy(&hofs, &bus_.io[0x010 + bg * 4], 2);
        memcpy(&vofs, &bus_.io[0x012 + bg * 4], 2);
        printf("  BG%d: CNT=0x%04X tilebase=%d mapbase=%d 8bpp=%d size=%d pri=%d hofs=%d vofs=%d mosaic=%d\n",
               bg, bgcnt, (bgcnt >> 2) & 3, (bgcnt >> 8) & 0x1F,
               (bgcnt >> 7) & 1, (bgcnt >> 14) & 3, bgcnt & 3,
               hofs & 0x1FF, vofs & 0x1FF, (bgcnt >> 6) & 1);
    }

    u16 bldcnt, bldalpha, bldy;
    memcpy(&bldcnt, &bus_.io[0x050], 2);
    memcpy(&bldalpha, &bus_.io[0x052], 2);
    memcpy(&bldy, &bus_.io[0x054], 2);
    printf("  BLDCNT=0x%04X mode=%d 1st=%02X 2nd=%02X  BLDALPHA=0x%04X EVA=%d EVB=%d  BLDY=%d\n",
           bldcnt, (bldcnt >> 6) & 3, bldcnt & 0x3F, (bldcnt >> 8) & 0x3F,
           bldalpha, bldalpha & 0x1F, (bldalpha >> 8) & 0x1F, bldy & 0x1F);

    u16 win0h, win1h, win0v, win1v, winin, winout;
    memcpy(&win0h, &bus_.io[0x040], 2);
    memcpy(&win1h, &bus_.io[0x042], 2);
    memcpy(&win0v, &bus_.io[0x044], 2);
    memcpy(&win1v, &bus_.io[0x046], 2);
    memcpy(&winin, &bus_.io[0x048], 2);
    memcpy(&winout, &bus_.io[0x04A], 2);
    printf("  WIN0: X=%d-%d Y=%d-%d  WIN1: X=%d-%d Y=%d-%d\n",
           win0h >> 8, win0h & 0xFF, win0v >> 8, win0v & 0xFF,
           win1h >> 8, win1h & 0xFF, win1v >> 8, win1v & 0xFF);
    printf("  WININ=0x%04X (W0=%02X W1=%02X) WINOUT=0x%04X (OUT=%02X OBJ=%02X)\n",
           winin, winin & 0x3F, (winin >> 8) & 0x3F, winout, winout & 0x3F, (winout >> 8) & 0x3F);
}
