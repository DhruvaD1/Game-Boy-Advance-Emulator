// Thumb instruction execution — all 19 formats
#include "cpu/thumb_instructions.hpp"
#include "cpu/arm7tdmi.hpp"
#include "memory/bus.hpp"
#include <cstdio>

static u32 reg_val(ARM7TDMI& cpu, int r) {
    if (r == 15) return (cpu.pc() + 2) & ~1u;
    return cpu.reg(r);
}

static int fmt1_shift(ARM7TDMI& cpu, u16 instr) {
    int op = bits(instr, 12, 11);
    int offset = bits(instr, 10, 6);
    int rs = bits(instr, 5, 3);
    int rd = bits(instr, 2, 0);

    bool carry = cpu.flag_c();
    u32 result = cpu.barrel_shift(cpu.reg(rs), op, offset, carry, false);

    cpu.reg(rd) = result;
    cpu.set_nz(result);
    cpu.set_flag_c(carry);
    return 1;
}

static int fmt2_add_sub(ARM7TDMI& cpu, u16 instr) {
    bool I = test_bit(instr, 10);
    bool sub = test_bit(instr, 9);
    int rn_or_imm = bits(instr, 8, 6);
    int rs = bits(instr, 5, 3);
    int rd = bits(instr, 2, 0);

    u32 op1 = cpu.reg(rs);
    u32 op2 = I ? rn_or_imm : cpu.reg(rn_or_imm);

    u32 result;
    if (sub) {
        result = op1 - op2;
        cpu.set_flag_c(op1 >= op2);
        cpu.set_flag_v(((op1 ^ op2) & (op1 ^ result)) >> 31);
    } else {
        u64 r = (u64)op1 + op2;
        result = (u32)r;
        cpu.set_flag_c(r > 0xFFFFFFFF);
        cpu.set_flag_v(((op1 ^ ~op2) & (op1 ^ result)) >> 31);
    }

    cpu.reg(rd) = result;
    cpu.set_nz(result);
    return 1;
}

static int fmt3_imm(ARM7TDMI& cpu, u16 instr) {
    int op = bits(instr, 12, 11);
    int rd = bits(instr, 10, 8);
    u32 imm = instr & 0xFF;

    u32 result;
    switch (op) {
        case 0:
            result = imm;
            cpu.reg(rd) = result;
            cpu.set_nz(result);
            break;
        case 1:
            result = cpu.reg(rd) - imm;
            cpu.set_flag_c(cpu.reg(rd) >= imm);
            cpu.set_flag_v(((cpu.reg(rd) ^ imm) & (cpu.reg(rd) ^ result)) >> 31);
            cpu.set_nz(result);
            break;
        case 2: {
            u64 r = (u64)cpu.reg(rd) + imm;
            result = (u32)r;
            cpu.set_flag_c(r > 0xFFFFFFFF);
            cpu.set_flag_v(((cpu.reg(rd) ^ ~imm) & (cpu.reg(rd) ^ result)) >> 31);
            cpu.reg(rd) = result;
            cpu.set_nz(result);
            break;
        }
        case 3: {
            u32 op1 = cpu.reg(rd);
            result = op1 - imm;
            cpu.set_flag_c(op1 >= imm);
            cpu.set_flag_v(((op1 ^ imm) & (op1 ^ result)) >> 31);
            cpu.reg(rd) = result;
            cpu.set_nz(result);
            break;
        }
    }
    return 1;
}

