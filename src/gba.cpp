// GBA system — wires components, runs scanlines per frame
#include "gba.hpp"
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <libgen.h>

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
    apu_.set_dma(&dma_);
    dma_.set_bus(&bus_);
    dma_.set_interrupt(&interrupt_);
    timer_.set_bus(&bus_);
    timer_.set_interrupt(&interrupt_);
    timer_.set_apu(&apu_);
    interrupt_.set_bus(&bus_);
    bios_.set_cpu(&cpu_);
    bios_.set_bus(&bus_);

    bus_.has_rtc = true;
    cheat_.set_bus(&bus_);
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

    std::string path_copy = path;
    char* dir = dirname(&path_copy[0]);
    std::string rom_dir = dir;

    std::string rom_name = path;
    auto slash = rom_name.rfind('/');
    if (slash != std::string::npos) rom_name = rom_name.substr(slash + 1);
    auto dot2 = rom_name.rfind('.');
    if (dot2 != std::string::npos) rom_name = rom_name.substr(0, dot2);

    cheats_path_ = rom_dir + "/cheats/" + rom_name + ".txt";
    cheat_.load(cheats_path_);

    printf("Save file: %s\n", save_path_.c_str());
    printf("Cheats file: %s\n", cheats_path_.c_str());
    printf("Controls: Arrows=D-pad, X=A, Z=B, Enter=Start, RShift=Select, A=L, S=R\n");
    return true;
}

bool GBA::reset_and_load_rom(const std::string& path) {
    flash_.save(save_path_);

    apu_.shutdown_audio();

    bus_.ewram.fill(0);
    bus_.iwram.fill(0);
    bus_.palette.fill(0);
    bus_.vram.fill(0);
    bus_.oam.fill(0);
    bus_.io.fill(0);
    bus_.install_bios_stub();
    bus_.last_read = 0;

    ppu_ = PPU();
    ppu_.set_bus(&bus_);
    bus_.set_ppu(&ppu_);

    apu_ = APU();
    apu_.set_bus(&bus_);
    apu_.set_dma(&dma_);
    bus_.set_apu(&apu_);

    dma_ = DMA();
    dma_.set_bus(&bus_);
    dma_.set_interrupt(&interrupt_);
    bus_.set_dma(&dma_);

    timer_ = Timer();
    timer_.set_bus(&bus_);
    timer_.set_interrupt(&interrupt_);
    timer_.set_apu(&apu_);
    bus_.set_timer(&timer_);

    interrupt_ = InterruptController();
    interrupt_.set_bus(&bus_);
    bus_.set_interrupt(&interrupt_);

    flash_ = Flash();
    bus_.set_flash(&flash_);

    rtc_ = RTC();
    bus_.set_rtc(&rtc_);
    bus_.has_rtc = true;

    cheat_ = CheatEngine();
    cheat_.set_bus(&bus_);

    frame_num_ = 0;

    return load_rom(path);
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

    frame_num_++;
    if (frame_num_ == 1) {
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
    cheat_.apply();

    if (ppu_.vblank_irq_enabled()) {
        interrupt_.request_irq(IRQ_VBLANK);
    }

    dma_.trigger(DmaTiming::VBlank);

    for (int line = (int)VISIBLE_LINES; line < (int)TOTAL_LINES; line++) {
        run_scanline(line);
    }
}

std::string GBA::state_path(int slot) const {
    std::string path = save_path_;
    auto dot = path.rfind('.');
    if (dot != std::string::npos) path = path.substr(0, dot);
    path += ".ss" + std::to_string(slot);
    return path;
}

bool GBA::save_state(int slot) {
    std::string path = state_path(slot);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;


    const char magic[4] = {'G', 'B', 'A', 'S'};
    u32 version = 1;
    u8 reserved[8] = {};
    fwrite(magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(reserved, 8, 1, f);

    bool ok = true;
    ok = ok && cpu_.save_state(f);
    ok = ok && bus_.save_state(f);
    ok = ok && ppu_.save_state(f);
    ok = ok && apu_.save_state(f);
    ok = ok && dma_.save_state(f);
    ok = ok && timer_.save_state(f);
    ok = ok && flash_.save_state(f);
    ok = ok && rtc_.save_state(f);
    ok = ok && (fwrite(&frame_num_, sizeof(frame_num_), 1, f) == 1);

    fclose(f);
    if (!ok) {
        remove(path.c_str());
        printf("Failed to save state to slot %d\n", slot);
        return false;
    }
    printf("State saved to slot %d\n", slot);
    return true;
}

bool GBA::load_state(int slot) {
    std::string path = state_path(slot);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        printf("No save state in slot %d\n", slot);
        return false;
    }


    char magic[4];
    u32 version;
    u8 reserved[8];
    if (fread(magic, 4, 1, f) != 1 || fread(&version, 4, 1, f) != 1 || fread(reserved, 8, 1, f) != 1) {
        fclose(f);
        return false;
    }
    if (magic[0] != 'G' || magic[1] != 'B' || magic[2] != 'A' || magic[3] != 'S' || version != 1) {
        fclose(f);
        printf("Invalid save state file in slot %d\n", slot);
        return false;
    }

    bool ok = true;
    ok = ok && cpu_.load_state(f);
    ok = ok && bus_.load_state(f);
    ok = ok && ppu_.load_state(f);
    ok = ok && apu_.load_state(f);
    ok = ok && dma_.load_state(f);
    ok = ok && timer_.load_state(f);
    ok = ok && flash_.load_state(f);
    ok = ok && rtc_.load_state(f);
    ok = ok && (fread(&frame_num_, sizeof(frame_num_), 1, f) == 1);

    fclose(f);
    if (!ok) {
        printf("Failed to load state from slot %d\n", slot);
        return false;
    }
    printf("State loaded from slot %d\n", slot);
    return true;
}

void GBA::save_game() {
    flash_.save(save_path_);
    printf("Game saved to %s\n", save_path_.c_str());
}

void GBA::save_cheats() {
    if (!cheat_.dirty()) return;
    std::string dir = cheats_path_;
    auto slash = dir.rfind('/');
    if (slash != std::string::npos) {
        dir = dir.substr(0, slash);
        mkdir(dir.c_str(), 0755);
    }
    cheat_.save(cheats_path_);
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
