// APU mixing, square/wave/noise channels, FIFO playback
#include "apu/apu.hpp"
#include "memory/bus.hpp"
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
            ch1_.volume = (val >> 12) & 0xF;
            ch1_.envelope_period = (val >> 8) & 7;
            break;
        case 0x064:
            ch1_.frequency = val & 0x7FF;
            if (val & (1 << 15)) { ch1_.enabled = true; ch1_.duty_pos = 0; }
            ch1_.length_enabled = val & (1 << 14);
            break;

        case 0x068:
            ch2_.duty = (val >> 6) & 3;
            ch2_.volume = (val >> 12) & 0xF;
            ch2_.envelope_period = (val >> 8) & 7;
            break;
        case 0x06C:
            ch2_.frequency = val & 0x7FF;
            if (val & (1 << 15)) { ch2_.enabled = true; ch2_.duty_pos = 0; }
            ch2_.length_enabled = val & (1 << 14);
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
            if (val & (1 << 15)) { ch3_.enabled = true; ch3_.pos = 0; }
            break;

        case 0x078:
            ch4_.volume = (val >> 12) & 0xF;
            ch4_.envelope_period = (val >> 8) & 7;
            break;
        case 0x07C:
            ch4_.width_7 = val & (1 << 3);
            if (val & (1 << 15)) { ch4_.enabled = true; ch4_.lfsr = 0x7FFF; }
            break;

        case 0x080:
            break;

        case 0x082:
            fifo_a_.volume = (val >> 2) & 1;
            fifo_b_.volume = (val >> 3) & 1;
            fifo_a_.enabled = (val & 0x300) != 0;
            fifo_b_.enabled = (val & 0xC00) != 0;
            fifo_a_.timer_id = (val >> 10) & 1;
            fifo_b_.timer_id = (val >> 14) & 1;
            if (val & (1 << 11)) fifo_a_.reset();
            if (val & (1 << 15)) fifo_b_.reset();
            break;

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

        if (fifo_a_.size <= 16) {

        }
    }
    if (fifo_b_.timer_id == timer_id && fifo_b_.enabled) {
        fifo_b_.pop();
    }
}

void APU::tick(int cycles) {
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

    s32 legacy = 0;
    legacy += ch1_.sample();
    legacy += ch2_.sample();
    legacy += ch3_.sample();
    legacy += ch4_.sample();

    ch1_.duty_pos++;
    ch2_.duty_pos++;
    ch3_.pos++;

    if (ch4_.enabled) {
        u16 feedback = ch4_.lfsr;
        ch4_.lfsr >>= 1;
        if (feedback & 1) {
            ch4_.lfsr ^= ch4_.width_7 ? 0x60 : 0x6000;
        }
    }

    s32 dma_l = 0, dma_r = 0;
    if (fifo_a_.enabled) {
        s32 a = fifo_a_.current_sample * (fifo_a_.volume ? 4 : 2);
        dma_l += a;
        dma_r += a;
    }
    if (fifo_b_.enabled) {
        s32 b = fifo_b_.current_sample * (fifo_b_.volume ? 4 : 2);
        dma_l += b;
        dma_r += b;
    }

    s32 left = legacy * 2 + dma_l;
    s32 right = legacy * 2 + dma_r;

    left = std::clamp(left * 64, -32768, 32767);
    right = std::clamp(right * 64, -32768, 32767);

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
