// ARM instruction execution — data processing, loads/stores, branches
#include "cpu/arm_instructions.hpp"
#include "cpu/arm7tdmi.hpp"
#include "memory/bus.hpp"
#include <cstdio>

static int msr(ARM7TDMI& cpu, u32 instr);
static int mrs(ARM7TDMI& cpu, u32 instr);

static inline u32 rd(u32 i) { return bits(i, 15, 12); }
static inline u32 rn(u32 i) { return bits(i, 19, 16); }
static inline u32 rs(u32 i) { return bits(i, 11, 8); }
static inline u32 rm(u32 i) { return bits(i, 3, 0); }

static u32 reg_val(ARM7TDMI& cpu, int r, u32 instr) {
    if (r == 15) return cpu.pc() + 4;
    return cpu.reg(r);
}

static u32 dp_imm(u32 instr, bool& carry) {
    u32 imm = instr & 0xFF;
    int rot = bits(instr, 11, 8) * 2;
    if (rot == 0) return imm;
    u32 result = (imm >> rot) | (imm << (32 - rot));
    carry = (result >> 31) & 1;
    return result;
}

static u32 dp_shift_reg(ARM7TDMI& cpu, u32 instr, bool& carry) {
    u32 val = reg_val(cpu, rm(instr), instr);
    int shift_type = bits(instr, 6, 5);

    if (test_bit(instr, 4)) {

        int amount = cpu.reg(rs(instr)) & 0xFF;
        if (rm(instr) == 15) val += 4;
        return cpu.barrel_shift(val, shift_type, amount, carry, true);
    } else {

        int amount = bits(instr, 11, 7);
        return cpu.barrel_shift(val, shift_type, amount, carry, false);
    }
}

static u32 alu_add(u32 a, u32 b, ARM7TDMI& cpu, bool set_flags) {
    u64 result = (u64)a + (u64)b;
    u32 r = (u32)result;
    if (set_flags) {
        cpu.set_flag_c(result > 0xFFFFFFFF);
        cpu.set_flag_v(((a ^ ~b) & (a ^ r)) >> 31);
        cpu.set_nz(r);
    }
    return r;
}

static u32 alu_adc(u32 a, u32 b, ARM7TDMI& cpu, bool set_flags) {
    u32 c = cpu.flag_c() ? 1 : 0;
    u64 result = (u64)a + (u64)b + c;
    u32 r = (u32)result;
    if (set_flags) {
        cpu.set_flag_c(result > 0xFFFFFFFF);
        cpu.set_flag_v(((a ^ ~b) & (a ^ r)) >> 31);
        cpu.set_nz(r);
    }
    return r;
}

static u32 alu_sub(u32 a, u32 b, ARM7TDMI& cpu, bool set_flags) {
    u32 r = a - b;
    if (set_flags) {
        cpu.set_flag_c(a >= b);
        cpu.set_flag_v(((a ^ b) & (a ^ r)) >> 31);
        cpu.set_nz(r);
    }
    return r;
}

static u32 alu_sbc(u32 a, u32 b, ARM7TDMI& cpu, bool set_flags) {
    u32 c = cpu.flag_c() ? 0 : 1;
    u32 r = a - b - c;
    if (set_flags) {
        cpu.set_flag_c((u64)a >= (u64)b + c);
        cpu.set_flag_v(((a ^ b) & (a ^ r)) >> 31);
        cpu.set_nz(r);
    }
    return r;
}