static int fmt4_alu(ARM7TDMI& cpu, u16 instr) {
    int op = bits(instr, 9, 6);
    int rs = bits(instr, 5, 3);
    int rd = bits(instr, 2, 0);

    u32 a = cpu.reg(rd);
    u32 b = cpu.reg(rs);
    u32 result;

    switch (op) {
        case 0x0:
            result = a & b; cpu.reg(rd) = result; cpu.set_nz(result); break;
        case 0x1:
            result = a ^ b; cpu.reg(rd) = result; cpu.set_nz(result); break;
        case 0x2: {
            int shift = b & 0xFF;
            bool carry = cpu.flag_c();
            result = cpu.barrel_shift(a, 0, shift, carry, true);
            cpu.reg(rd) = result; cpu.set_nz(result); cpu.set_flag_c(carry);
            return 2;
        }
        case 0x3: {
            int shift = b & 0xFF;
            bool carry = cpu.flag_c();
            result = cpu.barrel_shift(a, 1, shift, carry, true);
            cpu.reg(rd) = result; cpu.set_nz(result); cpu.set_flag_c(carry);
            return 2;
        }
        case 0x4: {
            int shift = b & 0xFF;
            bool carry = cpu.flag_c();
            result = cpu.barrel_shift(a, 2, shift, carry, true);
            cpu.reg(rd) = result; cpu.set_nz(result); cpu.set_flag_c(carry);
            return 2;
        }
        case 0x5: {
            u32 c = cpu.flag_c() ? 1 : 0;
            u64 r = (u64)a + b + c;
            result = (u32)r;
            cpu.set_flag_c(r > 0xFFFFFFFF);
            cpu.set_flag_v(((a ^ ~b) & (a ^ result)) >> 31);
            cpu.reg(rd) = result; cpu.set_nz(result);
            break;
        }
        case 0x6: {
            u32 c = cpu.flag_c() ? 0 : 1;
            result = a - b - c;
            cpu.set_flag_c((u64)a >= (u64)b + c);
            cpu.set_flag_v(((a ^ b) & (a ^ result)) >> 31);
            cpu.reg(rd) = result; cpu.set_nz(result);
            break;
        }
        case 0x7: {
            int shift = b & 0xFF;
            bool carry = cpu.flag_c();
            result = cpu.barrel_shift(a, 3, shift, carry, true);
            cpu.reg(rd) = result; cpu.set_nz(result); cpu.set_flag_c(carry);
            return 2;
        }
        case 0x8:
            result = a & b; cpu.set_nz(result); break;
        case 0x9:
            result = 0 - b;
            cpu.set_flag_c(b == 0);
            cpu.set_flag_v((b & result) >> 31);
            cpu.reg(rd) = result; cpu.set_nz(result);
            break;
        case 0xA:
            result = a - b;
            cpu.set_flag_c(a >= b);
            cpu.set_flag_v(((a ^ b) & (a ^ result)) >> 31);
            cpu.set_nz(result);
            break;
        case 0xB: {
            u64 r = (u64)a + b;
            result = (u32)r;
            cpu.set_flag_c(r > 0xFFFFFFFF);
            cpu.set_flag_v(((a ^ ~b) & (a ^ result)) >> 31);
            cpu.set_nz(result);
            break;
        }
        case 0xC:
            result = a | b; cpu.reg(rd) = result; cpu.set_nz(result); break;
        case 0xD: {
            result = a * b; cpu.reg(rd) = result; cpu.set_nz(result);
            return 4;
        }
        case 0xE:
            result = a & ~b; cpu.reg(rd) = result; cpu.set_nz(result); break;
        case 0xF:
            result = ~b; cpu.reg(rd) = result; cpu.set_nz(result); break;
    }
    return 1;
}

static int fmt5_hireg(ARM7TDMI& cpu, u16 instr) {
    int op = bits(instr, 9, 8);
    bool h1 = test_bit(instr, 7);
    bool h2 = test_bit(instr, 6);
    int rs = bits(instr, 5, 3) | (h2 ? 8 : 0);
    int rd = bits(instr, 2, 0) | (h1 ? 8 : 0);

    u32 a = (rd == 15) ? ((cpu.pc() + 2) & ~1u) : cpu.reg(rd);
    u32 b = (rs == 15) ? ((cpu.pc() + 2) & ~1u) : cpu.reg(rs);

    switch (op) {
        case 0:
            cpu.reg(rd) = a + b;
            if (rd == 15) { cpu.pc() &= ~1u; cpu.flush_pipeline(); return 3; }
            break;
        case 1:
        {
            u32 result = a - b;
            cpu.set_flag_c(a >= b);
            cpu.set_flag_v(((a ^ b) & (a ^ result)) >> 31);
            cpu.set_nz(result);
            break;
        }
        case 2:
            cpu.reg(rd) = b;
            if (rd == 15) { cpu.pc() &= ~1u; cpu.flush_pipeline(); return 3; }
            break;
        case 3:
            if (h1) {

                cpu.reg(14) = cpu.pc() | 1;
            }
            if (b & 1) {
                cpu.set_cpsr(cpu.cpsr() | (1 << CPSR_T));
                cpu.pc() = b & ~1u;
            } else {
                cpu.set_cpsr(cpu.cpsr() & ~(1 << CPSR_T));
                cpu.pc() = b & ~3u;
            }
            cpu.flush_pipeline();
            return 3;
    }
    return 1;
}

