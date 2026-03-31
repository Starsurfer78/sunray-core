#pragma once
// HardwareInterface.h — Abstract Hardware Abstraction Layer for Sunray Core
//
// Derived from SerialRobotDriver analysis (Alfred/STM32 platform, March 2026).
// One flat interface — no artificial sub-driver split.
//
// Implementors:
//   hal/SerialRobotDriver/  — Alfred PCB (STM32 via UART + Port-Expander I2C)
//   hal/PicoRobotDriver/    — Phase 2 (Raspberry Pi Pico, direct PWM/Hall)
//
// The Core never includes any driver header — only this file.

#include <string>
#include <cstdint>

namespace sunray {

// ── Data structures returned by read*() methods ──────────────────────────────

/// Wheel and mow motor encoder increments since last readOdometry() call.
/// mcuConnected = false means no valid data this cycle (ticks should be treated
/// as zero by the caller). Returned at ~50 Hz cadence (AT+M rate on Alfred).
struct OdometryData {
    int  leftTicks   = 0;
    int  rightTicks  = 0;
    int  mowTicks    = 0;
    bool mcuConnected = false;  ///< false → ticks are invalid this cycle
};

/// Snapshot of all binary sensor inputs.
/// Updated at ~50 Hz from fast channel (bumper, lift, stopButton, motorFault)
/// and at ~2 Hz from slow channel (rain). Callers must not assume atomic update.
///
/// nearObstacle: intentionally always false on Alfred — protocol has no
/// sonar/ToF. Pico driver may implement it. Callers must handle false gracefully.
struct SensorData {
    bool bumperLeft   = false;
    bool bumperRight  = false;
    bool lift         = false;
    bool rain         = false;
    bool stopButton   = false;
    bool motorFault   = false;
    bool nearObstacle = false;  ///< optional — always false on Alfred
};

/// Power / charging state snapshot. Updated at ~2 Hz (AT+S rate on Alfred).
struct BatteryData {
    float voltage         = 0.0f;  ///< battery voltage (V)
    float chargeVoltage   = 0.0f;  ///< charger output voltage (V)
    float chargeCurrent   = 0.0f;  ///< charging current (A), 0 when not charging
    float batteryTemp     = 0.0f;  ///< battery temperature (°C), -9999 if unavailable
    bool  chargerConnected = false; ///< true when chargeVoltage exceeds dock-detect threshold
};

/// Snapshot of IMU (Inertial Measurement Unit) data.
/// Updated at ~50 Hz. Values are in SI units (g, rad/s, rad).
struct ImuData {
    float accX = 0.0f, accY = 0.0f, accZ = 0.0f;  ///< acceleration [g]
    float gyroX = 0.0f, gyroY = 0.0f, gyroZ = 0.0f; ///< angular velocity [rad/s]
    float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;    ///< integrated orientation [rad]
    bool  calibrating = false; ///< true while gyro bias estimation is running
    bool  valid = false;  ///< true if sensor is responding
};

// ── LED control ───────────────────────────────────────────────────────────────

/// Physical LED slots on the panel.
/// Alfred PCB has 3 LEDs (top-down order: LED_2, LED_3, LED_1 in case).
/// Pico driver must map to available LEDs or silently ignore unknown ids.
enum class LedId : uint8_t {
    LED_1 = 1,  ///< Alfred: bottom — WiFi status
    LED_2 = 2,  ///< Alfred: top    — Error / idle status
    LED_3 = 3,  ///< Alfred: middle — GPS status
};

/// Each LED slot has a red and a green element — only one should be active
/// at a time for clear semantics. OFF = both off.
enum class LedState : uint8_t {
    OFF   = 0,
    GREEN = 1,
    RED   = 2,
};

// ── HardwareInterface ─────────────────────────────────────────────────────────

class HardwareInterface {
public:
    virtual ~HardwareInterface() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Initialize hardware (open serial port, configure I2C, power on IMU/fan,
    /// run ADC self-test, etc.). Called once at startup.
    /// Returns true on success. On false, Core should abort.
    virtual bool init() = 0;

    /// Periodic driver tick — must be called every main-loop iteration.
    /// Handles internal timing (AT+M at 50 Hz, AT+S at 2 Hz, LED update at 3 s,
    /// CPU temp at 60 s, WiFi poll at 7 s, etc.) without blocking.
    virtual void run() = 0;

    // ── Motor output ──────────────────────────────────────────────────────────

    /// Set traction and mow motor PWM values.
    /// Range: -255 (full reverse) … 0 (stop) … +255 (full forward).
    /// The driver is responsible for any physical left/right swap compensation
    /// (BUG-07 cross-wiring on Alfred — stays inside the driver, not here).
    virtual void setMotorPwm(int left, int right, int mow) = 0;

    /// Reset a latched motor fault condition. Called by Core after a fault
    /// has been handled and the operator requests a retry.
    virtual void resetMotorFault() = 0;

    // ── Sensor input ──────────────────────────────────────────────────────────

    /// Returns the most recent encoder tick deltas and MCU connectivity state.
    /// The driver accumulates ticks internally between calls; each call
    /// resets the delta counter. Must be called at the control-loop rate.
    virtual OdometryData readOdometry() = 0;

    /// Returns a snapshot of all binary sensor states.
    /// Returned values reflect the most recently received AT-frame data —
    /// no additional filtering is applied by the interface.
    virtual SensorData readSensors() = 0;

    /// Returns the most recently received battery / charging data.
    virtual BatteryData readBattery() = 0;

    /// Returns the most recent IMU (gyro/accel/heading) snapshot.
    /// Implementation should handle internal integration (heading) and filtering.
    virtual ImuData readImu() = 0;

    /// Start IMU calibration (gyro bias estimation).
    virtual void calibrateImu() = 0;

    // ── Actuators (non-motor) ─────────────────────────────────────────────────

    /// Activate (on=true) or deactivate (on=false) the buzzer.
    /// Frequency selection is not part of the interface — drivers that support
    /// tone frequencies may implement it internally (e.g. on Pico).
    virtual void setBuzzer(bool on) = 0;

    /// Set the state of a panel LED.
    /// Drivers that do not have the requested LedId silently ignore the call.
    virtual void setLed(LedId id, LedState state) = 0;

    // ── Power management ──────────────────────────────────────────────────────

    /// Tell the driver whether the system should stay on.
    /// keepPowerOn(false) triggers the platform shutdown sequence:
    ///   Alfred: turns off LEDs, waits 5 s, calls `shutdown now`
    ///   Pico:   signals safe shutdown to Pico via UART
    /// keepPowerOn(true) cancels any pending shutdown.
    virtual void keepPowerOn(bool flag) = 0;

    // ── System information ────────────────────────────────────────────────────

    /// CPU temperature in °C. Returns -9999 if not available on this platform.
    virtual float getCpuTemperature() = 0;

    /// Platform-unique robot identifier (e.g. eth0 MAC address on Alfred).
    virtual std::string getRobotId() = 0;

    /// MCU firmware name and version as reported by AT+V.
    /// Returns empty strings until the first AT+V response is received.
    virtual std::string getMcuFirmwareName() = 0;
    virtual std::string getMcuFirmwareVersion() = 0;
};

} // namespace sunray
