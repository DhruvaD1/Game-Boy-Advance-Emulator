// Thumb (16-bit) instruction decoder
#pragma once
#include "types.hpp"

class ARM7TDMI;

namespace thumb {
    int execute(ARM7TDMI& cpu, u16 instr);
}