static int fmt6_pc_load(ARM7TDMI& cpu, u16 instr) {
    int rd = bits(instr, 10, 8);
    u32 offset = (instr & 0xFF) << 2;
    u32 addr = ((cpu.pc() + 2) & ~3u) + offset;
    cpu.reg(rd) = cpu.read32(addr);
    return 3;
}

static int fmt7_reg_offset(ARM7TDMI& cpu, u16 instr) {
    bool L = test_bit(instr, 11);
    bool B = test_bit(instr, 10);
    int ro = bits(instr, 8, 6);
    int rb = bits(instr, 5, 3);
    int rd = bits(instr, 2, 0);

    u32 addr = cpu.reg(rb) + cpu.reg(ro);

    if (L) {
        if (B) {
            cpu.reg(rd) = cpu.read8(addr);
        } else {
            u32 val = cpu.read32(addr & ~3u);
            int rot = (addr & 3) * 8;
            if (rot) val = (val >> rot) | (val << (32 - rot));
            cpu.reg(rd) = val;
        }
        return 3;
    } else {
        if (B) {
            cpu.write8(addr, (u8)cpu.reg(rd));
        } else {
            cpu.write32(addr & ~3u, cpu.reg(rd));
        }
        return 2;
    }
}

static int fmt8_sign_ext(ARM7TDMI& cpu, u16 instr) {
    int op = bits(instr, 11, 10);
    int ro = bits(instr, 8, 6);
    int rb = bits(instr, 5, 3);
    int rd = bits(instr, 2, 0);

    u32 addr = cpu.reg(rb) + cpu.reg(ro);

    switch (op) {
        case 0:
            cpu.write16(addr & ~1u, (u16)cpu.reg(rd));
            return 2;
        case 1:
            cpu.reg(rd) = (s32)(s8)cpu.read8(addr);
            return 3;
        case 2: {
            u32 val = cpu.read16(addr & ~1u);

            if (addr & 1) val = (val >> 8) | (val << 24);
            cpu.reg(rd) = val;
            return 3;
        }
        case 3:
            if (addr & 1)
                cpu.reg(rd) = (s32)(s8)cpu.read8(addr);
            else
                cpu.reg(rd) = (s32)(s16)cpu.read16(addr);
            return 3;
    }
    return 1;
}

static int fmt9_imm_offset(ARM7TDMI& cpu, u16 instr) {
    bool B = test_bit(instr, 12);
    bool L = test_bit(instr, 11);
    int offset = bits(instr, 10, 6);
    int rb = bits(instr, 5, 3);
    int rd = bits(instr, 2, 0);

    u32 addr;
    if (B) {
        addr = cpu.reg(rb) + offset;
    } else {
        addr = cpu.reg(rb) + (offset << 2);
    }

    if (L) {
        if (B) {
            cpu.reg(rd) = cpu.read8(addr);
        } else {
            u32 val = cpu.read32(addr & ~3u);
            int rot = (addr & 3) * 8;
            if (rot) val = (val >> rot) | (val << (32 - rot));
            cpu.reg(rd) = val;
        }
        return 3;
    } else {
        if (B) {
            cpu.write8(addr, (u8)cpu.reg(rd));
        } else {
            cpu.write32(addr & ~3u, cpu.reg(rd));
        }
        return 2;
    }
}

