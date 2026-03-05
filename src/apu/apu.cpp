// APU mixing, square/wave/noise channels, FIFO playback
#include "apu/apu.hpp"
#include "memory/bus.hpp"
#include "dma/dma.hpp"
#include <cstring>
#include <algorithm>
#include <cstdio>

APU::APU() {}
APU::~APU() { shutdown_audio(); }

void APU::init_audio() {
    SDL_AudioSpec want{}, have{};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = this;

    audio_device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio_device_) {
        SDL_PauseAudioDevice(audio_device_, 0);
    }
}

void APU::shutdown_audio() {
    if (audio_device_) {
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
    }
}

void APU::write_register(u32 reg, u16 val) {
    switch (reg) {

        case 0x060:
            ch1_.sweep_period = (val >> 4) & 7;
            break;
        case 0x062:
            ch1_.duty = (val >> 6) & 3;
            ch1_.length_counter = 64 - (val & 0x3F);
            ch1_.envelope_dir = (val >> 11) & 1;
            ch1_.volume = (val >> 12) & 0xF;
            ch1_.envelope_period = (val >> 8) & 7;
            break;
        case 0x064:
            ch1_.frequency = val & 0x7FF;
            ch1_.length_enabled = val & (1 << 14);
            if (val & (1 << 15)) {
                ch1_.enabled = true;
                ch1_.duty_pos = 0;
                ch1_.timer = (2048 - ch1_.frequency) * 16;
            }
            break;

        case 0x068:
            ch2_.duty = (val >> 6) & 3;
            ch2_.length_counter = 64 - (val & 0x3F);
            ch2_.envelope_dir = (val >> 11) & 1;
            ch2_.volume = (val >> 12) & 0xF;
            ch2_.envelope_period = (val >> 8) & 7;
            break;
        case 0x06C:
            ch2_.frequency = val & 0x7FF;
            ch2_.length_enabled = val & (1 << 14);
            if (val & (1 << 15)) {
                ch2_.enabled = true;
                ch2_.duty_pos = 0;
                ch2_.timer = (2048 - ch2_.frequency) * 16;
            }
            break;

        case 0x070:
            ch3_.enabled = val & (1 << 7);
            break;
        case 0x072:
            ch3_.volume_shift = ((val >> 13) & 3);
            if (ch3_.volume_shift == 0) ch3_.volume_shift = 4;
            else ch3_.volume_shift--;
            break;
        case 0x074:
            ch3_.frequency = val & 0x7FF;
            if (val & (1 << 15)) {
                ch3_.enabled = true;
                ch3_.pos = 0;
                ch3_.timer = (2048 - ch3_.frequency) * 8;
            }
            break;

        case 0x078:
            ch4_.envelope_dir = (val >> 11) & 1;
            ch4_.volume = (val >> 12) & 0xF;
            ch4_.envelope_period = (val >> 8) & 7;
            break;
        case 0x07C:
            ch4_.dividing_ratio = val & 7;
            ch4_.width_7 = val & (1 << 3);
            ch4_.shift_clock = (val >> 4) & 0xF;
            if (val & (1 << 15)) {
                ch4_.enabled = true;
                ch4_.lfsr = ch4_.width_7 ? 0x7F : 0x7FFF;
                int r = ch4_.dividing_ratio;
                ch4_.timer = (r == 0 ? 8 : 16 * r) << ch4_.shift_clock;
            }
            break;

        case 0x080:
            master_vol_right_ = val & 7;
            master_vol_left_ = (val >> 4) & 7;
            psg_enable_right_ = (val >> 8) & 0xF;
            psg_enable_left_ = (val >> 12) & 0xF;
            break;

        case 0x082:
        {
            psg_volume_shift_ = val & 3;

            fifo_a_.volume = (val >> 2) & 1;
            fifo_a_.enable_right = val & (1 << 8);
            fifo_a_.enable_left = val & (1 << 9);
            fifo_a_.enabled = fifo_a_.enable_right || fifo_a_.enable_left;
            fifo_a_.timer_id = (val >> 10) & 1;
            if (val & (1 << 11)) fifo_a_.reset();

            fifo_b_.volume = (val >> 3) & 1;
            fifo_b_.enable_right = val & (1 << 12);
            fifo_b_.enable_left = val & (1 << 13);
            fifo_b_.enabled = fifo_b_.enable_right || fifo_b_.enable_left;
            fifo_b_.timer_id = (val >> 14) & 1;
            if (val & (1 << 15)) fifo_b_.reset();
            break;
        }

        case 0x084:
            if (!(val & (1 << 7))) {
                ch1_.enabled = ch2_.enabled = ch3_.enabled = ch4_.enabled = false;
            }
            break;

        case 0x090: case 0x092: case 0x094: case 0x096:
        case 0x098: case 0x09A: case 0x09C: case 0x09E:
        {
            int idx = (reg - 0x090) * 2;
            if (idx + 1 < 32) {
                ch3_.wave_ram[idx] = (val >> 4) & 0xF;
                ch3_.wave_ram[idx + 1] = val & 0xF;
            }
            break;
        }

        case 0x0A0: case 0x0A2:
            fifo_a_.push((s8)(val & 0xFF));
            fifo_a_.push((s8)((val >> 8) & 0xFF));
            break;

        case 0x0A4: case 0x0A6:
            fifo_b_.push((s8)(val & 0xFF));
            fifo_b_.push((s8)((val >> 8) & 0xFF));
            break;
    }
}

