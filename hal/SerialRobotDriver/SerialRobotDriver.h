#pragma once
// SerialRobotDriver.h — Alfred (STM32) implementation of HardwareInterface.
//
// Communicates with the STM32 MCU via UART AT-frames:
//   AT+M  (50 Hz)  — motor PWM command + encoder/sensor response
//   AT+S  (2 Hz)   — summary request (battery, bumpers, rain, currents)
//   AT+V  (once)   — firmware name/version handshake
//
// Architecture decisions (see .memory/decisions.md):
//   - Flat class: the 8 sub-drivers of the original code are merged here.
//   - BUG-07: PWM/encoder left↔right swap compensation lives here, not in HardwareInterface.
//   - BUG-05: unsigned tick overflow fixed by casting to long before subtraction.
//   - nearObstacle: always false — Alfred AT-protocol has no sonar/ToF field.
//   - init() opens Serial + I2C; constructor never throws.
//   - setLed() writes immediately to EX3 PortExpander (PCA9555 at 0x22).
//   - Fan controlled internally based on CPU temp (>65°C on, <60°C off).
//   - WiFi LED (LED_1) updated internally every 7 s via wpa_cli.
//   - keepPowerOn(false): 5 s grace → fan off → `shutdown now`.

#include <cstdint>
#include <memory>
#include <string>

#include "../HardwareInterface.h"
#include "../../core/Config.h"
#include "../../core/Logger.h"
#include "../../platform/Serial.h"
#include "../../platform/I2C.h"
#include "../../platform/I2cMux.h"
#include "../../platform/PortExpander.h"
#include "../Imu/Mpu6050Driver.h"

namespace sunray {

class SerialRobotDriver final : public HardwareInterface {
public:
    /// Construct without touching hardware. Config is read in init().
    explicit SerialRobotDriver(std::shared_ptr<Config> config, std::shared_ptr<Logger> logger);
    ~SerialRobotDriver() override;

    // Non-copyable, non-movable (owns hardware resources)
    SerialRobotDriver(const SerialRobotDriver&)            = delete;
    SerialRobotDriver& operator=(const SerialRobotDriver&) = delete;

    // ── HardwareInterface ─────────────────────────────────────────────────────

    /// Open UART + I2C, power IMU + fan, configure LEDs.
    /// Returns false on any hardware failure — Core must abort if false.
    bool init() override;

    /// Non-blocking main loop tick. Must be called every control-loop iteration.
    /// Handles: AT+M at 50 Hz, AT+S at 2 Hz, LED at 3 s, CPU temp at 59 s,
    ///          WiFi state at 7 s, shutdown sequencing.
    void run() override;

    /// Latch PWM values for the next AT+M frame (sent at next 50 Hz tick).
    /// BUG-07: left/right swap compensation applied internally before sending.
    void setMotorPwm(int left, int right, int mow) override;

    /// No-op on Alfred — STM32 auto-clears motor fault after overload recovery.
    void resetMotorFault() override;

    /// Returns encoder tick deltas since the last call. Resets the delta counter.
    /// BUG-05: unsigned overflow handled via long cast.
    /// BUG-07: field remapping applied so returned leftTicks/rightTicks are correct.
    OdometryData readOdometry() override;

    /// Returns a snapshot of bumper, lift, rain, stopButton, motorFault states.
    SensorData readSensors() override;

    /// Returns battery voltage, charge voltage/current, charger connected state.
    /// If MCU disconnected: voltage falls back to 28 V (safe default for Pi standalone).
    BatteryData readBattery() override;

    /// Returns the most recent IMU (gyro/accel/heading) snapshot.
    ImuData readImu() override;

    /// Start IMU calibration (gyro bias estimation).
    void calibrateImu() override;

    /// Drive buzzer via EX2 PortExpander (PCA9555 at 0x20, IO1.1).
    void setBuzzer(bool on) override;

    /// Write LED state immediately to EX3 PortExpander (PCA9555 at 0x22).
    /// LED_1 = WiFi (bottom), LED_2 = Error/idle (top), LED_3 = GPS (middle).
    void setLed(LedId id, LedState state) override;

    /// keepPowerOn(false) starts a 5 s grace period then calls `shutdown now`.
    /// keepPowerOn(true) cancels any pending shutdown.
    void keepPowerOn(bool flag) override;

    /// CPU temperature from /sys/class/thermal/thermal_zone0/temp. -9999 if unavailable.
    float getCpuTemperature() override;

    /// eth0 MAC address (used as unique robot identifier).
    std::string getRobotId() override;

    /// Firmware name from AT+V response (empty until first response received).
    std::string getMcuFirmwareName() override;

    /// Firmware version from AT+V response.
    std::string getMcuFirmwareVersion() override;

private:
    std::shared_ptr<Config> config_;
    std::shared_ptr<Logger> logger_;

