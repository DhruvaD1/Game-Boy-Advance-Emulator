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

    rom_path_ = path;

    save_path_ = path;
    auto dot = save_path_.rfind('.');
    if (dot != std::string::npos) save_path_ = save_path_.substr(0, dot);
    save_path_ += ".sav";

    flash_.set_save_path(save_path_);
    flash_.load(save_path_);

    cpu_.reset();

    if (bios_intro_ && bus_.has_real_bios()) {
        cpu_.pc() = 0x00000000;
        cpu_.set_cpsr(static_cast<u32>(CpuMode::Supervisor) | (1 << CPSR_I) | (1 << CPSR_F));
        cpu_.flush_pipeline();
        frame_num_ = 1;
    } else {
        u16 vcount = 0x7E;
        memcpy(&bus_.io[0x006], &vcount, 2);
        bus_.io[0x300] = 1;

        if (bios_intro_) {
            boot_anim_active_ = true;
            boot_anim_frame_ = 0;
        }
    }

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
    bus_.restore_bios();
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

    bool use_hle = !bus_.has_real_bios();

    while (cycles_run < (int)HDRAW_CYCLES) {
        if (use_hle && cpu_.pc() == 0x140) {
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
        if (use_hle && cpu_.pc() == 0x140) {
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
    if (boot_anim_active_) {
        render_boot_frame();
        return;
    }

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

static const uint8_t BOOT_FONT[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00},
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00},
    {0x18,0x7E,0xC0,0x7C,0x06,0xFC,0x18,0x00},
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00},
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00},
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00},
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00},
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00},
    {0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00},
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    {0x7C,0xC6,0x06,0x1C,0x30,0x66,0xFE,0x00},
    {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00},
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00},
    {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00},
    {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00},
    {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00},
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00},
    {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00},
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    {0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00},
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00},
    {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00},
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00},
    {0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00}, // A
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, // B
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, // C
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, // D
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, // E
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, // F
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00}, // G
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, // J
    {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, // K
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, // L
    {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00}, // M
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // N
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // O
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, // P
    {0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x06}, // Q
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, // R
    {0x7C,0xC6,0xE0,0x7C,0x0E,0xC6,0x7C,0x00}, // S
    {0x7E,0x7E,0x5A,0x18,0x18,0x18,0x3C,0x00}, // T
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // U
    {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00}, // V
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, // W
    {0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0x00}, // X
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, // Y
    {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, // Z
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // [
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00},
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ]
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, // a
    {0xE0,0x60,0x7C,0x66,0x66,0x66,0xDC,0x00}, // b
    {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00}, // c
    {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00}, // d
    {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00}, // e
    {0x1C,0x36,0x30,0x78,0x30,0x30,0x78,0x00}, // f
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, // g
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, // h
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // i
    {0x06,0x00,0x06,0x06,0x06,0x66,0x66,0x3C}, // j
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, // k
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // l
    {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0x00}, // m
    {0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00}, // n
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00}, // o
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, // p
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, // q
    {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00}, // r
    {0x00,0x00,0x7C,0xC0,0x7C,0x06,0xFC,0x00}, // s
    {0x30,0x30,0x7C,0x30,0x30,0x36,0x1C,0x00}, // t
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, // u
    {0x00,0x00,0xC6,0xC6,0x6C,0x38,0x10,0x00}, // v
    {0x00,0x00,0xC6,0xD6,0xD6,0xFE,0x6C,0x00}, // w
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // x
    {0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0xFC}, // y
    {0x00,0x00,0xFE,0x8C,0x18,0x32,0xFE,0x00}, // z
};

static u32 make_color(u8 r, u8 g, u8 b) {
    return r | ((u32)g << 8) | ((u32)b << 16) | 0xFF000000u;
}

static void boot_draw_char(u32* fb, char ch, int x, int y, int scale, u32 color, int alpha) {
    if (ch < 32 || ch > 'z') return;
    int idx = ch - 32;
    if (idx >= 95) return;
    for (int row = 0; row < 8; row++) {
        u8 bits = BOOT_FONT[idx][row];
        for (int col = 0; col < 8; col++) {
            if (!(bits & (0x80 >> col))) continue;
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int px = x + col * scale + sx;
                    int py = y + row * scale + sy;
                    if (px < 0 || px >= 240 || py < 0 || py >= 160) continue;
                    int i = py * 240 + px;
                    if (alpha >= 255) {
                        fb[i] = color;
                    } else {
                        u32 d = fb[i];
                        u8 r = ((color & 0xFF) * alpha + (d & 0xFF) * (255 - alpha)) / 255;
                        u8 g = (((color >> 8) & 0xFF) * alpha + ((d >> 8) & 0xFF) * (255 - alpha)) / 255;
                        u8 b = (((color >> 16) & 0xFF) * alpha + ((d >> 16) & 0xFF) * (255 - alpha)) / 255;
                        fb[i] = r | (g << 8) | (b << 16) | 0xFF000000u;
                    }
                }
            }
        }
    }
}

