// Flash 128K save emulation (Sanyo LE39FW512)
#pragma once
#include "types.hpp"
#include <array>
#include <string>
#include <cstdio>

class Flash {
public:
    Flash();

    u8 read(u32 addr);
    void write(u32 addr, u8 val);

    void load(const std::string& path);
    void save(const std::string& path);

    void set_save_path(const std::string& path) { save_path_ = path; }

    bool save_state(FILE* f) const;
    bool load_state(FILE* f);

private:
    std::array<u8, FLASH_SIZE> data_{};
    std::string save_path_;

    enum class State {
        Ready,
        Cmd1,
        Cmd2,
        Identify,
        EraseCmd1,
        EraseCmd2,
        Write,
        BankSwitch
    };

    State state_ = State::Ready;
    int current_bank_ = 0;
    bool id_mode_ = false;

    static constexpr u8 MANUFACTURER_ID = 0x62;
    static constexpr u8 DEVICE_ID = 0x13;
};
