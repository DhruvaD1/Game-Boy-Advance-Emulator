// flash command state machine, bank switching, .sav persistence
#include "memory/flash.hpp"
#include <fstream>
#include <cstring>
#include <cstdio>

Flash::Flash() {
    data_.fill(0xFF);
}

u8 Flash::read(u32 addr) {
    if (id_mode_) {
        if (addr == 0x0000) return MANUFACTURER_ID;
        if (addr == 0x0001) return DEVICE_ID;
        return 0;
    }

    u32 offset = current_bank_ * 0x10000 + (addr & 0xFFFF);
    if (offset < FLASH_SIZE) return data_[offset];
    return 0xFF;
}

void Flash::write(u32 addr, u8 val) {
    switch (state_) {
        case State::Ready:
            if (addr == 0x5555 && val == 0xAA) {
                state_ = State::Cmd1;
            }
            break;

        case State::Cmd1:
            if (addr == 0x2AAA && val == 0x55) {
                state_ = State::Cmd2;
            } else {
                state_ = State::Ready;
            }
            break;

        case State::Cmd2:
            if (addr == 0x5555) {
                switch (val) {
                    case 0x90:
                        id_mode_ = true;
                        state_ = State::Ready;
                        break;
                    case 0xF0:
                        id_mode_ = false;
                        state_ = State::Ready;
                        break;
                    case 0x80:
                        state_ = State::EraseCmd1;
                        break;
                    case 0xA0:
                        state_ = State::Write;
                        break;
                    case 0xB0:
                        state_ = State::BankSwitch;
                        break;
                    default:
                        state_ = State::Ready;
                        break;
                }
            } else {
                state_ = State::Ready;
            }
            break;

        case State::EraseCmd1:
            if (addr == 0x5555 && val == 0xAA) {
                state_ = State::EraseCmd2;
            } else {
                state_ = State::Ready;
            }
            break;

        case State::EraseCmd2:
            if (addr == 0x2AAA && val == 0x55) {
                state_ = State::Identify;
            } else {
                state_ = State::Ready;
            }
            break;

        case State::Identify:
            if (addr == 0x5555 && val == 0x10) {

                data_.fill(0xFF);
                printf("Flash: chip erase\n");
            } else if (val == 0x30) {

                u32 sector = current_bank_ * 0x10000 + (addr & 0xF000);
                if (sector + 0x1000 <= FLASH_SIZE) {
                    memset(&data_[sector], 0xFF, 0x1000);
                }
            }
            state_ = State::Ready;
            break;

        case State::Write: {
            u32 offset = current_bank_ * 0x10000 + (addr & 0xFFFF);
            if (offset < FLASH_SIZE) {
                data_[offset] &= val;
            }
            state_ = State::Ready;

            if (!save_path_.empty()) save(save_path_);
            break;
        }

        case State::BankSwitch:
            if (addr == 0x0000) {
                current_bank_ = val & 1;
            }
            state_ = State::Ready;
            break;
    }
}

void Flash::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(data_.data()), FLASH_SIZE);
        printf("Flash: loaded save from %s\n", path.c_str());
    }
}

void Flash::save(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(data_.data()), FLASH_SIZE);
    }
}

bool Flash::save_state(FILE* f) const {
    if (fwrite(data_.data(), sizeof(data_), 1, f) != 1) return false;
    int state_int = static_cast<int>(state_);
    if (fwrite(&state_int, sizeof(state_int), 1, f) != 1) return false;
    if (fwrite(&current_bank_, sizeof(current_bank_), 1, f) != 1) return false;
    if (fwrite(&id_mode_, sizeof(id_mode_), 1, f) != 1) return false;
    return true;
}

bool Flash::load_state(FILE* f) {
    if (fread(data_.data(), sizeof(data_), 1, f) != 1) return false;
    int state_int;
    if (fread(&state_int, sizeof(state_int), 1, f) != 1) return false;
    state_ = static_cast<State>(state_int);
    if (fread(&current_bank_, sizeof(current_bank_), 1, f) != 1) return false;
    if (fread(&id_mode_, sizeof(id_mode_), 1, f) != 1) return false;
    return true;
}