static int data_processing(ARM7TDMI& cpu, u32 instr) {
    int opcode = bits(instr, 24, 21);
    bool S = test_bit(instr, 20);

    if (!S && opcode >= 0x8 && opcode <= 0xB) {
        if (test_bit(instr, 25)) {

            return msr(cpu, instr);
        }

        return 1;
    }

    int d = rd(instr);
    u32 op1 = reg_val(cpu, rn(instr), instr);

    bool carry = cpu.flag_c();
    u32 op2;
    if (test_bit(instr, 25)) {
        op2 = dp_imm(instr, carry);
    } else {
        op2 = dp_shift_reg(cpu, instr, carry);
    }

    u32 result = 0;
    bool write_result = true;
    int cycles = 1;

    if (!test_bit(instr, 25) && test_bit(instr, 4)) cycles++;

    switch (opcode) {
        case 0x0:
            result = op1 & op2;
            if (S && d != 15) { cpu.set_nz(result); cpu.set_flag_c(carry); }
            break;
        case 0x1:
            result = op1 ^ op2;
            if (S && d != 15) { cpu.set_nz(result); cpu.set_flag_c(carry); }
            break;
        case 0x2:
            result = alu_sub(op1, op2, cpu, S && d != 15);
            break;
        case 0x3:
            result = alu_sub(op2, op1, cpu, S && d != 15);
            break;
        case 0x4:
            result = alu_add(op1, op2, cpu, S && d != 15);
            break;
        case 0x5:
            result = alu_adc(op1, op2, cpu, S && d != 15);
            break;
        case 0x6:
            result = alu_sbc(op1, op2, cpu, S && d != 15);
            break;
        case 0x7:
            result = alu_sbc(op2, op1, cpu, S && d != 15);
            break;
        case 0x8:
            result = op1 & op2;
            write_result = false;
            if (S) { cpu.set_nz(result); cpu.set_flag_c(carry); }
            break;
        case 0x9:
            result = op1 ^ op2;
            write_result = false;
            if (S) { cpu.set_nz(result); cpu.set_flag_c(carry); }
            break;
        case 0xA:
            result = alu_sub(op1, op2, cpu, true);
            write_result = false;
            break;
        case 0xB:
            result = alu_add(op1, op2, cpu, true);
            write_result = false;
            break;
        case 0xC:
            result = op1 | op2;
            if (S && d != 15) { cpu.set_nz(result); cpu.set_flag_c(carry); }
            break;
        case 0xD:
            result = op2;
            if (S && d != 15) { cpu.set_nz(result); cpu.set_flag_c(carry); }
            break;
        case 0xE:
            result = op1 & ~op2;
            if (S && d != 15) { cpu.set_nz(result); cpu.set_flag_c(carry); }
            break;
        case 0xF:
            result = ~op2;
            if (S && d != 15) { cpu.set_nz(result); cpu.set_flag_c(carry); }
            break;
    }

    if (write_result) {
        cpu.reg(d) = result;
        if (d == 15) {
            if (S) {
                cpu.set_cpsr(cpu.spsr());
            }
            cpu.flush_pipeline();
            cycles += 2;
        }
    }

    return cycles;
}

static int multiply(ARM7TDMI& cpu, u32 instr) {
    bool A = test_bit(instr, 21);
    bool S = test_bit(instr, 20);
    int d = rn(instr);
    u32 rs_val = cpu.reg(bits(instr, 11, 8));
    u32 rm_val = cpu.reg(rm(instr));

    u32 result;
    if (A) {
        result = rm_val * rs_val + cpu.reg(rd(instr));
    } else {
        result = rm_val * rs_val;
    }

    cpu.reg(d) = result;
    if (S) cpu.set_nz(result);

    int cycles = 1;
    if ((rs_val & 0xFFFFFF00) == 0 || (rs_val & 0xFFFFFF00) == 0xFFFFFF00) cycles += 1;
    else if ((rs_val & 0xFFFF0000) == 0 || (rs_val & 0xFFFF0000) == 0xFFFF0000) cycles += 2;
    else if ((rs_val & 0xFF000000) == 0 || (rs_val & 0xFF000000) == 0xFF000000) cycles += 3;
    else cycles += 4;
    if (A) cycles++;

    return cycles;
}

