#pragma once
// I2C.h — Linux i2cdev bus wrapper (replaces Arduino Wire.h / Wire.cpp).
//
// Design:
//   - One I2C object per bus (e.g. /dev/i2c-1).
//   - Multiple devices on the same bus → same I2C instance, address per call.
//   - Three operations: write(), read(), writeRead().
//   - writeRead() uses I2C_RDWR ioctl → single atomic transaction with
//     Repeated START (no STOP between write and read phase).
//     Required for proper register-addressed reads (PCA9555, MCP3421, EEPROM).
//   - write() / read() use I2C_SLAVE ioctl + syscall (simpler, sufficient for
//     single-phase transactions).
//   - Current device address is cached to avoid redundant ioctl() calls.
//   - Constructor throws std::runtime_error on open failure.
//   - All methods return false / 0 on error — no exceptions at runtime.
//
// Alfred I2C device map (bus /dev/i2c-1):
//   0x20  PCA9555 EX2 — Buzzer, SWD-switching
//   0x21  PCA9555 EX1 — IMU power, Fan power, ADC mux
//   0x22  PCA9555 EX3 — Panel LEDs (LED1..LED3 red/green)
//   0x50  BL24C256A   — EEPROM
//   0x68  MCP3421     — ADC (battery voltage)
//   0x70  TCA9548A    — I2C mux (selects IMU / EEPROM / ADC sub-bus)
//
// Usage:
//   sunray::platform::I2C bus("/dev/i2c-1");
//   uint8_t cmd[2] = {0x06, 0x00};          // PCA9555: configure port 0 as output
//   bus.write(0x22, cmd, sizeof(cmd));
//   uint8_t reg = 0x00;                      // PCA9555: read port 0
//   uint8_t val;
//   bus.writeRead(0x22, &reg, 1, &val, 1);

#include <cstddef>
#include <cstdint>
#include <string>

namespace sunray::platform {

class I2C {
public:
    /// Open the I2C bus device (e.g. "/dev/i2c-1").
    /// Throws std::runtime_error if the device node cannot be opened.
    explicit I2C(const std::string& bus);

    /// Close the bus file descriptor.
    ~I2C();

    // Non-copyable, move-constructible
    I2C(const I2C&)            = delete;
    I2C& operator=(const I2C&) = delete;
    I2C(I2C&&) noexcept;
    I2C& operator=(I2C&&) noexcept;

    // ── Single-phase operations ───────────────────────────────────────────────

    /// Write len bytes from buf to the 7-bit device address addr.
    /// Returns true on success.
    bool write(uint8_t addr, const uint8_t* buf, size_t len);

    /// Read len bytes from device addr into buf.
    /// Returns true on success.
    bool read(uint8_t addr, uint8_t* buf, size_t len);

    // ── Combined write+read (atomic, Repeated START) ──────────────────────────

    /// Write txLen bytes, then — without releasing the bus — read rxLen bytes.
    /// Uses I2C_RDWR ioctl so no STOP is inserted between the two phases.
    /// Typical use: write [register address], read [register value].
    /// Returns true if both phases succeeded.
    bool writeRead(uint8_t        addr,
                   const uint8_t* txBuf, size_t txLen,
                   uint8_t*       rxBuf, size_t rxLen);

    // ── Status ────────────────────────────────────────────────────────────────

    /// True if the bus file descriptor is open.
    bool isOpen() const { return fd_ >= 0; }

    /// The device path passed to the constructor.
    const std::string& busPath() const { return bus_; }

private:
    std::string bus_;
    int         fd_          = -1;
    uint8_t     currentAddr_ = 0xFF;  ///< cached addr — avoids redundant ioctl

    /// Set I2C_SLAVE address (skipped if addr == currentAddr_).
    /// Returns false on ioctl failure.
    bool selectDevice(uint8_t addr);

    void closeBus();
};

} // namespace sunray::platform