    // Platform objects — constructed in init(), null until then
    std::unique_ptr<platform::Serial>       uart_;
    std::unique_ptr<platform::I2C>          i2c_;
    std::unique_ptr<platform::I2cMux>       mux_;
    std::unique_ptr<platform::PortExpander> ex1_;  // 0x21: IMU power, Fan, ADC mux
    std::unique_ptr<platform::PortExpander> ex2_;  // 0x20: Buzzer
    std::unique_ptr<platform::PortExpander> ex3_;  // 0x22: Panel LEDs
    std::unique_ptr<Mpu6050Driver>          imu_;

    // MCU identity
    std::string robotId_;
    std::string mcuFirmwareName_;
    std::string mcuFirmwareVersion_;

    // Latched PWM (applied at next 50 Hz tick)
    int pwmLeft_  = 0;
    int pwmRight_ = 0;
    int pwmMow_   = 0;

    // Sensor snapshots (updated by AT frame parsers)
    SensorData  sensors_{};
    BatteryData battery_{};
    bool        rawChargerConnected_ = false;
    bool        motorFaultFast_ = false;
    bool        motorFaultSummary_ = false;
    unsigned    chargerConnectedStableCount_ = 0;
    unsigned    chargerDisconnectedStableCount_ = 0;

    // Raw cumulative encoder ticks from MCU (unsigned, as sent by STM32)
    unsigned long rawTicksLeft_  = 0;
    unsigned long rawTicksRight_ = 0;
    unsigned long rawTicksMow_   = 0;
    unsigned long lastTicksLeft_  = 0;
    unsigned long lastTicksRight_ = 0;
    unsigned long lastTicksMow_   = 0;
    bool          resetTicks_    = true;
    bool          mcuConnected_  = false;

    // System state
    float cpuTemp_        = 30.0f;
    bool  wifiConnected_  = false;
    bool  wifiInactive_   = false;
    std::string wifiInterface_ = "wlan0";
    std::string wifiState_;
    std::string wifiLastLoggedState_;
    bool  serialDebug_    = false;
    bool  shutdownPending_ = false;
    uint64_t shutdownAt_  = 0;   // nowMs() at which shutdown command fires
    uint64_t rxLastByteMs_ = 0;
    uint64_t rxGapMs_      = 50;
    uint64_t wifiLastScanMs_ = 0;
    uint64_t wifiLastReconnectMs_ = 0;
    uint64_t motorPeriodMs_ = 20;

    // Diagnostic counters (reset every 1 s)
    int motorTxCount_   = 0;
    int motorRxCount_   = 0;
    int summaryTxCount_ = 0;
    int summaryRxCount_ = 0;

    // Timing (nowMs() timestamps for each periodic task)
    uint64_t nextMotorMs_   = 0;
    uint64_t nextSummaryMs_ = 0;
    uint64_t nextConsoleMs_ = 0;
    uint64_t nextTempMs_    = 0;
    uint64_t nextWifiMs_    = 0;
    uint64_t nextLedMs_     = 0;
    uint64_t nextImuMs_           = 0;
    uint64_t lastImuMs_           = 0;
    uint64_t nextImuMuxRecoverMs_ = 0;  ///< next mux channel re-select attempt when IMU is down

    // Cached mux channel numbers (read from config in init(), used every run() cycle)
    int imuMuxChannel_    = 4;
    int ex3MuxChannel_    = 0;
    int legacyMuxChannel_ = 0;

    // RX accumulation buffer (chars until \r or \n)
    std::string rxBuf_;

    // ── AT protocol ───────────────────────────────────────────────────────────
    void sendAt(const std::string& cmd);
    void pollRx();
    void dispatchFrame(std::string frame);  // strips CRC, routes to parse*
    void drainFramedRx();                   // extracts Alfred AT frames without CR/LF
    void parseMotorFrame  (const std::string& frame);
    void parseSummaryFrame(const std::string& frame);
    void parseVersionFrame(const std::string& frame);
    void updateChargerConnected(bool rawConnected);

    static bool    verifyCrc(std::string& frame);  // strips CRC suffix on success
    static uint8_t calcCrc  (const std::string& s);

    // ── Hardware helpers ──────────────────────────────────────────────────────
    void setFanPower(bool on);
    bool writeLed(LedId id, LedState state);  // direct EX3 write, returns false on I2C error

    void updateCpuTemp();
    void updateWifi();
    void recoverWifi();
    void updateWifiLed();

    // ── Utility ───────────────────────────────────────────────────────────────
    static uint64_t    nowMs();
    static std::string shellRead(const char* cmd);  // popen + collect output
    static bool        shellExec(const std::string& cmd);
    static std::string trim(std::string s);
    void               logSerialBytes(const char* prefix, const uint8_t* buf, size_t len) const;
    uint8_t            configI2cAddr(const std::string& key, uint8_t fallback) const;
    void               selectImuChannel();      ///< select IMU mux channel exclusively
    void               selectEx3Channel();      ///< select LED/EX3 mux channel exclusively
    void               selectLegacyChannel();   ///< select EX1/EX2 (fan, buzzer) mux channel
    static int           fieldInt  (const std::string& s);
    static unsigned long fieldULong(const std::string& s);  ///< BUG-003: for tick counters
    static float         fieldFloat(const std::string& s);
};

} // namespace sunray