static int multiply_long(ARM7TDMI& cpu, u32 instr) {
    bool U = test_bit(instr, 22);
    bool A = test_bit(instr, 21);
    bool S = test_bit(instr, 20);
    int dhi = rn(instr);
    int dlo = rd(instr);
    u32 rs_val = cpu.reg(rs(instr));
    u32 rm_val = cpu.reg(rm(instr));

    u64 result;
    if (U) {
        result = (s64)(s32)rm_val * (s64)(s32)rs_val;
    } else {
        result = (u64)rm_val * (u64)rs_val;
    }

    if (A) {
        result += ((u64)cpu.reg(dhi) << 32) | cpu.reg(dlo);
    }

    cpu.reg(dlo) = (u32)result;
    cpu.reg(dhi) = (u32)(result >> 32);

    if (S) {
        cpu.set_flag_n(cpu.reg(dhi) >> 31);
        cpu.set_flag_z(result == 0);
    }

    int cycles = 2;
    if ((rs_val & 0xFFFFFF00) == 0 || (rs_val & 0xFFFFFF00) == 0xFFFFFF00) cycles += 1;
    else if ((rs_val & 0xFFFF0000) == 0 || (rs_val & 0xFFFF0000) == 0xFFFF0000) cycles += 2;
    else if ((rs_val & 0xFF000000) == 0 || (rs_val & 0xFF000000) == 0xFF000000) cycles += 3;
    else cycles += 4;
    if (A) cycles++;

    return cycles;
}

static int single_transfer(ARM7TDMI& cpu, u32 instr) {
    bool I = test_bit(instr, 25);
    bool P = test_bit(instr, 24);
    bool U = test_bit(instr, 23);
    bool B = test_bit(instr, 22);
    bool W = test_bit(instr, 21);
    bool L = test_bit(instr, 20);

    u32 base = reg_val(cpu, rn(instr), instr);
    u32 offset;

    if (I) {
        bool carry = cpu.flag_c();
        int shift_type = bits(instr, 6, 5);
        int shift_amount = bits(instr, 11, 7);
        offset = cpu.barrel_shift(cpu.reg(rm(instr)), shift_type, shift_amount, carry, false);
    } else {
        offset = instr & 0xFFF;
    }

    u32 addr = U ? base + offset : base - offset;
    u32 effective = P ? addr : base;

    int cycles = 1;

    if (L) {
        u32 val;
        if (B) {
            val = cpu.read8(effective);
        } else {
            val = cpu.read32(effective & ~3u);

            int rot = (effective & 3) * 8;
            if (rot) val = (val >> rot) | (val << (32 - rot));
        }
        cpu.reg(rd(instr)) = val;
        if (rd(instr) == 15) {
            cpu.flush_pipeline();
            cycles += 4;
        }
        cycles += 2;
    } else {
        u32 val = reg_val(cpu, rd(instr), instr);
        if (rd(instr) == 15) val += 4;
        if (B) {
            cpu.write8(effective, (u8)val);
        } else {
            cpu.write32(effective & ~3u, val);
        }
        cycles += 1;
    }

    if (!P || W) {
        if (rn(instr) != rd(instr) || !L)
            cpu.reg(rn(instr)) = P ? addr : addr;
        if (!P) cpu.reg(rn(instr)) = addr;
    }

    return cycles;
}

static int halfword_transfer(ARM7TDMI& cpu, u32 instr) {
    bool P = test_bit(instr, 24);
    bool U = test_bit(instr, 23);
    bool I = test_bit(instr, 22);
    bool W = test_bit(instr, 21);
    bool L = test_bit(instr, 20);
    int sh = bits(instr, 6, 5);

    u32 base = reg_val(cpu, rn(instr), instr);
    u32 offset;
    if (I) {
        offset = (bits(instr, 11, 8) << 4) | (instr & 0xF);
    } else {
        offset = cpu.reg(rm(instr));
    }

    u32 addr = U ? base + offset : base - offset;
    u32 effective = P ? addr : base;

    int cycles = 1;

    if (L) {
        u32 val;
        switch (sh) {
            case 1:
                val = cpu.read16(effective & ~1u);
                if (effective & 1) val = (val >> 8) | (val << 24);
                break;
            case 2:
                val = (s32)(s8)cpu.read8(effective);
                break;
            case 3:
                if (effective & 1) {
                    val = (s32)(s8)cpu.read8(effective);
                } else {
                    val = (s32)(s16)cpu.read16(effective);
                }
                break;
            default:
                val = 0;
                break;
        }
        cpu.reg(rd(instr)) = val;
        if (rd(instr) == 15) { cpu.flush_pipeline(); cycles += 4; }
        cycles += 2;
    } else {

        u32 val = reg_val(cpu, rd(instr), instr);
        cpu.write16(effective & ~1u, (u16)val);
        cycles += 1;
    }

    if (!P || W) {
        cpu.reg(rn(instr)) = addr;
    }

    return cycles;
}

