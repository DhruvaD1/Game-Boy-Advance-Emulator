// ARM7TDMI CPU core — registers, modes, pipeline
#pragma once
#include "types.hpp"
#include <array>
#include <functional>
#include <cstdio>

class Bus;

enum class CpuMode : u8 {
    User       = 0x10,
    FIQ        = 0x11,
    IRQ        = 0x12,
    Supervisor = 0x13,
    Abort      = 0x17,
    Undefined  = 0x1B,
    System     = 0x1F
};

constexpr int CPSR_N = 31;
constexpr int CPSR_Z = 30;
constexpr int CPSR_C = 29;
constexpr int CPSR_V = 28;
constexpr int CPSR_I = 7;
constexpr int CPSR_F = 6;
constexpr int CPSR_T = 5;

constexpr u32 VECTOR_RESET = 0x00;
constexpr u32 VECTOR_UND   = 0x04;
constexpr u32 VECTOR_SWI   = 0x08;
constexpr u32 VECTOR_PABT  = 0x0C;
constexpr u32 VECTOR_DABT  = 0x10;
constexpr u32 VECTOR_IRQ   = 0x18;
constexpr u32 VECTOR_FIQ   = 0x1C;

class ARM7TDMI {
public:
    ARM7TDMI();

    void reset();
    void set_bus(Bus* bus) { bus_ = bus; }

    int step();

    void check_irq();

    u32& reg(int n);
    u32  reg(int n) const;
    u32& pc() { return regs_[15]; }
    u32  pc() const { return regs_[15]; }

    u32  cpsr() const { return cpsr_; }
    void set_cpsr(u32 val);
    u32  spsr() const;
    void set_spsr(u32 val);

    bool flag_n() const { return test_bit(cpsr_, CPSR_N); }
    bool flag_z() const { return test_bit(cpsr_, CPSR_Z); }
    bool flag_c() const { return test_bit(cpsr_, CPSR_C); }
    bool flag_v() const { return test_bit(cpsr_, CPSR_V); }

    void set_flag_n(bool v) { cpsr_ = v ? set_bit(cpsr_, CPSR_N) : clear_bit(cpsr_, CPSR_N); }
    void set_flag_z(bool v) { cpsr_ = v ? set_bit(cpsr_, CPSR_Z) : clear_bit(cpsr_, CPSR_Z); }
    void set_flag_c(bool v) { cpsr_ = v ? set_bit(cpsr_, CPSR_C) : clear_bit(cpsr_, CPSR_C); }
    void set_flag_v(bool v) { cpsr_ = v ? set_bit(cpsr_, CPSR_V) : clear_bit(cpsr_, CPSR_V); }

    void set_nz(u32 result) {
        set_flag_n(result >> 31);
        set_flag_z(result == 0);
    }

    bool in_thumb() const { return test_bit(cpsr_, CPSR_T); }

    CpuMode current_mode() const { return static_cast<CpuMode>(cpsr_ & 0x1F); }
    void switch_mode(CpuMode new_mode);

    void raise_exception(u32 vector, CpuMode mode);

    u32 barrel_shift(u32 val, int type, int amount, bool& carry, bool reg_shift);

    bool check_condition(u32 cond) const;

    u8  read8(u32 addr);
    u16 read16(u32 addr);
    u32 read32(u32 addr);
    void write8(u32 addr, u8 val);
    void write16(u32 addr, u16 val);
    void write32(u32 addr, u32 val);

    void flush_pipeline();

    bool save_state(FILE* f) const;
    bool load_state(FILE* f);

    bool halted = false;

    u32 pipeline[2] = {0, 0};

    Bus* bus_ = nullptr;

private:

    std::array<u32, 16> regs_{};

    std::array<u32, 7> fiq_regs_{};
    std::array<u32, 2> svc_regs_{};
    std::array<u32, 2> abt_regs_{};
    std::array<u32, 2> irq_regs_{};
    std::array<u32, 2> und_regs_{};
    std::array<u32, 7> usr_regs_{};

    u32 cpsr_ = 0;
    std::array<u32, 5> spsr_{};

    bool pipeline_valid_ = false;
    u32 last_fetch_addr_ = 0;

    int execute_arm(u32 instr);
    int execute_thumb(u16 instr);

    int spsr_index() const;

    void bank_registers(CpuMode old_mode, CpuMode new_mode);
};
