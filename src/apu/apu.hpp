// APU — sound channels and DMA audio
#pragma once
#include "types.hpp"
#include <array>
#include <SDL.h>

class Bus;
class DMA;

class APU {
public:
    APU();
    ~APU();

    void set_bus(Bus* bus) { bus_ = bus; }
    void set_dma(DMA* dma) { dma_ = dma; }

    void init_audio();
    void shutdown_audio();

    void write_register(u32 reg, u16 val);
    void timer_overflow(int timer_id);
    void tick(int cycles);

    void fill_buffer(s16* buffer, int samples);

    bool save_state(FILE* f) const;
    bool load_state(FILE* f);

private:
    Bus* bus_ = nullptr;
    DMA* dma_ = nullptr;
    SDL_AudioDeviceID audio_device_ = 0;

    struct FifoChannel {
        std::array<s8, 32> fifo{};
        int read_pos = 0;
        int write_pos = 0;
        int size = 0;
        s8 current_sample = 0;
        int timer_id = 0;
        bool enabled = false;
        bool enable_left = false;
        bool enable_right = false;
        int volume = 0;

        void push(s8 sample) {
            if (size < 32) {
                fifo[write_pos] = sample;
                write_pos = (write_pos + 1) % 32;
                size++;
            }
        }

        s8 pop() {
            if (size > 0) {
                s8 val = fifo[read_pos];
                read_pos = (read_pos + 1) % 32;
                size--;
                current_sample = val;
                return val;
            }
            return current_sample;
        }

        void reset() {
            read_pos = write_pos = size = 0;
            current_sample = 0;
        }
    };

    FifoChannel fifo_a_, fifo_b_;

    struct SquareChannel {
        bool enabled = false;
        int frequency = 0;
        int duty = 0;
        int volume = 0;
        int envelope_period = 0;
        int envelope_dir = 0;
        int sweep_period = 0;
        int timer = 0;
        int duty_pos = 0;
        int length_counter = 0;
        bool length_enabled = false;

        s8 sample() const {
            if (!enabled || volume == 0) return 0;
            static const u8 duty_table[4][8] = {
                {0,0,0,0,0,0,0,1},
                {1,0,0,0,0,0,0,1},
                {1,0,0,0,0,1,1,1},
                {0,1,1,1,1,1,1,0}
            };
            return duty_table[duty & 3][duty_pos & 7] ? volume : -volume;
        }
    };

    struct WaveChannel {
        bool enabled = false;
        int frequency = 0;
        int volume_shift = 0;
        int pos = 0;
        int timer = 0;
        std::array<u8, 32> wave_ram{};

        s8 sample() const {
            if (!enabled) return 0;
            u8 val = wave_ram[pos & 31];
            int shifted = val >> volume_shift;
            return (s8)(shifted - 8);
        }
    };

    struct NoiseChannel {
        bool enabled = false;
        int volume = 0;
        int envelope_period = 0;
        int envelope_dir = 0;
        u16 lfsr = 0x7FFF;
        int timer = 0;
        bool width_7 = false;
        int dividing_ratio = 0;
        int shift_clock = 0;

        s8 sample() const {
            if (!enabled) return 0;
            return (lfsr & 1) ? -volume : volume;
        }
    };

    SquareChannel ch1_, ch2_;
    WaveChannel ch3_;
    NoiseChannel ch4_;


    int frame_seq_counter_ = 0;
    int frame_seq_step_ = 0;
    static constexpr int FRAME_SEQ_PERIOD = CPU_FREQ / 512;


    int master_vol_left_ = 7;
    int master_vol_right_ = 7;
    u8 psg_enable_left_ = 0;
    u8 psg_enable_right_ = 0;


    int psg_volume_shift_ = 2;

    int cycle_counter_ = 0;
    static constexpr int SAMPLE_RATE = 32768;
    static constexpr int CYCLES_PER_SAMPLE = CPU_FREQ / SAMPLE_RATE;

    static constexpr int AUDIO_BUFFER_SIZE = 4096;
    std::array<s16, AUDIO_BUFFER_SIZE * 2> sample_buffer_{};
    int sample_write_pos_ = 0;
    int sample_read_pos_ = 0;

    void tick_channels(int cycles);
    void tick_frame_sequencer(int cycles);
    void mix_sample();

    static void audio_callback(void* userdata, u8* stream, int len);
};