static int block_transfer(ARM7TDMI& cpu, u32 instr) {
    bool P = test_bit(instr, 24);
    bool U = test_bit(instr, 23);
    bool S = test_bit(instr, 22);
    bool W = test_bit(instr, 21);
    bool L = test_bit(instr, 20);

    u16 reg_list = instr & 0xFFFF;
    u32 base = cpu.reg(rn(instr));
    int count = __builtin_popcount(reg_list);

    if (count == 0) {

        count = 16;
    }

    u32 start_addr;
    u32 writeback_addr;

    if (U) {
        start_addr = P ? base + 4 : base;
        writeback_addr = base + count * 4;
    } else {
        start_addr = P ? base - count * 4 : base - count * 4 + 4;
        writeback_addr = base - count * 4;
    }

    u32 addr = start_addr;
    int cycles = L ? 2 : 1;
    bool first = true;

    for (int i = 0; i < 16; i++) {
        if (!(reg_list & (1 << i))) continue;

        if (L) {
            u32 val = cpu.read32(addr & ~3u);
            if (S && !(reg_list & (1 << 15))) {

                cpu.reg(i) = val;
            } else {
                cpu.reg(i) = val;
            }
            if (i == 15) {
                if (S) cpu.set_cpsr(cpu.spsr());
                cpu.flush_pipeline();
                cycles += 2;
            }
        } else {
            u32 val = cpu.reg(i);
            if (i == 15) val += 4;
            if (first && W) {

            }
            cpu.write32(addr & ~3u, val);
        }
        addr += 4;
        first = false;
        cycles++;
    }

    if (W && (!L || !(reg_list & (1 << rn(instr))))) {
        cpu.reg(rn(instr)) = writeback_addr;
    }

    return cycles;
}

static int branch(ARM7TDMI& cpu, u32 instr) {
    bool link = test_bit(instr, 24);
    s32 offset = (s32)(instr << 8) >> 6;

    if (link) {
        cpu.reg(14) = cpu.pc();
    }

    cpu.pc() += offset + 4;
    cpu.flush_pipeline();
    return 3;
}

static int branch_exchange(ARM7TDMI& cpu, u32 instr) {
    u32 addr = cpu.reg(rm(instr));
    if (addr & 1) {
        cpu.set_cpsr(cpu.cpsr() | (1 << CPSR_T));
        cpu.pc() = addr & ~1u;
    } else {
        cpu.set_cpsr(cpu.cpsr() & ~(1 << CPSR_T));
        cpu.pc() = addr & ~3u;
    }
    cpu.flush_pipeline();
    return 3;
}

static int software_interrupt(ARM7TDMI& cpu, u32 instr) {
    cpu.raise_exception(VECTOR_SWI, CpuMode::Supervisor);
    return 3;
}

static int mrs(ARM7TDMI& cpu, u32 instr) {
    bool use_spsr = test_bit(instr, 22);
    cpu.reg(rd(instr)) = use_spsr ? cpu.spsr() : cpu.cpsr();
    return 1;
}

