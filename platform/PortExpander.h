#pragma once
// PortExpander.h — PCA9555 16-bit I/O port expander driver.
//
// The PCA9555 has two 8-bit ports (port 0, port 1), each with independent
// direction (input/output per pin) and output latch registers.
//
// Register map (per PCA9555 datasheet):
//   0x00 / 0x01  Input  Port 0 / 1  (read-only — reflects actual pin state)
//   0x02 / 0x03  Output Port 0 / 1  (output latch — what gets driven)
//   0x04 / 0x05  Polarity Inversion  (unused here — left at reset default 0x00)
//   0x06 / 0x07  Config Port 0 / 1  (0 = output, 1 = input per bit; reset = 0xFF)
//
// Alfred PCB I2C device map (bus /dev/i2c-1):
//   EX1  0x21  IMU power (IO1.6), Fan power (IO1.7), ADC mux A0-A2+EN (IO1.0-3)
//   EX2  0x20  Buzzer (IO1.1), SWD-CS6 (IO0.6)
//   EX3  0x22  LED1 green/red (IO0.0/1), LED2 (IO0.2/3), LED3 (IO0.4/5)
//
// Design:
//   - One PortExpander object per device (holds I2C& reference + address).
//   - setPin(): configure pin as output AND set level (read-modify-write each call).
//   - getPin(): configure pin as input, read and return level.
//   - setOutputPort() / setConfigPort(): batch operations — set all 8 bits at once.
//     More efficient when multiple pins on the same port change simultaneously.
//   - No internal state cache — always reads register before modifying
//     (matches ioExpanderOut/In behaviour, safe for multi-master scenarios).
//
// Usage:
//   sunray::platform::I2C        bus("/dev/i2c-1");
//   sunray::platform::PortExpander ex3(bus, 0x22);  // LED panel
//   ex3.setPin(0, 0, true);   // LED1 green ON
//   ex3.setPin(0, 1, false);  // LED1 red  OFF

#include <cstdint>
#include "I2C.h"

namespace sunray::platform {

class PortExpander {
public:
    /// Bind this driver to an I2C bus and a PCA9555 at 7-bit address addr.
    /// Does NOT talk to the hardware yet — call init() or setPin() first.
    PortExpander(I2C& bus, uint8_t addr);

    // Copyable (holds only a reference + address — no resource ownership)
    PortExpander(const PortExpander&)            = default;
    PortExpander& operator=(const PortExpander&) = default;

    // ── Single-pin operations ─────────────────────────────────────────────────

    /// Configure pin as output and drive it to level (true = high, false = low).
    /// Performs read-modify-write on both the config and output registers.
    /// Returns false on any I2C error.
    bool setPin(uint8_t port, uint8_t pin, bool level);

    /// Configure pin as input and return its current state.
    /// Returns true for high, false for low or I2C error.
    bool getPin(uint8_t port, uint8_t pin);

    // ── Batch port operations (more efficient for multiple-pin changes) ────────

    /// Write the full 8-bit output latch for port (0 or 1).
    /// Does NOT change the direction register — caller must ensure pins are outputs.
    /// Returns false on I2C error.
    bool setOutputPort(uint8_t port, uint8_t value);

    /// Set the direction register for port (0 or 1).
    /// Bit = 0 → output, Bit = 1 → input (PCA9555 reset default: 0xFF = all inputs).
    bool setConfigPort(uint8_t port, uint8_t dirMask);

    /// Read the full 8-bit input register for port (0 or 1) into value.
    /// Returns false on I2C error.
    bool getInputPort(uint8_t port, uint8_t& value);

    // ── Accessors ─────────────────────────────────────────────────────────────

    uint8_t address() const { return addr_; }

private:
    I2C&    bus_;
    uint8_t addr_;

    // PCA9555 register address helpers
    static uint8_t regInput (uint8_t port) { return static_cast<uint8_t>(0x00 + port); }
    static uint8_t regOutput(uint8_t port) { return static_cast<uint8_t>(0x02 + port); }
    static uint8_t regConfig(uint8_t port) { return static_cast<uint8_t>(0x06 + port); }

    /// Read one register byte. Returns false on error.
    bool readReg (uint8_t reg, uint8_t& val);
    /// Write one register byte. Returns false on error.
    bool writeReg(uint8_t reg, uint8_t  val);
};

} // namespace sunray::platform
