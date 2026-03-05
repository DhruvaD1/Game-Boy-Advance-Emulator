// real-time clock (GPIO-based, for Pokemon)
#pragma once
#include "types.hpp"
#include <cstdio>

class RTC {
public:
    u8 read(u32 offset);
    void write(u32 offset, u8 val);

    bool save_state(FILE* f) const;
    bool load_state(FILE* f);

private:
    u8 data_reg_ = 0;
    u8 direction_reg_ = 0;
    u8 control_reg_ = 1;

    enum class State {
        Idle,
        Command,
        Reading,
        Writing
    };

    State state_ = State::Idle;
    int bit_count_ = 0;
    u8 command_ = 0;
    u8 serial_data_[8]{};
    int data_idx_ = 0;
    int data_len_ = 0;
    bool cs_last_ = false;
    bool sck_last_ = false;

    void process_bit(bool bit_val);
    void execute_command();
    u8 to_bcd(u8 val);
};