static int msr(ARM7TDMI& cpu, u32 instr) {
    bool use_spsr = test_bit(instr, 22);
    u32 val;

    if (test_bit(instr, 25)) {
        bool carry = cpu.flag_c();
        val = dp_imm(instr, carry);
    } else {
        val = cpu.reg(rm(instr));
    }

    u32 mask = 0;
    if (test_bit(instr, 19)) mask |= 0xFF000000;
    if (test_bit(instr, 18)) mask |= 0x00FF0000;
    if (test_bit(instr, 17)) mask |= 0x0000FF00;
    if (test_bit(instr, 16)) mask |= 0x000000FF;

    if (cpu.current_mode() == CpuMode::User)
        mask &= 0xFF000000;

    if (use_spsr) {
        u32 spsr = cpu.spsr();
        cpu.set_spsr((spsr & ~mask) | (val & mask));
    } else {
        u32 cpsr = cpu.cpsr();
        cpu.set_cpsr((cpsr & ~mask) | (val & mask));
    }

    return 1;
}

static int swap(ARM7TDMI& cpu, u32 instr) {
    bool B = test_bit(instr, 22);
    u32 addr = cpu.reg(rn(instr));

    if (B) {
        u8 old = cpu.read8(addr);
        cpu.write8(addr, (u8)cpu.reg(rm(instr)));
        cpu.reg(rd(instr)) = old;
    } else {
        u32 old = cpu.read32(addr & ~3u);
        int rot = (addr & 3) * 8;
        if (rot) old = (old >> rot) | (old << (32 - rot));
        cpu.write32(addr & ~3u, cpu.reg(rm(instr)));
        cpu.reg(rd(instr)) = old;
    }
    return 4;
}

static int count_leading_zeros(ARM7TDMI& cpu, u32 instr) {
    u32 val = cpu.reg(rm(instr));
    cpu.reg(rd(instr)) = val ? __builtin_clz(val) : 32;
    return 1;
}

int arm::execute(ARM7TDMI& cpu, u32 instr) {

    u32 cond = bits(instr, 31, 28);
    if (!cpu.check_condition(cond)) return 1;

    u32 op1 = bits(instr, 27, 20);
    u32 op2 = bits(instr, 7, 4);

    if (cond == 0xF) {

        s32 offset = (s32)(instr << 8) >> 6;
        offset |= (test_bit(instr, 24) ? 2 : 0);
        cpu.reg(14) = cpu.pc();
        cpu.pc() += offset + 4;
        cpu.set_cpsr(cpu.cpsr() | (1 << CPSR_T));
        cpu.flush_pipeline();
        return 3;
    }

    u32 bits27_26 = (instr >> 26) & 3;
    u32 bits24_20 = (instr >> 20) & 0x1F;

    switch (bits27_26) {
        case 0b00: {

            if ((op2 & 0xF) == 0x9) {

                if ((op1 & 0xFC) == 0x00) return multiply(cpu, instr);
                if ((op1 & 0xF8) == 0x08) return multiply_long(cpu, instr);
                if ((op1 & 0xFB) == 0x10) return swap(cpu, instr);
            }

            if (!test_bit(instr, 25) && (op2 & 0x9) == 0x9 && (op2 & 0x6) != 0) {
                return halfword_transfer(cpu, instr);
            }

            if ((instr & 0x0FFFFFF0) == 0x012FFF10) {
                return branch_exchange(cpu, instr);
            }

            if ((instr & 0x0FFFFFF0) == 0x012FFF30) {
                cpu.reg(14) = cpu.pc() - 4;
                return branch_exchange(cpu, instr);
            }

            if ((op1 & 0xFB) == 0x10 && op2 == 0) {
                return mrs(cpu, instr);
            }

            if ((op1 & 0xFB) == 0x12 && (op2 & 0xF) == 0) {
                return msr(cpu, instr);
            }

            return data_processing(cpu, instr);
        }

        case 0b01: {

            if (test_bit(instr, 25) && test_bit(instr, 4)) {

                return 1;
            }
            return single_transfer(cpu, instr);
        }

        case 0b10: {
            if (test_bit(instr, 25)) {

                return branch(cpu, instr);
            }

            return block_transfer(cpu, instr);
        }

        case 0b11: {
            if (test_bit(instr, 25)) {
                if (test_bit(instr, 24)) {

                    return software_interrupt(cpu, instr);
                }

                return 1;
            }

            return 1;
        }
    }

    return 1;
}