static int fmt10_halfword(ARM7TDMI& cpu, u16 instr) {
    bool L = test_bit(instr, 11);
    int offset = bits(instr, 10, 6) << 1;
    int rb = bits(instr, 5, 3);
    int rd = bits(instr, 2, 0);

    u32 addr = cpu.reg(rb) + offset;

    if (L) {
        u32 val = cpu.read16(addr & ~1u);

        if (addr & 1) val = (val >> 8) | (val << 24);
        cpu.reg(rd) = val;
        return 3;
    } else {
        cpu.write16(addr & ~1u, (u16)cpu.reg(rd));
        return 2;
    }
}

static int fmt11_sp_relative(ARM7TDMI& cpu, u16 instr) {
    bool L = test_bit(instr, 11);
    int rd = bits(instr, 10, 8);
    u32 offset = (instr & 0xFF) << 2;
    u32 addr = cpu.reg(13) + offset;

    if (L) {
        cpu.reg(rd) = cpu.read32(addr & ~3u);
        return 3;
    } else {
        cpu.write32(addr & ~3u, cpu.reg(rd));
        return 2;
    }
}

static int fmt12_load_addr(ARM7TDMI& cpu, u16 instr) {
    bool sp = test_bit(instr, 11);
    int rd = bits(instr, 10, 8);
    u32 offset = (instr & 0xFF) << 2;

    if (sp) {
        cpu.reg(rd) = cpu.reg(13) + offset;
    } else {
        cpu.reg(rd) = ((cpu.pc() + 2) & ~3u) + offset;
    }
    return 1;
}

static int fmt13_sp_offset(ARM7TDMI& cpu, u16 instr) {
    u32 offset = (instr & 0x7F) << 2;
    if (test_bit(instr, 7)) {
        cpu.reg(13) -= offset;
    } else {
        cpu.reg(13) += offset;
    }
    return 1;
}

static int fmt14_push_pop(ARM7TDMI& cpu, u16 instr) {
    bool L = test_bit(instr, 11);
    bool R = test_bit(instr, 8);
    u8 reg_list = instr & 0xFF;
    int count = __builtin_popcount(reg_list) + (R ? 1 : 0);

    int cycles = 1;

    if (L) {
        u32 addr = cpu.reg(13);
        for (int i = 0; i < 8; i++) {
            if (reg_list & (1 << i)) {
                cpu.reg(i) = cpu.read32(addr);
                addr += 4;
                cycles++;
            }
        }
        if (R) {
            u32 val = cpu.read32(addr);

            if (val & 1) {
                cpu.pc() = val & ~1u;
            } else {
                cpu.set_cpsr(cpu.cpsr() & ~(1 << CPSR_T));
                cpu.pc() = val & ~3u;
            }
            addr += 4;
            cpu.flush_pipeline();
            cycles += 3;
        }
        cpu.reg(13) = addr;
    } else {
        u32 addr = cpu.reg(13) - count * 4;
        cpu.reg(13) = addr;
        for (int i = 0; i < 8; i++) {
            if (reg_list & (1 << i)) {
                cpu.write32(addr, cpu.reg(i));
                addr += 4;
                cycles++;
            }
        }
        if (R) {
            cpu.write32(addr, cpu.reg(14));
            addr += 4;
            cycles++;
        }
    }
    return cycles;
}

static int fmt15_multiple(ARM7TDMI& cpu, u16 instr) {
    bool L = test_bit(instr, 11);
    int rb = bits(instr, 10, 8);
    u8 reg_list = instr & 0xFF;

    u32 addr = cpu.reg(rb) & ~3u;
    int cycles = 1;
    bool empty = (reg_list == 0);

    if (empty) {

        if (L) {
            cpu.pc() = cpu.read32(addr);
            cpu.flush_pipeline();
        } else {
            cpu.write32(addr, cpu.pc() + 2);
        }
        cpu.reg(rb) = addr + 0x40;
        return 5;
    }

    bool rb_in_list = reg_list & (1 << rb);
    u32 old_base = addr;
    u32 new_base = addr + __builtin_popcount(reg_list) * 4;

    if (L) {
        for (int i = 0; i < 8; i++) {
            if (reg_list & (1 << i)) {
                cpu.reg(i) = cpu.read32(addr);
                addr += 4;
                cycles++;
            }
        }

        if (!rb_in_list) cpu.reg(rb) = new_base;
    } else {
        for (int i = 0; i < 8; i++) {
            if (reg_list & (1 << i)) {

                cpu.write32(addr, (i == rb) ? old_base : cpu.reg(i));
                addr += 4;
                cycles++;
            }
        }
        cpu.reg(rb) = new_base;
    }
    return cycles;
}