static void boot_draw_string(u32* fb, const char* text, int x, int y, int scale, u32 color, int alpha) {
    for (int i = 0; text[i]; i++) {
        boot_draw_char(fb, text[i], x + i * 8 * scale, y, scale, color, alpha);
    }
}

static int boot_text_width(const char* text, int scale) {
    int len = 0;
    while (text[len]) len++;
    return len * 8 * scale;
}

static u32 hsv_to_color(int h, int s, int v) {
    h = ((h % 360) + 360) % 360;
    int region = h / 60;
    int remainder = (h - region * 60) * 255 / 60;
    int p = v * (255 - s) / 255;
    int q = v * (255 - s * remainder / 255) / 255;
    int t = v * (255 - s * (255 - remainder) / 255) / 255;
    int r, g, b;
    switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | 0xFF000000u;
}

void GBA::render_boot_frame() {
    u32* fb = ppu_.framebuffer_data();
    int f = boot_anim_frame_++;

    constexpr int ANIM_FRAMES = 220;
    if (f >= ANIM_FRAMES) {
        boot_anim_active_ = false;
        return;
    }

    for (int i = 0; i < 240 * 160; i++) fb[i] = 0xFFFFFFFF;

    const char* text = "GAME BOY";
    const int text_len = 8; // space here :)
    int scale = 3;
    int char_w = 8 * scale;
    int total_w = text_len * char_w;
    int base_x = (240 - total_w) / 2;
    int text_y = 45;

    const int letter_interval = 5;
    const int rainbow_end = 100;      // rainbow ends :(
    const int blue_settle = 120;      //blue frame

    for (int i = 0; i < text_len; i++) {
        char ch = text[i];
        if (ch == ' ') continue;

        int appear_frame = i * letter_interval;
        if (f < appear_frame) continue;

        int lx = base_x + i * char_w;

        u32 color;
        if (f < rainbow_end) {
            int hue = (i * 45 + f * 8) % 360;
            color = hsv_to_color(hue, 255, 220);
        } else if (f < blue_settle) {
            int blend = (f - rainbow_end) * 255 / (blue_settle - rainbow_end);
            int hue = (i * 45 + f * 8) % 360;
            u32 rainbow = hsv_to_color(hue, 255, 220);
            u32 blue = make_color(0, 0, 200);
            // Lerp
            int rr = (int)(rainbow & 0xFF);
            int rg = (int)((rainbow >> 8) & 0xFF);
            int rb = (int)((rainbow >> 16) & 0xFF);
            int br = (int)(blue & 0xFF);
            int bg = (int)((blue >> 8) & 0xFF);
            int bb = (int)((blue >> 16) & 0xFF);
            int fr = rr + (br - rr) * blend / 255;
            int fg = rg + (bg - rg) * blend / 255;
            int fbl = rb + (bb - rb) * blend / 255;
            color = (u32)fr | ((u32)fg << 8) | ((u32)fbl << 16) | 0xFF000000u;
        } else {
            color = make_color(0, 0, 200);
        }

        int lf = f - appear_frame;
        int cur_scale = scale;
        int offset_x = 0, offset_y = 0;
        if (lf < 4) {
            cur_scale = scale + 2 - lf / 2;
            offset_x = -(8 * (cur_scale - scale)) / 2;
            offset_y = -(8 * (cur_scale - scale)) / 2;
        }

        boot_draw_char(fb, ch, lx + offset_x, text_y + offset_y, cur_scale, color, 255);
    }

    int nintendo_y = text_y + 8 * scale + 20;
    if (f >= blue_settle) {
        u32 black = make_color(10, 10, 10);
        const char* ntext = "Nintendo";
        int nw = boot_text_width(ntext, 2);
        int nx = (240 - nw) / 2;
        int alpha = 255;
        if (f < blue_settle + 20) alpha = 255 * (f - blue_settle) / 20;
        boot_draw_string(fb, ntext, nx, nintendo_y, 2, black, alpha);
    }

    if (f >= 180) {
        int fade = 255 * (f - 180) / 40;
        if (fade > 255) fade = 255;
        for (int i = 0; i < 240 * 160; i++) {
            u32 c = fb[i];
            u8 r = (int)(c & 0xFF) * (255 - fade) / 255;
            u8 g = (int)((c>>8)&0xFF) * (255 - fade) / 255;
            u8 b = (int)((c>>16)&0xFF) * (255 - fade) / 255;
            fb[i] = r | (g << 8) | (b << 16) | 0xFF000000u;
        }
    }
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
