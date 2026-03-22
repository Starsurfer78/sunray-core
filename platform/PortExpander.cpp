// PortExpander.cpp — PCA9555 driver (see PortExpander.h for design notes).

#include "PortExpander.h"

namespace sunray::platform {

// ── Construction ──────────────────────────────────────────────────────────────

PortExpander::PortExpander(I2C& bus, uint8_t addr)
    : bus_(bus)
    , addr_(addr)
{}

// ── Private register helpers ──────────────────────────────────────────────────

bool PortExpander::readReg(uint8_t reg, uint8_t& val) {
    // Write the register pointer, then read 1 byte (Repeated START).
    return bus_.writeRead(addr_, &reg, 1, &val, 1);
}

bool PortExpander::writeReg(uint8_t reg, uint8_t val) {
    const uint8_t buf[2] = {reg, val};
    return bus_.write(addr_, buf, sizeof(buf));
}

// ── Single-pin operations ─────────────────────────────────────────────────────

bool PortExpander::setPin(uint8_t port, uint8_t pin, bool level) {
    const uint8_t mask = static_cast<uint8_t>(1u << pin);

    // 1. Configure pin as output (clear its bit in the config register).
    uint8_t cfg = 0;
    if (!readReg(regConfig(port), cfg))  return false;
    if (!writeReg(regConfig(port), cfg & static_cast<uint8_t>(~mask))) return false;

    // 2. Set pin level in the output latch.
    uint8_t out = 0;
    if (!readReg(regOutput(port), out))  return false;
    if (level)
        out = static_cast<uint8_t>(out |  mask);
    else
        out = static_cast<uint8_t>(out & ~mask);
    return writeReg(regOutput(port), out);
}

bool PortExpander::getPin(uint8_t port, uint8_t pin) {
    const uint8_t mask = static_cast<uint8_t>(1u << pin);

    // 1. Configure pin as input (set its bit in the config register).
    uint8_t cfg = 0;
    if (!readReg(regConfig(port), cfg)) return false;
    if (!writeReg(regConfig(port), cfg | mask)) return false;

    // 2. Read the input register.
    uint8_t in = 0;
    if (!readReg(regInput(port), in)) return false;
    return (in & mask) != 0;
}

// ── Batch port operations ─────────────────────────────────────────────────────

bool PortExpander::setOutputPort(uint8_t port, uint8_t value) {
    return writeReg(regOutput(port), value);
}

bool PortExpander::setConfigPort(uint8_t port, uint8_t dirMask) {
    return writeReg(regConfig(port), dirMask);
}

bool PortExpander::getInputPort(uint8_t port, uint8_t& value) {
    return readReg(regInput(port), value);
}

} // namespace sunray::platform
