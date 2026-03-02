// 4-channel DMA controller
#pragma once
#include "types.hpp"

class Bus;
class InterruptController;

enum class DmaTiming {
    Immediate = 0,
    VBlank    = 1,
    HBlank    = 2,
    Special   = 3
};

class DMA {
public:
    void set_bus(Bus* bus) { bus_ = bus; }
    void set_interrupt(InterruptController* ic) { interrupt_ = ic; }

    void write_register(u32 reg, u16 val);

    void trigger(DmaTiming timing);

    void check_immediate();

    bool active() const { return active_; }

private:
    Bus* bus_ = nullptr;
    InterruptController* interrupt_ = nullptr;

    struct Channel {
        u32 src = 0;
        u32 dst = 0;
        u32 count = 0;
        u16 control = 0;

        u32 internal_src = 0;
        u32 internal_dst = 0;
        u32 internal_count = 0;
        bool enabled = false;
    };

    Channel channels_[4]{};
    bool active_ = false;

    void run_channel(int ch);
};
