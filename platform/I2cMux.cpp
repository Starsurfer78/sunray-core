// I2cMux.cpp — minimal TCA9548A helper.

#include "I2cMux.h"

#include "I2C.h"

namespace sunray::platform {

I2cMux::I2cMux(I2C& bus, uint8_t addr)
    : bus_(bus)
    , addr_(addr)
{}

bool I2cMux::getEnabledMask(uint8_t& mask) {
    return bus_.read(addr_, &mask, 1);
}

bool I2cMux::setEnabledMask(uint8_t mask) {
    return bus_.write(addr_, &mask, 1);
}

bool I2cMux::enableChannel(uint8_t channel) {
    if (channel > 7) return false;
    uint8_t mask = 0;
    if (!getEnabledMask(mask)) return false;
    mask = static_cast<uint8_t>(mask | (1u << channel));
    return setEnabledMask(mask);
}

} // namespace sunray::platform
