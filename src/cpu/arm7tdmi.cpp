// ARM7TDMI fetch/decode/execute, mode switching, IRQ handling
#include "cpu/arm7tdmi.hpp"
#include "cpu/arm_instructions.hpp"
#include "cpu/thumb_instructions.hpp"
#include "memory/bus.hpp"
#include <cstring>

ARM7TDMI::ARM7TDMI() {
    reset();
}

void ARM7TDMI::reset() {
    regs_.fill(0);
    fiq_regs_.fill(0);
    svc_regs_.fill(0);
    abt_regs_.fill(0);
    irq_regs_.fill(0);
    und_regs_.fill(0);
    usr_regs_.fill(0);
    spsr_.fill(0);

    cpsr_ = static_cast<u32>(CpuMode::System) | (1 << CPSR_I) | (1 << CPSR_F);

    regs_[15] = 0x08000000;
    regs_[13] = 0x03007F00;
    irq_regs_[0] = 0x03007FA0;
    svc_regs_[0] = 0x03007FE0;

    pipeline_valid_ = false;
    halted = false;
}

static int rom_fetch_cycles(u32 addr, bool is_sequential, bool is_arm) {
    u32 region = addr >> 24;
    if (region >= 0x08 && region <= 0x0D) {

        int n_cost = 5;
        int s_cost = 3;
        if (is_arm) {

            return is_sequential ? (s_cost + s_cost) : (n_cost + s_cost);
        } else {

            return is_sequential ? s_cost : n_cost;
        }
    } else if (region == 0x02) {

        return is_arm ? 6 : 3;
    }

    return is_arm ? 1 : 1;
}

int ARM7TDMI::step() {
    if (halted) {
        return 1;
    }

    u32 old_pc = pc();

    if (in_thumb()) {
        u32 addr = pc() & ~1u;
        u16 instr = read16(addr);
        pipeline[0] = instr;
        pc() += 2;
        int cycles = execute_thumb(instr);

        bool is_seq = (addr == last_fetch_addr_ + 2);
        last_fetch_addr_ = addr;
        cycles += rom_fetch_cycles(addr, is_seq, false) - 1;
        return cycles;
    } else {
        u32 addr = pc() & ~3u;
        u32 instr = read32(addr);
        pipeline[0] = instr;
        pc() += 4;
        int cycles = execute_arm(instr);

        bool is_seq = (addr == last_fetch_addr_ + 4);
        last_fetch_addr_ = addr;
        cycles += rom_fetch_cycles(addr, is_seq, true) - 1;
        return cycles;
    }
}

void ARM7TDMI::check_irq() {
    if (!bus_) return;

    u16 ie = bus_->read16(0x04000200);
    u16 iff = bus_->read16(0x04000202);
    u16 ime = bus_->read16(0x04000208);

    if (ime && (ie & iff)) {

        halted = false;

        if (!test_bit(cpsr_, CPSR_I)) {
            u32 handler = bus_->read32(0x03FFFFFC);
            if (handler != 0) {
                raise_exception(VECTOR_IRQ, CpuMode::IRQ);
            }
        }
    }
}

u32& ARM7TDMI::reg(int n) { return regs_[n]; }
u32 ARM7TDMI::reg(int n) const { return regs_[n]; }

void ARM7TDMI::set_cpsr(u32 val) {
    CpuMode old_mode = current_mode();
    cpsr_ = val;
    CpuMode new_mode = current_mode();
    if (old_mode != new_mode) {
        bank_registers(old_mode, new_mode);
    }
}

u32 ARM7TDMI::spsr() const {
    int idx = spsr_index();
    if (idx < 0) return cpsr_;
    return spsr_[idx];
}

void ARM7TDMI::set_spsr(u32 val) {
    int idx = spsr_index();
    if (idx >= 0) spsr_[idx] = val;
}

int ARM7TDMI::spsr_index() const {
    switch (current_mode()) {
        case CpuMode::FIQ:        return 0;
        case CpuMode::Supervisor: return 1;
        case CpuMode::Abort:      return 2;
        case CpuMode::IRQ:        return 3;
        case CpuMode::Undefined:  return 4;
        default: return -1;
    }
}

void ARM7TDMI::switch_mode(CpuMode new_mode) {
    CpuMode old_mode = current_mode();
    if (old_mode == new_mode) return;
    bank_registers(old_mode, new_mode);
    cpsr_ = (cpsr_ & ~0x1Fu) | static_cast<u32>(new_mode);
}