void APU::timer_overflow(int timer_id) {
    if (fifo_a_.timer_id == timer_id && fifo_a_.enabled) {
        fifo_a_.pop();
        if (fifo_a_.size <= 16 && dma_) {
            dma_->trigger_fifo(0);
        }
    }
    if (fifo_b_.timer_id == timer_id && fifo_b_.enabled) {
        fifo_b_.pop();
        if (fifo_b_.size <= 16 && dma_) {
            dma_->trigger_fifo(1);
        }
    }
}

void APU::tick_channels(int cycles) {
    if (ch1_.enabled) {
        ch1_.timer -= cycles;
        while (ch1_.timer <= 0) {
            ch1_.timer += (2048 - ch1_.frequency) * 16;
            ch1_.duty_pos = (ch1_.duty_pos + 1) & 7;
        }
    }

    if (ch2_.enabled) {
        ch2_.timer -= cycles;
        while (ch2_.timer <= 0) {
            ch2_.timer += (2048 - ch2_.frequency) * 16;
            ch2_.duty_pos = (ch2_.duty_pos + 1) & 7;
        }
    }

    if (ch3_.enabled) {
        ch3_.timer -= cycles;
        while (ch3_.timer <= 0) {
            ch3_.timer += (2048 - ch3_.frequency) * 8;
            ch3_.pos = (ch3_.pos + 1) & 31;
        }
    }

    if (ch4_.enabled) {
        ch4_.timer -= cycles;
        while (ch4_.timer <= 0) {
            int r = ch4_.dividing_ratio;
            ch4_.timer += (r == 0 ? 8 : 16 * r) << ch4_.shift_clock;

            u16 xor_bit = (ch4_.lfsr ^ (ch4_.lfsr >> 1)) & 1;
            ch4_.lfsr >>= 1;
            if (xor_bit) {
                ch4_.lfsr |= ch4_.width_7 ? 0x40 : 0x4000;
            }
        }
    }
}

