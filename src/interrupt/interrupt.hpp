// interrupt controller — IE, IF, IME
#pragma once
#include "types.hpp"

class Bus;

enum IRQ : u16 {
    IRQ_VBLANK  = 1 << 0,
    IRQ_HBLANK  = 1 << 1,
    IRQ_VCOUNT  = 1 << 2,
    IRQ_TIMER0  = 1 << 3,
    IRQ_TIMER1  = 1 << 4,
    IRQ_TIMER2  = 1 << 5,
    IRQ_TIMER3  = 1 << 6,
    IRQ_SERIAL  = 1 << 7,
    IRQ_DMA0    = 1 << 8,
    IRQ_DMA1    = 1 << 9,
    IRQ_DMA2    = 1 << 10,
    IRQ_DMA3    = 1 << 11,
    IRQ_KEYPAD  = 1 << 12,
    IRQ_GAMEPAK = 1 << 13
};

class InterruptController {
public:
    void set_bus(Bus* bus) { bus_ = bus; }

    void request_irq(u16 flag);
    bool irq_pending() const;

private:
    Bus* bus_ = nullptr;
};
