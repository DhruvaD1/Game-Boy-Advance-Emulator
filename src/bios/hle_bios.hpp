// HLE BIOS — SWI handler
#pragma once
#include "types.hpp"

class ARM7TDMI;
class Bus;

class HleBios {
public:
    void set_cpu(ARM7TDMI* cpu) { cpu_ = cpu; }
    void set_bus(Bus* bus) { bus_ = bus; }

    void handle_swi(u8 number);

private:
    ARM7TDMI* cpu_ = nullptr;
    Bus* bus_ = nullptr;

    void swi_soft_reset();
    void swi_register_ram_reset();
    void swi_halt();
    void swi_vblank_intr_wait();
    void swi_intr_wait();
    void swi_div();
    void swi_div_arm();
    void swi_sqrt();
    void swi_arctan();
    void swi_arctan2();
    void swi_cpu_set();
    void swi_cpu_fast_set();
    void swi_bg_affine_set();
    void swi_obj_affine_set();
    void swi_bit_unpack();
    void swi_lz77_uncomp_vram();
    void swi_lz77_uncomp_wram();
    void swi_huff_uncomp();
    void swi_rl_uncomp_vram();
    void swi_rl_uncomp_wram();
    void swi_diff_8bit_unfilter_vram();
    void swi_sound_bias();
    void swi_midi_key_to_freq();
    void swi_sound_driver_init();
};
