#include "Mpu6050Driver.h"

#include <cmath>
#include <iostream>

namespace sunray {

// ── MPU-6050 Registers ────────────────────────────────────────────────────────
static constexpr uint8_t REG_WHO_AM_I      = 0x75;
static constexpr uint8_t REG_PWR_MGMT_1    = 0x6B;
static constexpr uint8_t REG_CONFIG        = 0x1A;
static constexpr uint8_t REG_GYRO_CONFIG   = 0x1B;
static constexpr uint8_t REG_ACCEL_CONFIG  = 0x1C;
static constexpr uint8_t REG_ACCEL_XOUT_H  = 0x3B;

// ── Constants ────────────────────────────────────────────────────────────────
static constexpr float GYRO_FS_250_LSB = 131.0f;  // LSB per deg/s
static constexpr float ACCEL_FS_2_LSB  = 16384.0f; // LSB per g
static constexpr float DEG_TO_RAD       = M_PI / 180.0f;
static constexpr float RAD_TO_DEG       = 180.0f / M_PI;

Mpu6050Driver::Mpu6050Driver(platform::I2C& i2c, uint8_t addr)
    : i2c_(i2c), addr_(addr)
{}

bool Mpu6050Driver::init() {
    // 1. Check WHO_AM_I
    uint8_t who = 0;
    if (!readRegs(REG_WHO_AM_I, &who, 1) || who != 0x68) {
        std::cerr << "[MPU] sensor not found at 0x" << std::hex << (int)addr_ << std::dec << '\n';
        return false;
    }

    // 2. Wake up sensor, set clock source to X gyro (0x01)
    if (!writeReg(REG_PWR_MGMT_1, 0x01)) return false;

    // 3. Set DLPF to ~44 Hz (0x03)
    if (!writeReg(REG_CONFIG, 0x03)) return false;

    // 4. Set Gyro Range to ±250 °/s (0x00)
    if (!writeReg(REG_GYRO_CONFIG, 0x00)) return false;

    // 5. Set Accel Range to ±2 g (0x00)
    if (!writeReg(REG_ACCEL_CONFIG, 0x00)) return false;

    std::cerr << "[MPU] initialized at 0x" << std::hex << (int)addr_ << std::dec << '\n';
    return true;
}

void Mpu6050Driver::update(float dt_s) {
    uint8_t buf[14];
    if (!readRegs(REG_ACCEL_XOUT_H, buf, 14)) {
        std::lock_guard<std::mutex> lk(mutex_);
        data_.valid = false;
        return;
    }

    // Big-endian 16-bit signed
    auto s16 = [](uint8_t h, uint8_t l) {
        return static_cast<int16_t>((h << 8) | l);
    };

    const float ax = s16(buf[0],  buf[1])  / ACCEL_FS_2_LSB;
    const float ay = s16(buf[2],  buf[3])  / ACCEL_FS_2_LSB;
    const float az = s16(buf[4],  buf[5])  / ACCEL_FS_2_LSB;
    // buf[6,7] = Temp (ignored)
    const float gx = s16(buf[8],  buf[9])  / GYRO_FS_250_LSB;
    const float gy = s16(buf[10], buf[11]) / GYRO_FS_250_LSB;
    const float gz = s16(buf[12], buf[13]) / GYRO_FS_250_LSB;

    if (calibrating_) {
        sumGx_ += gx;
        sumGy_ += gy;
        sumGz_ += gz;
        calibSamples_++;
        if (calibSamples_ >= 250) { // ~5 seconds at 50 Hz
            biasGx_ = sumGx_ / 250.0f;
            biasGy_ = sumGy_ / 250.0f;
            biasGz_ = sumGz_ / 250.0f;
            calibrating_ = false;
            std::cerr << "[MPU] calibration done. biases: " << biasGx_ << ", " << biasGy_ << ", " << biasGz_ << '\n';
        }
        return;
    }

    // Bias correction and rad/s conversion
    const float rgx = (gx - biasGx_) * DEG_TO_RAD;
    const float rgy = (gy - biasGy_) * DEG_TO_RAD;
    const float rgz = (gz - biasGz_) * DEG_TO_RAD;

    // Yaw integration
    yaw_ += rgz * dt_s;

    // Pitch and Roll from Accel gravity vector
    // atan2(ay, az) gives roll (tilt around x axis)
    // atan2(-ax, sqrt(ay*ay + az*az)) gives pitch (tilt around y axis)
    const float accRoll  = std::atan2(ay, az);
    const float accPitch = std::atan2(-ax, std::sqrt(ay*ay + az*az));

    // Complementary filter (98% gyro, 2% accel)
    const float alpha = 0.98f;
    roll_  = alpha * (roll_  + rgx * dt_s) + (1.0f - alpha) * accRoll;
    pitch_ = alpha * (pitch_ + rgy * dt_s) + (1.0f - alpha) * accPitch;

    // Update snapshot
    {
        std::lock_guard<std::mutex> lk(mutex_);
        data_.accX  = ax; data_.accY  = ay; data_.accZ  = az;
        data_.gyroX = rgx; data_.gyroY = rgy; data_.gyroZ = rgz;
        data_.roll  = roll_;
        data_.pitch = pitch_;
        data_.yaw   = yaw_;
        data_.valid = true;
    }
}

void Mpu6050Driver::startCalibration() {
    std::lock_guard<std::mutex> lk(mutex_);
    calibSamples_ = 0;
    sumGx_ = sumGy_ = sumGz_ = 0;
    calibrating_ = true;
    std::cerr << "[MPU] starting calibration (5s)... keep robot still\n";
}

ImuData Mpu6050Driver::getData() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return data_;
}

bool Mpu6050Driver::writeReg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_.write(addr_, buf, 2);
}

bool Mpu6050Driver::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
    return i2c_.writeRead(addr_, &reg, 1, buf, len);
}

} // namespace sunray
