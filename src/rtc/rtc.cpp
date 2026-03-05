// RTC serial protocol, maps to host system time
#include "rtc/rtc.hpp"
#include <ctime>

u8 RTC::to_bcd(u8 val) {
    return ((val / 10) << 4) | (val % 10);
}

u8 RTC::read(u32 offset) {
    switch (offset) {
        case 0xC4: return data_reg_;
        case 0xC6: return direction_reg_;
        case 0xC8: return control_reg_;
        default: return 0;
    }
}

void RTC::write(u32 offset, u8 val) {
    switch (offset) {
        case 0xC4: {
            u8 old = data_reg_;
            data_reg_ = val;

            bool cs = (val >> 2) & 1;
            bool sck = val & 1;
            bool sio = (val >> 1) & 1;

            if (cs && !cs_last_) {
                state_ = State::Command;
                bit_count_ = 0;
                command_ = 0;
                data_idx_ = 0;
            }

            if (!cs) {
                state_ = State::Idle;
            }

            if (cs && sck && !sck_last_) {
                if (direction_reg_ & 2) {

                    process_bit(sio);
                } else {

                    if (state_ == State::Reading && data_idx_ < data_len_) {
                        bool out_bit = (serial_data_[data_idx_] >> (bit_count_ % 8)) & 1;
                        data_reg_ = (data_reg_ & ~2) | (out_bit ? 2 : 0);
                        bit_count_++;
                        if (bit_count_ % 8 == 0) data_idx_++;
                    }
                }
            }

            cs_last_ = cs;
            sck_last_ = sck;
            break;
        }
        case 0xC6:
            direction_reg_ = val;
            break;
        case 0xC8:
            control_reg_ = val;
            break;
    }
}

void RTC::process_bit(bool bit_val) {
    if (state_ == State::Command) {
        command_ |= (bit_val ? 1 : 0) << bit_count_;
        bit_count_++;

        if (bit_count_ == 8) {
            execute_command();
            bit_count_ = 0;
        }
    } else if (state_ == State::Writing) {
        if (data_idx_ < data_len_) {
            serial_data_[data_idx_] |= (bit_val ? 1 : 0) << (bit_count_ % 8);
            bit_count_++;
            if (bit_count_ % 8 == 0) data_idx_++;
        }
    }
}

void RTC::execute_command() {

    u8 cmd = 0;
    for (int i = 0; i < 8; i++) {
        cmd |= ((command_ >> i) & 1) << (7 - i);
    }

    u8 command_id = (cmd >> 1) & 7;
    bool reading = cmd & 1;

    switch (command_id) {
        case 0:
            state_ = State::Idle;
            break;
        case 1:
            data_len_ = 1;
            if (reading) {
                serial_data_[0] = 0x40;
                state_ = State::Reading;
            } else {
                serial_data_[0] = 0;
                state_ = State::Writing;
            }
            break;
        case 2:
        case 3:
        {
            data_len_ = 7;
            if (reading) {
                time_t now = time(nullptr);
                struct tm* t = localtime(&now);

                serial_data_[0] = to_bcd(t->tm_year % 100);
                serial_data_[1] = to_bcd(t->tm_mon + 1);
                serial_data_[2] = to_bcd(t->tm_mday);
                serial_data_[3] = to_bcd(t->tm_wday);
                serial_data_[4] = to_bcd(t->tm_hour);
                serial_data_[5] = to_bcd(t->tm_min);
                serial_data_[6] = to_bcd(t->tm_sec);

                state_ = State::Reading;
            } else {
                for (int i = 0; i < 7; i++) serial_data_[i] = 0;
                state_ = State::Writing;
            }
            break;
        }
        case 4:
        {
            data_len_ = 3;
            if (reading) {
                time_t now = time(nullptr);
                struct tm* t = localtime(&now);
                serial_data_[0] = to_bcd(t->tm_hour);
                serial_data_[1] = to_bcd(t->tm_min);
                serial_data_[2] = to_bcd(t->tm_sec);
                state_ = State::Reading;
            } else {
                for (int i = 0; i < 3; i++) serial_data_[i] = 0;
                state_ = State::Writing;
            }
            break;
        }
        default:
            state_ = State::Idle;
            break;
    }
}

bool RTC::save_state(FILE* f) const {
    if (fwrite(&data_reg_, sizeof(data_reg_), 1, f) != 1) return false;
    if (fwrite(&direction_reg_, sizeof(direction_reg_), 1, f) != 1) return false;
    if (fwrite(&control_reg_, sizeof(control_reg_), 1, f) != 1) return false;
    int state_int = static_cast<int>(state_);
    if (fwrite(&state_int, sizeof(state_int), 1, f) != 1) return false;
    if (fwrite(&bit_count_, sizeof(bit_count_), 1, f) != 1) return false;
    if (fwrite(&command_, sizeof(command_), 1, f) != 1) return false;
    if (fwrite(serial_data_, sizeof(serial_data_), 1, f) != 1) return false;
    if (fwrite(&data_idx_, sizeof(data_idx_), 1, f) != 1) return false;
    if (fwrite(&data_len_, sizeof(data_len_), 1, f) != 1) return false;
    if (fwrite(&cs_last_, sizeof(cs_last_), 1, f) != 1) return false;
    if (fwrite(&sck_last_, sizeof(sck_last_), 1, f) != 1) return false;
    return true;
}

bool RTC::load_state(FILE* f) {
    if (fread(&data_reg_, sizeof(data_reg_), 1, f) != 1) return false;
    if (fread(&direction_reg_, sizeof(direction_reg_), 1, f) != 1) return false;
    if (fread(&control_reg_, sizeof(control_reg_), 1, f) != 1) return false;
    int state_int;
    if (fread(&state_int, sizeof(state_int), 1, f) != 1) return false;
    state_ = static_cast<State>(state_int);
    if (fread(&bit_count_, sizeof(bit_count_), 1, f) != 1) return false;
    if (fread(&command_, sizeof(command_), 1, f) != 1) return false;
    if (fread(serial_data_, sizeof(serial_data_), 1, f) != 1) return false;
    if (fread(&data_idx_, sizeof(data_idx_), 1, f) != 1) return false;
    if (fread(&data_len_, sizeof(data_len_), 1, f) != 1) return false;
    if (fread(&cs_last_, sizeof(cs_last_), 1, f) != 1) return false;
    if (fread(&sck_last_, sizeof(sck_last_), 1, f) != 1) return false;
    return true;
}
