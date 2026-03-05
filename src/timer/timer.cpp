// timer tick, overflow, cascade, IRQ
#include "timer/timer.hpp"
#include "memory/bus.hpp"
#include "interrupt/interrupt.hpp"
#include "apu/apu.hpp"
#include <cstring>

void Timer::tick(int cycles) {
    for (int i = 0; i < 4; i++) {
        auto& t = timers_[i];
        if (!t.running || t.cascade) continue;

        t.prescaler_counter += cycles;
        while (t.prescaler_counter >= t.prescaler) {
            t.prescaler_counter -= t.prescaler;
            t.counter++;
            if (t.counter == 0) {
                overflow(i);
            }
        }
    }
}

void Timer::overflow(int idx) {
    auto& t = timers_[idx];
    t.counter = t.reload;

    if (t.irq && interrupt_) {
        interrupt_->request_irq(IRQ_TIMER0 << idx);
    }

    if (apu_ && (idx == 0 || idx == 1)) {
        apu_->timer_overflow(idx);
    }

    if (idx < 3 && timers_[idx + 1].running && timers_[idx + 1].cascade) {
        timers_[idx + 1].counter++;
        if (timers_[idx + 1].counter == 0) {
            overflow(idx + 1);
        }
    }
}

u16 Timer::read(u32 reg) {
    int idx = (reg - 0x100) / 4;
    bool is_cnt = (reg & 2) != 0;

    if (idx < 0 || idx > 3) return 0;

    if (is_cnt) {
        return timers_[idx].control;
    } else {
        return timers_[idx].counter;
    }
}

void Timer::write(u32 reg, u16 val) {
    int idx = (reg - 0x100) / 4;
    bool is_cnt = (reg & 2) != 0;

    if (idx < 0 || idx > 3) return;

    auto& t = timers_[idx];

    if (is_cnt) {
        bool was_running = t.running;
        t.control = val;
        t.running = val & (1 << 7);
        t.cascade = (idx > 0) && (val & (1 << 2));
        t.irq = val & (1 << 6);
        t.prescaler = prescaler_value(val & 3);

        if (!was_running && t.running) {
            t.counter = t.reload;
            t.prescaler_counter = 0;
        }

        memcpy(&bus_->io[reg], &val, 2);
    } else {
        t.reload = val;
        memcpy(&bus_->io[reg], &val, 2);
    }
}

bool Timer::save_state(FILE* f) const {
    if (fwrite(timers_, sizeof(timers_), 1, f) != 1) return false;
    return true;
}

bool Timer::load_state(FILE* f) {
    if (fread(timers_, sizeof(timers_), 1, f) != 1) return false;
    return true;
}
