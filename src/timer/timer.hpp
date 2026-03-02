// 4 hardware timers with cascade support
#pragma once
#include "types.hpp"

class Bus;
class InterruptController;
class APU;

class Timer {
public:
    void set_bus(Bus* bus) { bus_ = bus; }
    void set_interrupt(InterruptController* ic) { interrupt_ = ic; }
    void set_apu(APU* apu) { apu_ = apu; }

    void tick(int cycles);
    u16 read(u32 reg);
    void write(u32 reg, u16 val);

private:
    Bus* bus_ = nullptr;
    InterruptController* interrupt_ = nullptr;
    APU* apu_ = nullptr;

    struct TimerState {
        u16 reload = 0;
        u16 counter = 0;
        u16 control = 0;
        int prescaler_counter = 0;
        bool running = false;
        bool cascade = false;
        bool irq = false;
        int prescaler = 1;
    };

    TimerState timers_[4]{};

    void overflow(int idx);
    static int prescaler_value(int sel) {
        static const int vals[] = {1, 64, 256, 1024};
        return vals[sel & 3];
    }
};
