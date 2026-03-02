// ARM (32-bit) instruction decoder
#pragma once
#include "types.hpp"

class ARM7TDMI;

namespace arm {
    int execute(ARM7TDMI& cpu, u32 instr);
}
