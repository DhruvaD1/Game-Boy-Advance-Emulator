// IRQ request/acknowledge
#include "interrupt/interrupt.hpp"
#include "memory/bus.hpp"
#include <cstring>

void InterruptController::request_irq(u16 flag) {
    u16 if_reg;
    memcpy(&if_reg, &bus_->io[0x202], 2);
    if_reg |= flag;
    memcpy(&bus_->io[0x202], &if_reg, 2);
}

bool InterruptController::irq_pending() const {
    u16 ie, if_reg, ime;
    memcpy(&ie, &bus_->io[0x200], 2);
    memcpy(&if_reg, &bus_->io[0x202], 2);
    memcpy(&ime, &bus_->io[0x208], 2);
    return ime && (ie & if_reg);
}