static int fmt16_cond_branch(ARM7TDMI& cpu, u16 instr) {
    u32 cond = bits(instr, 11, 8);
    if (!cpu.check_condition(cond)) return 1;

    s32 offset = (s32)(s8)(instr & 0xFF) << 1;

    cpu.pc() += offset + 2;
    cpu.flush_pipeline();
    return 3;
}

static int fmt17_swi(ARM7TDMI& cpu, u16 instr) {
    cpu.raise_exception(VECTOR_SWI, CpuMode::Supervisor);
    return 3;
}

static int fmt18_branch(ARM7TDMI& cpu, u16 instr) {
    s32 offset = instr & 0x7FF;
    if (offset & 0x400) offset |= 0xFFFFF800;
    offset <<= 1;

    cpu.pc() += offset + 2;
    cpu.flush_pipeline();
    return 3;
}

static int fmt19_long_branch(ARM7TDMI& cpu, u16 instr) {
    bool H = test_bit(instr, 11);
    u32 offset = instr & 0x7FF;

    if (!H) {

        s32 soff = offset;
        if (soff & 0x400) soff |= 0xFFFFF800;
        cpu.reg(14) = (cpu.pc() + 2) + (soff << 12);
        return 1;
    } else {

        u32 next_pc = cpu.reg(14) + (offset << 1);
        cpu.reg(14) = cpu.pc() | 1;
        cpu.pc() = next_pc & ~1u;
        cpu.flush_pipeline();
        return 3;
    }
}

int thumb::execute(ARM7TDMI& cpu, u16 instr) {

    u16 top = instr >> 8;

    if ((instr >> 11) < 3) return fmt1_shift(cpu, instr);

    if ((instr >> 11) == 3) return fmt2_add_sub(cpu, instr);

    if ((instr >> 13) == 1) return fmt3_imm(cpu, instr);

    if ((instr >> 10) == 0x10) return fmt4_alu(cpu, instr);

    if ((instr >> 10) == 0x11) return fmt5_hireg(cpu, instr);

    if ((instr >> 11) == 9) return fmt6_pc_load(cpu, instr);

    if ((instr >> 12) == 5 && !test_bit(instr, 9)) return fmt7_reg_offset(cpu, instr);

    if ((instr >> 12) == 5 && test_bit(instr, 9)) return fmt8_sign_ext(cpu, instr);

    if ((instr >> 13) == 3) return fmt9_imm_offset(cpu, instr);

    if ((instr >> 12) == 8) return fmt10_halfword(cpu, instr);

    if ((instr >> 12) == 9) return fmt11_sp_relative(cpu, instr);

    if ((instr >> 12) == 0xA) return fmt12_load_addr(cpu, instr);

    if ((top & 0xFF) == 0xB0) return fmt13_sp_offset(cpu, instr);

    if ((instr >> 12) == 0xB && (bits(instr, 11, 9) == 2 || bits(instr, 11, 9) == 6))
        return fmt14_push_pop(cpu, instr);

    if ((instr >> 12) == 0xC) return fmt15_multiple(cpu, instr);

    if ((instr >> 12) == 0xD && bits(instr, 11, 8) < 0xE) return fmt16_cond_branch(cpu, instr);

    if ((top & 0xFF) == 0xDF) return fmt17_swi(cpu, instr);

    if ((instr >> 11) == 0x1C) return fmt18_branch(cpu, instr);

    if ((instr >> 12) == 0xF || (instr >> 11) == 0x1E) return fmt19_long_branch(cpu, instr);

    return 1;
}