void ARM7TDMI::bank_registers(CpuMode old_mode, CpuMode new_mode) {

    auto save_banked = [&](CpuMode mode) {
        switch (mode) {
            case CpuMode::FIQ:

                for (int i = 0; i < 7; i++) fiq_regs_[i] = regs_[8 + i];
                for (int i = 0; i < 5; i++) regs_[8 + i] = usr_regs_[i];
                regs_[13] = usr_regs_[5];
                regs_[14] = usr_regs_[6];
                break;
            case CpuMode::IRQ:
                irq_regs_[0] = regs_[13]; irq_regs_[1] = regs_[14]; break;
            case CpuMode::Supervisor:
                svc_regs_[0] = regs_[13]; svc_regs_[1] = regs_[14]; break;
            case CpuMode::Abort:
                abt_regs_[0] = regs_[13]; abt_regs_[1] = regs_[14]; break;
            case CpuMode::Undefined:
                und_regs_[0] = regs_[13]; und_regs_[1] = regs_[14]; break;
            case CpuMode::User:
            case CpuMode::System:
                usr_regs_[5] = regs_[13]; usr_regs_[6] = regs_[14];

                for (int i = 0; i < 5; i++) usr_regs_[i] = regs_[8 + i];
                break;
        }
    };

    auto load_banked = [&](CpuMode mode) {
        switch (mode) {
            case CpuMode::FIQ:

                for (int i = 0; i < 5; i++) usr_regs_[i] = regs_[8 + i];
                usr_regs_[5] = regs_[13]; usr_regs_[6] = regs_[14];
                for (int i = 0; i < 7; i++) regs_[8 + i] = fiq_regs_[i];
                break;
            case CpuMode::IRQ:
                regs_[13] = irq_regs_[0]; regs_[14] = irq_regs_[1]; break;
            case CpuMode::Supervisor:
                regs_[13] = svc_regs_[0]; regs_[14] = svc_regs_[1]; break;
            case CpuMode::Abort:
                regs_[13] = abt_regs_[0]; regs_[14] = abt_regs_[1]; break;
            case CpuMode::Undefined:
                regs_[13] = und_regs_[0]; regs_[14] = und_regs_[1]; break;
            case CpuMode::User:
            case CpuMode::System:

                regs_[13] = usr_regs_[5]; regs_[14] = usr_regs_[6];
                break;
        }
    };

    save_banked(old_mode);
    load_banked(new_mode);
}

void ARM7TDMI::raise_exception(u32 vector, CpuMode mode) {
    u32 old_cpsr = cpsr_;
    u32 return_addr = pc();

    switch (vector) {
        case VECTOR_SWI:
        case VECTOR_UND:

            return_addr = pc();
            break;
        case VECTOR_IRQ:
        case VECTOR_FIQ:

            return_addr = pc() + 4;
            break;
        default:
            break;
    }

    switch_mode(mode);
    set_spsr(old_cpsr);
    regs_[14] = return_addr;
    cpsr_ = (cpsr_ & ~((1 << CPSR_T) | 0x1Fu)) | static_cast<u32>(mode) | (1 << CPSR_I);
    if (vector == VECTOR_FIQ || vector == VECTOR_RESET)
        cpsr_ |= (1 << CPSR_F);
    pc() = vector;
    pipeline_valid_ = false;
}

u32 ARM7TDMI::barrel_shift(u32 val, int type, int amount, bool& carry, bool reg_shift) {
    if (amount == 0 && !reg_shift) {

        switch (type) {
            case 0:
                return val;
            case 1:
                carry = (val >> 31) & 1;
                return 0;
            case 2:
                carry = (val >> 31) & 1;
                return carry ? 0xFFFFFFFF : 0;
            case 3:
                {
                    bool old_carry = carry;
                    carry = val & 1;
                    return (old_carry ? (1u << 31) : 0) | (val >> 1);
                }
        }
    }

    switch (type) {
        case 0:
            if (amount == 0) return val;
            if (amount < 32) {
                carry = (val >> (32 - amount)) & 1;
                return val << amount;
            }
            if (amount == 32) { carry = val & 1; return 0; }
            carry = 0; return 0;

        case 1:
            if (amount == 0) return val;
            if (amount < 32) {
                carry = (val >> (amount - 1)) & 1;
                return val >> amount;
            }
            if (amount == 32) { carry = (val >> 31) & 1; return 0; }
            carry = 0; return 0;

        case 2:
            if (amount == 0) return val;
            if (amount < 32) {
                carry = (val >> (amount - 1)) & 1;
                return static_cast<s32>(val) >> amount;
            }
            carry = (val >> 31) & 1;
            return carry ? 0xFFFFFFFF : 0;

        case 3:
            if (amount == 0) return val;
            amount &= 31;
            if (amount == 0) { carry = (val >> 31) & 1; return val; }
            u32 result = (val >> amount) | (val << (32 - amount));
            carry = (result >> 31) & 1;
            return result;
    }
    return val;
}

bool ARM7TDMI::check_condition(u32 cond) const {
    switch (cond) {
        case 0x0: return flag_z();
        case 0x1: return !flag_z();
        case 0x2: return flag_c();
        case 0x3: return !flag_c();
        case 0x4: return flag_n();
        case 0x5: return !flag_n();
        case 0x6: return flag_v();
        case 0x7: return !flag_v();
        case 0x8: return flag_c() && !flag_z();
        case 0x9: return !flag_c() || flag_z();
        case 0xA: return flag_n() == flag_v();
        case 0xB: return flag_n() != flag_v();
        case 0xC: return !flag_z() && (flag_n() == flag_v());
        case 0xD: return flag_z() || (flag_n() != flag_v());
        case 0xE: return true;
        case 0xF: return true;
        default: return false;
    }
}

u8  ARM7TDMI::read8(u32 addr)  { return bus_->read8(addr); }
u16 ARM7TDMI::read16(u32 addr) { return bus_->read16(addr); }
u32 ARM7TDMI::read32(u32 addr) { return bus_->read32(addr); }
void ARM7TDMI::write8(u32 addr, u8 val)   { bus_->write8(addr, val); }
void ARM7TDMI::write16(u32 addr, u16 val) { bus_->write16(addr, val); }
void ARM7TDMI::write32(u32 addr, u32 val) { bus_->write32(addr, val); }

void ARM7TDMI::flush_pipeline() {
    pipeline_valid_ = false;
    last_fetch_addr_ = 0xFFFFFFFF;
}

int ARM7TDMI::execute_arm(u32 instr) {
    return arm::execute(*this, instr);
}

int ARM7TDMI::execute_thumb(u16 instr) {
    return thumb::execute(*this, instr);
}