void APU::tick_frame_sequencer(int cycles) {
    frame_seq_counter_ += cycles;
    while (frame_seq_counter_ >= FRAME_SEQ_PERIOD) {
        frame_seq_counter_ -= FRAME_SEQ_PERIOD;

        if ((frame_seq_step_ & 1) == 0) {
            if (ch1_.length_enabled && ch1_.length_counter > 0) {
                ch1_.length_counter--;
                if (ch1_.length_counter == 0) ch1_.enabled = false;
            }
            if (ch2_.length_enabled && ch2_.length_counter > 0) {
                ch2_.length_counter--;
                if (ch2_.length_counter == 0) ch2_.enabled = false;
            }
        }

        if (frame_seq_step_ == 7) {
            if (ch1_.envelope_period > 0) {
                if (ch1_.envelope_dir) {
                    if (ch1_.volume < 15) ch1_.volume++;
                } else {
                    if (ch1_.volume > 0) ch1_.volume--;
                }
            }
            if (ch2_.envelope_period > 0) {
                if (ch2_.envelope_dir) {
                    if (ch2_.volume < 15) ch2_.volume++;
                } else {
                    if (ch2_.volume > 0) ch2_.volume--;
                }
            }
            if (ch4_.envelope_period > 0) {
                if (ch4_.envelope_dir) {
                    if (ch4_.volume < 15) ch4_.volume++;
                } else {
                    if (ch4_.volume > 0) ch4_.volume--;
                }
            }
        }

        frame_seq_step_ = (frame_seq_step_ + 1) & 7;
    }
}

void APU::tick(int cycles) {
    tick_channels(cycles);
    tick_frame_sequencer(cycles);

    cycle_counter_ += cycles;
    while (cycle_counter_ >= CYCLES_PER_SAMPLE) {
        cycle_counter_ -= CYCLES_PER_SAMPLE;
        mix_sample();
    }
}

void APU::mix_sample() {
    u16 soundcnt_x;
    if (bus_) memcpy(&soundcnt_x, &bus_->io[0x084], 2);
    else soundcnt_x = 0;

    if (!(soundcnt_x & (1 << 7))) {
        int pos = sample_write_pos_ * 2;
        sample_buffer_[pos % (AUDIO_BUFFER_SIZE * 2)] = 0;
        sample_buffer_[(pos + 1) % (AUDIO_BUFFER_SIZE * 2)] = 0;
        sample_write_pos_ = (sample_write_pos_ + 1) % AUDIO_BUFFER_SIZE;
        return;
    }

    s32 psg_left = 0, psg_right = 0;

    s8 ch1_out = ch1_.sample();
    s8 ch2_out = ch2_.sample();
    s8 ch3_out = ch3_.sample();
    s8 ch4_out = ch4_.sample();

    if (psg_enable_left_ & 1) psg_left += ch1_out;
    if (psg_enable_left_ & 2) psg_left += ch2_out;
    if (psg_enable_left_ & 4) psg_left += ch3_out;
    if (psg_enable_left_ & 8) psg_left += ch4_out;

    if (psg_enable_right_ & 1) psg_right += ch1_out;
    if (psg_enable_right_ & 2) psg_right += ch2_out;
    if (psg_enable_right_ & 4) psg_right += ch3_out;
    if (psg_enable_right_ & 8) psg_right += ch4_out;

    psg_left = psg_left * (master_vol_left_ + 1);
    psg_right = psg_right * (master_vol_right_ + 1);

    if (psg_volume_shift_ < 2) {
        psg_left >>= (2 - psg_volume_shift_);
        psg_right >>= (2 - psg_volume_shift_);
    }

    s32 dma_left = 0, dma_right = 0;
    if (fifo_a_.enabled) {
        s32 a = fifo_a_.current_sample * (fifo_a_.volume ? 4 : 2);
        if (fifo_a_.enable_left)  dma_left += a;
        if (fifo_a_.enable_right) dma_right += a;
    }
    if (fifo_b_.enabled) {
        s32 b = fifo_b_.current_sample * (fifo_b_.volume ? 4 : 2);
        if (fifo_b_.enable_left)  dma_left += b;
        if (fifo_b_.enable_right) dma_right += b;
    }

    s32 left  = psg_left + dma_left * 2;
    s32 right = psg_right + dma_right * 2;

    left = std::clamp(left * 32, -32768, 32767);
    right = std::clamp(right * 32, -32768, 32767);

    int pos = (sample_write_pos_ * 2) % (AUDIO_BUFFER_SIZE * 2);
    sample_buffer_[pos] = (s16)left;
    sample_buffer_[pos + 1] = (s16)right;
    sample_write_pos_ = (sample_write_pos_ + 1) % AUDIO_BUFFER_SIZE;
}

