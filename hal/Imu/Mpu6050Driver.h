#pragma once
// Mpu6050Driver.h — I2C driver for InvenSense MPU-6050 IMU.
//
// Features:
//   - Uses platform/I2C.h for Linux i2cdev access.
//   - Configures Gyro (±250°/s) and Accel (±2g) ranges.
//   - Implements 50 Hz Yaw-integration (Gyro-Z).
//   - Gyro calibration (bias estimation) while robot is stationary.
//   - Complementary filter for Pitch/Roll (Accel-gravity vector).

#include "platform/I2C.h"
#include "hal/HardwareInterface.h"
#include "core/Logger.h"

#include <mutex>
#include <atomic>

namespace sunray {

class Mpu6050Driver {
public:
    /// Construct with reference to an existing I2C bus.
    /// addr: 7-bit I2C address (default: 0x68, AD0 = GND).
    explicit Mpu6050Driver(platform::I2C& i2c, std::shared_ptr<Logger> logger, uint8_t addr = 0x68);

    /// Initialize the sensor: wake up, set ranges, enable DLPF.
    /// Returns true if the sensor is responding (WHO_AM_I check).
    bool init();

    /// Read raw data and update internal orientation.
    /// dt_s: time since last call in seconds (for integration).
    /// Should be called at ~50 Hz.
    void update(float dt_s);

    /// Start bias calibration. Robot must be perfectly still.
    /// Collects samples for ~5 seconds.
    void startCalibration();

    /// Returns true if currently calibrating.
    bool isCalibrating() const { return calibrating_; }

    /// Returns the latest IMU snapshot (SI units).
    ImuData getData() const;

private:
    platform::I2C& i2c_;
    std::shared_ptr<Logger> logger_;
    uint8_t        addr_;

    mutable std::mutex mutex_;
    ImuData            data_;

    // Calibration state
    std::atomic<bool> calibrating_{false};
    int               calibSamples_ = 0;
    float             biasGx_ = 0, biasGy_ = 0, biasGz_ = 0;
    float             sumGx_  = 0, sumGy_  = 0, sumGz_  = 0;

    // Internal integration
    float yaw_   = 0;
    float pitch_ = 0;
    float roll_  = 0;

    bool writeReg(uint8_t reg, uint8_t val);
    bool readRegs(uint8_t reg, uint8_t* buf, size_t len);
};

} // namespace sunray
