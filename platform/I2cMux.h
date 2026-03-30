#pragma once
// I2cMux.h — minimal TCA9548A helper for Alfred I2C topology.

#include <cstdint>

namespace sunray::platform {

class I2C;

class I2cMux {
public:
    I2cMux(I2C& bus, uint8_t addr);

    bool getEnabledMask(uint8_t& mask);
    bool setEnabledMask(uint8_t mask);
    bool enableChannel(uint8_t channel);   // additive: keeps other channels active
    bool selectChannel(uint8_t channel);   // exclusive: disables all other channels

private:
    I2C&    bus_;
    uint8_t addr_;
};

} // namespace sunray::platform