void APU::fill_buffer(s16* buffer, int samples) {
    for (int i = 0; i < samples; i++) {
        int pos = (sample_read_pos_ * 2) % (AUDIO_BUFFER_SIZE * 2);
        buffer[i * 2] = sample_buffer_[pos];
        buffer[i * 2 + 1] = sample_buffer_[pos + 1];
        if (sample_read_pos_ != sample_write_pos_) {
            sample_read_pos_ = (sample_read_pos_ + 1) % AUDIO_BUFFER_SIZE;
        }
    }
}

void APU::audio_callback(void* userdata, u8* stream, int len) {
    APU* apu = static_cast<APU*>(userdata);
    int samples = len / 4;
    apu->fill_buffer(reinterpret_cast<s16*>(stream), samples);
}

bool APU::save_state(FILE* f) const {
    if (fwrite(&fifo_a_, sizeof(fifo_a_), 1, f) != 1) return false;
    if (fwrite(&fifo_b_, sizeof(fifo_b_), 1, f) != 1) return false;
    if (fwrite(&ch1_, sizeof(ch1_), 1, f) != 1) return false;
    if (fwrite(&ch2_, sizeof(ch2_), 1, f) != 1) return false;
    if (fwrite(&ch3_, sizeof(ch3_), 1, f) != 1) return false;
    if (fwrite(&ch4_, sizeof(ch4_), 1, f) != 1) return false;
    if (fwrite(&frame_seq_counter_, sizeof(frame_seq_counter_), 1, f) != 1) return false;
    if (fwrite(&frame_seq_step_, sizeof(frame_seq_step_), 1, f) != 1) return false;
    if (fwrite(&master_vol_left_, sizeof(master_vol_left_), 1, f) != 1) return false;
    if (fwrite(&master_vol_right_, sizeof(master_vol_right_), 1, f) != 1) return false;
    if (fwrite(&psg_enable_left_, sizeof(psg_enable_left_), 1, f) != 1) return false;
    if (fwrite(&psg_enable_right_, sizeof(psg_enable_right_), 1, f) != 1) return false;
    if (fwrite(&psg_volume_shift_, sizeof(psg_volume_shift_), 1, f) != 1) return false;
    if (fwrite(&cycle_counter_, sizeof(cycle_counter_), 1, f) != 1) return false;
    return true;
}

bool APU::load_state(FILE* f) {
    SDL_LockAudioDevice(audio_device_);

    if (fread(&fifo_a_, sizeof(fifo_a_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&fifo_b_, sizeof(fifo_b_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&ch1_, sizeof(ch1_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&ch2_, sizeof(ch2_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&ch3_, sizeof(ch3_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&ch4_, sizeof(ch4_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&frame_seq_counter_, sizeof(frame_seq_counter_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&frame_seq_step_, sizeof(frame_seq_step_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&master_vol_left_, sizeof(master_vol_left_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&master_vol_right_, sizeof(master_vol_right_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&psg_enable_left_, sizeof(psg_enable_left_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&psg_enable_right_, sizeof(psg_enable_right_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&psg_volume_shift_, sizeof(psg_volume_shift_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }
    if (fread(&cycle_counter_, sizeof(cycle_counter_), 1, f) != 1) { SDL_UnlockAudioDevice(audio_device_); return false; }

    sample_write_pos_ = 0;
    sample_read_pos_ = 0;
    sample_buffer_.fill(0);

    SDL_UnlockAudioDevice(audio_device_);
    return true;
}
