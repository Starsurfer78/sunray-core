// SerialRobotDriver.cpp — Alfred/STM32 HardwareInterface implementation.
// See SerialRobotDriver.h for design notes and architecture decisions.

#include "SerialRobotDriver.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

namespace sunray {

namespace {

constexpr auto kStartupLedHold = std::chrono::milliseconds(300);
constexpr auto kStartupBuzzerPulse = std::chrono::milliseconds(120);
constexpr unsigned kChargerDebounceSamples = 2;
constexpr uint64_t kWifiScanIntervalMs = 15000;
constexpr uint64_t kWifiReconnectIntervalMs = 10000;
constexpr uint64_t kImuPeriodMs = 20;
constexpr uint64_t kMotorPeriodMinMs = 5;
constexpr uint64_t kSummaryPeriodMs = 500;
constexpr uint64_t kConsolePeriodMs = 1000;
constexpr uint64_t kLedPeriodMs = 3000;
constexpr uint64_t kWifiPeriodMs = 7000;
constexpr uint64_t kTempPeriodMs = 59000;
constexpr uint64_t kImuRecoverPeriodMs = 10000;

// Convert charge-voltage reading into boolean charger-connected signal.
bool chargerConnectedFromVoltage(float chargeVoltage, float threshold) {
    return chargeVoltage > threshold;
}

// Convert loop period (ms) into expected frequency (Hz), minimum 1.
int expectedHzFromPeriodMs(uint64_t periodMs) {
    if (periodMs == 0) return 1;
    return std::max(1, static_cast<int>(1000 / periodMs));
}

// Warn when measured rate drops more than a small margin below expected.
int warnBelowHz(int expectedHz) {
    return std::max(1, expectedHz - 2);
}

enum class MotorFrameSchema {
    Compact,
    Legacy,
};

MotorFrameSchema detectMotorFrameSchema(const std::vector<std::string>& f) {
    return (f.size() == 6) ? MotorFrameSchema::Compact : MotorFrameSchema::Legacy;
}

// Cached config snapshot read once during init().
struct InitCfg {
    bool serialDebug = false;
    uint64_t rxGapMs = 50;
    std::string wifiInterface = "wlan0";

    std::string uartPort = "/dev/ttyS0";
    int uartBaud = 115200;
    uint64_t motorPeriodMs = kImuPeriodMs;

    std::string i2cBus = "/dev/i2c-1";
    int imuMuxChannel = 4;
    int ex3MuxChannel = 0;
    int legacyMuxChannel = 0;
    float chargerConnectedVoltageV = 7.0f;
};

// Read/init all SerialRobotDriver config keys in one place and clamp values.
InitCfg loadInitCfg(const sunray::Config& config) {
    InitCfg out;
    out.serialDebug = config.get<bool>("serial_debug", false);
    out.rxGapMs = static_cast<uint64_t>(config.get<int>("serial_rx_gap_ms", 50));
    out.wifiInterface = config.get<std::string>("wifi_interface", "wlan0");

    out.uartPort = config.get<std::string>("driver_port", "/dev/ttyS0");
    out.uartBaud = config.get<int>("driver_baud", 115200);

    out.motorPeriodMs = static_cast<uint64_t>(config.get<int>(
        "serial_motor_period_ms",
        (out.uartBaud <= 19200) ? 35 : 20));
    if (out.motorPeriodMs < kMotorPeriodMinMs) out.motorPeriodMs = kMotorPeriodMinMs;

    out.i2cBus = config.get<std::string>("i2c_bus", "/dev/i2c-1");
    out.imuMuxChannel = config.get<int>("imu_mux_channel", 4);
    out.ex3MuxChannel = config.get<int>("ex3_mux_channel", 0);
    out.legacyMuxChannel = config.get<int>("i2c_mux_legacy_channel", 0);
    out.chargerConnectedVoltageV = config.get<float>("charger_connected_voltage_v", 7.0f);
    return out;
}

} // namespace

// ── Construction ──────────────────────────────────────────────────────────────

// Construct driver without touching hardware; actual IO opens in init().
SerialRobotDriver::SerialRobotDriver(std::shared_ptr<Config> config, std::shared_ptr<Logger> logger)
    : config_(std::move(config))
    , logger_(std::move(logger))
{}

// Destructor relies on RAII cleanup of platform objects; does not power down fan.
SerialRobotDriver::~SerialRobotDriver() {
    // Platform objects closed by their own destructors.
    // Fan stays in whatever state it was — keepPowerOn handles graceful shutdown.
}

// ── HardwareInterface: init() ─────────────────────────────────────────────────

// Open UART/I2C, power up peripherals, init IMU, run LED/buzzer startup, arm timers.
bool SerialRobotDriver::init() {
    const InitCfg cfg = loadInitCfg(*config_);
    serialDebug_ = cfg.serialDebug;
    rxGapMs_ = cfg.rxGapMs;
    wifiInterface_ = cfg.wifiInterface;
    chargerConnectedVoltageV_ = cfg.chargerConnectedVoltageV;

    // ── UART to STM32 ─────────────────────────────────────────────────────────
    const std::string port = cfg.uartPort;
    const int baud = cfg.uartBaud;
    motorPeriodMs_ = cfg.motorPeriodMs;
    try {
        uart_ = std::make_unique<platform::Serial>(port, static_cast<unsigned>(baud));
    } catch (const std::runtime_error& e) {
        if (logger_) logger_->error("SRD", "UART open failed: " + std::string(e.what()));
        return false;
    }

    // ── I2C bus ───────────────────────────────────────────────────────────────
    const std::string i2cBus = cfg.i2cBus;
    try {
        i2c_ = std::make_unique<platform::I2C>(i2cBus);
        if (config_->get<bool>("i2c_mux_enabled", true)) {
            mux_ = std::make_unique<platform::I2cMux>(
                *i2c_, configI2cAddr("i2c_mux_addr", 0x70));
        }
        ex1_ = std::make_unique<platform::PortExpander>(*i2c_, 0x21); // IMU/fan/ADC mux
        ex2_ = std::make_unique<platform::PortExpander>(*i2c_, 0x20); // Buzzer
        ex3_ = std::make_unique<platform::PortExpander>(*i2c_, 0x22); // Panel LEDs
    } catch (const std::runtime_error& e) {
        if (logger_) logger_->error("SRD", "I2C open failed: " + std::string(e.what()));
        return false;
    }

    // ── Robot ID (eth0 MAC address) ───────────────────────────────────────────
    robotId_ = shellRead("ip link show eth0 | grep link/ether | awk '{print $2}'");
    if (!robotId_.empty() && robotId_.back() == '\n') robotId_.pop_back();
    if (logger_) logger_->info("SRD", "Robot ID: " + robotId_);

    // ── I2C Mux (TCA9548A) — cache channel numbers from config ───────────────
    imuMuxChannel_ = cfg.imuMuxChannel;
    ex3MuxChannel_ = cfg.ex3MuxChannel;
    legacyMuxChannel_ = cfg.legacyMuxChannel;
    // Select the legacy/EX1 channel first so EX1 power and buzzer ops are routed correctly.
    selectLegacyChannel();

    // ── IMU + Fan power via EX1 ───────────────────────────────────────────────
    // PCA9555 EX1: IO1.6 = IMU power, IO1.7 = Fan power
    // Ensure both are set correctly on init.
    if (ex1_) {
        bool ok = true;
        if (!ex1_->setPin(1, 6, true)) ok = false;  // IMU ON  (IO1.6)
        if (!ex1_->setPin(1, 7, true)) ok = false;  // Fan ON  (IO1.7) — channel already selected above
        if (ok) {
            if (logger_) logger_->info("SRD", "EX1: IMU power enabled (IO1.6)");
            // Give sensor time to power up before I2C access
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        } else {
            if (logger_) logger_->error("SRD", "EX1: failed to enable IMU power — I2C error at 0x21?");
        }
    }

    // ── MPU-6050 IMU ──────────────────────────────────────────────────────────
    // Select IMU channel exclusively before init — prevents address collision with
    // devices on other channels (e.g. ADC on ch6 may share address 0x68/0x69).
    selectImuChannel();
    const uint8_t imuAddr = configI2cAddr("imu_i2c_addr", 0x69);
    imu_ = std::make_unique<Mpu6050Driver>(*i2c_, logger_, imuAddr);
    bool imuOk = imu_->init();
    uint8_t usedImuAddr = imuAddr;
    if (!imuOk && imuAddr != 0x68) {
        if (logger_) {
            std::ostringstream ss;
            ss << "IMU at 0x" << std::hex << (int)imuAddr << std::dec << " failed, trying fallback 0x68";
            logger_->warn("SRD", ss.str());
        }
        // Wait a bit before retry to let I2C bus settle
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        selectImuChannel();  // Re-select channel for fallback attempt
        imu_ = std::make_unique<Mpu6050Driver>(*i2c_, logger_, 0x68);
        imuOk = imu_->init();
        if (imuOk) usedImuAddr = 0x68;
    }
    if (!imuOk) {
        if (logger_) logger_->error("SRD", "IMU init failed on both addresses - check wiring, mux channel " +
                                    std::to_string(imuMuxChannel_) + ", power, or sensor hardware");
    } else {
        if (logger_) {
            std::ostringstream ss;
            ss << "IMU successfully initialized at address 0x" << std::hex << (int)usedImuAddr;
            logger_->info("SRD", ss.str());
        }
    }

    // ── Panel LEDs — startup sequence ─────────────────────────────────────────
    // 1. All OFF
    writeLed(LedId::LED_1, LedState::OFF);
    writeLed(LedId::LED_2, LedState::OFF);
    writeLed(LedId::LED_3, LedState::OFF);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2. All RED for 2 seconds
    bool ledOk = writeLed(LedId::LED_1, LedState::RED);
    writeLed(LedId::LED_2, LedState::RED);
    writeLed(LedId::LED_3, LedState::RED);
    
    if (!ledOk) {
        if (logger_) logger_->warn("SRD", "LED panel not responding — assuming not installed");
    }

    if (config_->get<bool>("buzzer_enabled", true)) {
        setBuzzer(true);
        std::this_thread::sleep_for(kStartupBuzzerPulse);
        setBuzzer(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000) - kStartupBuzzerPulse);
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    // 3. Transition to green (initial state)
    writeLed(LedId::LED_1, LedState::GREEN);
    writeLed(LedId::LED_2, LedState::GREEN);
    writeLed(LedId::LED_3, LedState::GREEN);

    // ── Initial battery data default (safe fallback if MCU not yet connected) ─
    battery_.voltage = 28.0f;  // prevents immediate shutdown on startup

    // Avoid an immediate false communication warning on the very first run().
    const uint64_t now = nowMs();
    nextMotorMs_          = now + motorPeriodMs_;
    nextSummaryMs_        = now + kSummaryPeriodMs;
    nextConsoleMs_        = now + kConsolePeriodMs;
    nextLedMs_            = now + kLedPeriodMs;
    nextTempMs_           = now + kTempPeriodMs;
    nextWifiMs_           = now + kWifiPeriodMs;
    // Give the IMU a few seconds to become valid before mux recovery kicks in.
    nextImuMuxRecoverMs_  = now + 10000;

    if (logger_) {
        logger_->info("SRD", "init complete — UART=" + port + " @" + std::to_string(baud) +
                             " I2C=" + i2cBus);
    }
    return true;
}

// ── HardwareInterface: run() ──────────────────────────────────────────────────

// Non-blocking periodic tick. Poll RX first, then run scheduled tasks (motor, summary, IMU, etc).
void SerialRobotDriver::run() {
    if (!uart_) return;

    pollRx();

    const uint64_t now = nowMs();

    tickImu(now);
    tickMotor(now);
    tickSummary(now);
    tickConsole(now);
    tickCpuTemp(now);
    tickWifi(now);
    tickLed(now);
    tickShutdown(now);
}

// IMU update tick (nominal 50 Hz) + periodic diagnostics when IMU remains invalid.
void SerialRobotDriver::tickImu(uint64_t now) {
    if (!imu_ || now < nextImuMs_) return;

    selectImuChannel();
    const float dt = (lastImuMs_ == 0) ? (static_cast<float>(kImuPeriodMs) / 1000.0f)
                                       : (now - lastImuMs_) / 1000.0f;
    imu_->update(dt);
    lastImuMs_ = now;
    nextImuMs_ = now + kImuPeriodMs;

    if (!imu_->isValid() && now >= nextImuMuxRecoverMs_) {
        nextImuMuxRecoverMs_ = now + kImuRecoverPeriodMs;
        if (logger_) {
            logger_->warn("SRD", "IMU still not valid after driver recovery attempts - check hardware");
            ImuData data = imu_->getData();
            std::ostringstream ss;
            ss << "IMU data - valid: " << data.valid << ", calibrating: " << data.calibrating
               << ", acc: [" << data.accX << ", " << data.accY << ", " << data.accZ << "]";
            logger_->info("SRD", ss.str());
        }
    }
}

// Motor command tick: send AT+M with latched PWM values.
void SerialRobotDriver::tickMotor(uint64_t now) {
    if (now < nextMotorMs_) return;

    nextMotorMs_ = now + motorPeriodMs_;
    std::string cmd = "AT+M," + std::to_string(pwmRight_)
                    + ","     + std::to_string(pwmLeft_)
                    + ","     + std::to_string(pwmMow_);
    sendAt(cmd);
    motorTxCount_++;
}

// Summary request tick: send AT+S (slow battery/safety summary).
void SerialRobotDriver::tickSummary(uint64_t now) {
    if (now < nextSummaryMs_) return;

    nextSummaryMs_ = now + kSummaryPeriodMs;
    sendAt("AT+S");
    summaryTxCount_++;
}

// Console/health tick: request AT+V if needed, compute warn thresholds, reset counters.
void SerialRobotDriver::tickConsole(uint64_t now) {
    if (now < nextConsoleMs_) return;

    nextConsoleMs_ = now + kConsolePeriodMs;
    if (mcuConnected_ && mcuFirmwareName_.empty()) {
        sendAt("AT+V");
    }
    if (motorTxCount_ > 0 && motorRxCount_ == 0) {
        std::cerr << "[SRD] WARN: MCU not responding — resetting ticks\n";
        resetTicks_   = true;
        mcuConnected_ = false;
    }
    const int expectedMotorHz = expectedHzFromPeriodMs(motorPeriodMs_);
    if (motorRxCount_ < warnBelowHz(expectedMotorHz)) {
        std::cerr << "[SRD] WARN: motor freq " << motorRxCount_
                  << "/" << expectedMotorHz
                  << "  summary " << summaryRxCount_ << "/2\n";
    }
    motorTxCount_ = motorRxCount_ = summaryTxCount_ = summaryRxCount_ = 0;
}

// CPU temperature tick + fan hysteresis control.
void SerialRobotDriver::tickCpuTemp(uint64_t now) {
    if (now < nextTempMs_) return;

    nextTempMs_ = now + kTempPeriodMs;
    updateCpuTemp();
    if      (cpuTemp_ < 60.0f) setFanPower(false);
    else if (cpuTemp_ > 65.0f) setFanPower(true);
}

// WiFi state tick (reads wpa_state and runs recovery policy).
void SerialRobotDriver::tickWifi(uint64_t now) {
    if (now < nextWifiMs_) return;

    nextWifiMs_ = now + kWifiPeriodMs;
    updateWifi();
}

// LED tick (WiFi LED reflects connection state).
void SerialRobotDriver::tickLed(uint64_t now) {
    if (now < nextLedMs_) return;

    nextLedMs_ = now + kLedPeriodMs;
    updateWifiLed();
}

// Shutdown sequencing tick (grace period then shutdown).
void SerialRobotDriver::tickShutdown(uint64_t now) {
    if (!shutdownPending_ || now < shutdownAt_) return;

    shutdownAt_ = now + 10000;
    std::cerr << "[SRD] LINUX SHUTDOWN\n";
    setFanPower(false);
    std::system("shutdown now");
}

// ── HardwareInterface: motor ──────────────────────────────────────────────────

// Latch PWM setpoints. Applied on next AT+M tick; optional inversion via config.
void SerialRobotDriver::setMotorPwm(int left, int right, int mow) {
    if (config_->get<bool>("invert_left_motor", false)) left = -left;
    if (config_->get<bool>("invert_right_motor", false)) right = -right;
    pwmLeft_  = left;
    pwmRight_ = right;
    pwmMow_   = mow;
}

// Alfred: no-op (MCU recovers fault internally).
void SerialRobotDriver::resetMotorFault() {
    // Alfred STM32 auto-recovers from motor fault — nothing to send.
    std::cerr << "[SRD] resetMotorFault called (no-op on Alfred)\n";
}

// ── HardwareInterface: sensors ────────────────────────────────────────────────

// Return encoder tick deltas since last call; handles reconnect/reset and wraparound.
OdometryData SerialRobotDriver::readOdometry() {
    OdometryData data;
    data.mcuConnected = mcuConnected_;

    if (!mcuConnected_) {
        resetTicks_ = true;
        return data;  // zeros
    }

    if (resetTicks_) {
        resetTicks_    = false;
        lastTicksLeft_  = rawTicksLeft_;
        lastTicksRight_ = rawTicksRight_;
        lastTicksMow_   = rawTicksMow_;
        return data;  // zeros on first call after reconnect
    }

    // BUG-05: cast to long before subtracting to handle unsigned 32-bit wraparound
    const long dLeft  = static_cast<long>(rawTicksLeft_)  - static_cast<long>(lastTicksLeft_);
    const long dRight = static_cast<long>(rawTicksRight_) - static_cast<long>(lastTicksRight_);
    const long dMow   = static_cast<long>(rawTicksMow_)   - static_cast<long>(lastTicksMow_);

    // Sanity clamp: >1000 ticks in one cycle (20 ms) is physically impossible
    // Allow negative deltas for backward/inverted motion.
    data.leftTicks  = (std::abs(dLeft)  <= 1000) ? static_cast<int>(dLeft)  : 0;
    data.rightTicks = (std::abs(dRight) <= 1000) ? static_cast<int>(dRight) : 0;
    data.mowTicks   = (std::abs(dMow)   <= 1000) ? static_cast<int>(dMow)   : 0;

    lastTicksLeft_  = rawTicksLeft_;
    lastTicksRight_ = rawTicksRight_;
    lastTicksMow_   = rawTicksMow_;

    return data;
}

// Return last sensor snapshot decoded from incoming AT frames.
SensorData SerialRobotDriver::readSensors() {
    return sensors_;
}

// Return last battery snapshot decoded from incoming AT frames.
BatteryData SerialRobotDriver::readBattery() {
    return battery_;
}

// Return last IMU snapshot (or invalid/zero struct if IMU absent).
ImuData SerialRobotDriver::readImu() {
    if (imu_) return imu_->getData();
    return {};
}

// Start IMU calibration routine (bias estimation).
void SerialRobotDriver::calibrateImu() {
    if (imu_) imu_->startCalibration();
}

// ── HardwareInterface: actuators ──────────────────────────────────────────────

// Drive buzzer via EX2 port expander.
void SerialRobotDriver::setBuzzer(bool on) {
    if (ex2_) {
        selectLegacyChannel();
        if (!ex2_->setPin(1, 1, on)) {
            static uint64_t lastBuzzerWarn = 0;
            if (nowMs() - lastBuzzerWarn > 10000) {
                std::cerr << "[SRD] WARN: buzzer update failed (I2C error on EX2)\n";
                lastBuzzerWarn = nowMs();
            }
        }
    }
}

// Drive panel LED via EX3 port expander.
void SerialRobotDriver::setLed(LedId id, LedState state) {
    writeLed(id, state);
}

// keepPowerOn(false) arms shutdown grace period; keepPowerOn(true) cancels it.
void SerialRobotDriver::keepPowerOn(bool flag) {
    if (flag) {
        shutdownPending_ = false;
        shutdownAt_      = 0;
    } else {
        if (!shutdownPending_) {
            shutdownPending_ = true;
            shutdownAt_      = nowMs() + 5000;  // 5 s grace period
            // Keep a clear red system status while the shutdown grace period runs.
            writeLed(LedId::LED_1, LedState::OFF);
            writeLed(LedId::LED_2, LedState::RED);
            writeLed(LedId::LED_3, LedState::OFF);
        }
    }
}

// ── HardwareInterface: system info ───────────────────────────────────────────

// Last measured CPU temperature (°C).
float SerialRobotDriver::getCpuTemperature() { return cpuTemp_; }

// Identity strings discovered during init()/AT+V handshake.
std::string SerialRobotDriver::getRobotId()           { return robotId_;           }
std::string SerialRobotDriver::getMcuFirmwareName()   { return mcuFirmwareName_;   }
std::string SerialRobotDriver::getMcuFirmwareVersion(){ return mcuFirmwareVersion_; }

// ── AT protocol — send ────────────────────────────────────────────────────────

// Compute additive checksum over raw command string (matches RM18 cmdAnswer()).
uint8_t SerialRobotDriver::calcCrc(const std::string& s) {
    uint8_t crc = 0;
    for (unsigned char c : s) crc += c;
    return crc;
}

// Send one AT command with appended CRC trailer and CRLF.
void SerialRobotDriver::sendAt(const std::string& cmd) {
    char crcBuf[12];
    std::snprintf(crcBuf, sizeof(crcBuf), ",0x%02X\r\n", calcCrc(cmd));
    const std::string frame = cmd + crcBuf;
    if (serialDebug_) {
        std::cerr << "[SRD] TX " << frame;
    }
    uart_->writeStr(frame.c_str());
}

// ── AT protocol — receive ─────────────────────────────────────────────────────

// Poll UART, accumulate into rxBuf_, dispatch frames on CR/LF or gap/CRC-framer.
void SerialRobotDriver::pollRx() {
    const uint64_t now = nowMs();
    bool sawData = false;

    if (!uart_->waitReadable(1)) {
        if (!rxBuf_.empty() && rxLastByteMs_ != 0 && (now - rxLastByteMs_) >= rxGapMs_) {
            if (serialDebug_) {
                std::cerr << "[SRD] RX frame(gap): " << rxBuf_ << '\n';
            }
            dispatchFrame(std::move(rxBuf_));
            rxBuf_.clear();
        }
        return;
    }

    while (true) {
        uint8_t buf[256];
        int n = uart_->read(buf, sizeof(buf));

        if (n < 0) {
            std::cerr << "[SRD] WARN: UART read failed\n";
            break;
        }
        if (n == 0) {
            break;
        }

        sawData = true;
        rxLastByteMs_ = now;
        if (serialDebug_) {
            logSerialBytes("[SRD] RX", buf, static_cast<size_t>(n));
        }

        for (int i = 0; i < n; i++) {
            char ch = static_cast<char>(buf[i]);
            if (ch == '\r' || ch == '\n') {
                if (!rxBuf_.empty()) {
                    if (serialDebug_) {
                        std::cerr << "[SRD] RX frame(eol): " << rxBuf_ << '\n';
                    }
                    dispatchFrame(std::move(rxBuf_));
                    rxBuf_.clear();
                }
            } else if (rxBuf_.size() < 500) {
                rxBuf_ += ch;
            }
        }

        drainFramedRx();
    }

    if (!sawData && !rxBuf_.empty() && rxLastByteMs_ != 0 && (now - rxLastByteMs_) >= rxGapMs_) {
        if (serialDebug_) {
            std::cerr << "[SRD] RX frame(gap): " << rxBuf_ << '\n';
        }
        dispatchFrame(std::move(rxBuf_));
        rxBuf_.clear();
    }
}

// Extract back-to-back frames by scanning for ",0xNN" trailers inside rxBuf_.
void SerialRobotDriver::drainFramedRx() {
    // Alfred/RM18 replies can arrive as back-to-back frames without CR/LF.
    // Detect the CRC trailer ",0xNN" and dispatch each complete frame directly
    // from the byte stream.
    while (true) {
        if (rxBuf_.size() < 6) return;

        std::size_t start = rxBuf_.find_first_of("MSV");
        if (start == std::string::npos) {
            rxBuf_.clear();
            return;
        }
        if (start > 0) {
            rxBuf_.erase(0, start);
        }

        const std::size_t crcPos = rxBuf_.find(",0x");
        if (crcPos == std::string::npos) return;
        if (crcPos + 5 > rxBuf_.size()) return;  // wait for two hex digits

        const auto isHex = [](char c) {
            return (c >= '0' && c <= '9')
                || (c >= 'a' && c <= 'f')
                || (c >= 'A' && c <= 'F');
        };
        if (!isHex(rxBuf_[crcPos + 3]) || !isHex(rxBuf_[crcPos + 4])) {
            // False positive — skip this marker and keep scanning.
            rxBuf_.erase(0, crcPos + 1);
            continue;
        }

        std::string frame = rxBuf_.substr(0, crcPos + 5);
        rxBuf_.erase(0, crcPos + 5);
        if (serialDebug_) {
            std::cerr << "[SRD] RX frame(crc): " << frame << '\n';
        }
        dispatchFrame(std::move(frame));
    }
}

// Verify additive CRC; on success strips trailing ",0xNN" from frame.
bool SerialRobotDriver::verifyCrc(std::string& frame) {
    const auto idx = frame.rfind(',');
    if (idx == std::string::npos || idx == 0) return false;

    uint8_t expected = 0;
    for (size_t i = 0; i < idx; i++) expected += static_cast<uint8_t>(frame[i]);

    const std::string crcStr = frame.substr(idx + 1);
    const long received = std::strtol(crcStr.c_str(), nullptr, 16);
    if (expected != static_cast<uint8_t>(received)) return false;

    frame = frame.substr(0, idx);  // strip CRC suffix
    return true;
}

// Validate CRC then route frame by type prefix (M/S/V).
void SerialRobotDriver::dispatchFrame(std::string frame) {
    if (frame.size() < 4) return;
    if (!verifyCrc(frame)) {
        std::cerr << "[SRD] CRC error on: " << frame << '\n';
        return;
    }
    switch (frame[0]) {
        case 'M': parseMotorFrame  (frame); break;
        case 'S': parseSummaryFrame(frame); break;
        case 'V': parseVersionFrame(frame); break;
        default: break;
    }
}

// ── AT frame parsers ──────────────────────────────────────────────────────────

// Split comma-separated string into tokens. Token 0 = frame type char ('M'/'S'/'V').
static std::vector<std::string> csvSplit(const std::string& s) {
    std::vector<std::string> out;
    std::string tok;
    for (char c : s) {
        if (c == ',') { out.push_back(tok); tok.clear(); }
        else          { tok += c; }
    }
    out.push_back(tok);
    return out;
}

int          SerialRobotDriver::fieldInt  (const std::string& s) { return std::stoi(s);          }
unsigned long SerialRobotDriver::fieldULong(const std::string& s) { return std::stoul(s);  }  // BUG-003 fix
float        SerialRobotDriver::fieldFloat(const std::string& s) { return std::stof(s);          }

// Parse motor response frame; supports compact v1.1.25+ and legacy schema.
void SerialRobotDriver::parseMotorFrame(const std::string& frame) {
    const auto f = csvSplit(frame);
    try {
        const auto applyCompact = [&]() {
            rawTicksRight_ += fieldULong(f[1]);
            rawTicksLeft_  += fieldULong(f[2]);
            rawTicksMow_   += fieldULong(f[3]);
            setChargeVoltage(static_cast<float>(fieldInt(f[4])) / 100.0f);

            const unsigned int flags = static_cast<unsigned int>(fieldULong(f[5]));
            const bool bumperCombined = (flags & (1U << 0)) != 0;
            const bool bumperLeft = (flags & (1U << 4)) != 0;
            const bool bumperRight = (flags & (1U << 5)) != 0;
            sensors_.bumperLeft = bumperLeft || (!bumperRight && bumperCombined);
            sensors_.bumperRight = bumperRight;
            sensors_.lift = (flags & (1U << 1)) != 0;
            sensors_.stopButton = (flags & (1U << 2)) != 0;
            setMotorFaultFast((flags & (1U << 3)) != 0);
        };

        const auto applyLegacy = [&]() {
            if (f.size() > 1) rawTicksRight_ = fieldULong(f[1]);
            if (f.size() > 2) rawTicksLeft_  = fieldULong(f[2]);
            if (f.size() > 3) rawTicksMow_   = fieldULong(f[3]);
            if (f.size() > 4) setChargeVoltage(fieldFloat(f[4]));
            if (f.size() > 5) {
                sensors_.bumperLeft  = (fieldInt(f[5]) != 0);
                sensors_.bumperRight = false;
            }
            if (f.size() > 6) sensors_.lift       = (fieldInt(f[6]) != 0);
            if (f.size() > 7) sensors_.stopButton = (fieldInt(f[7]) != 0);
            if (f.size() > 8) setMotorFaultFast(fieldInt(f[8]) != 0);
            if (f.size() > 9) sensors_.bumperLeft  = (fieldInt(f[9])  != 0);
            if (f.size() > 10) sensors_.bumperRight = (fieldInt(f[10]) != 0);
        };

        switch (detectMotorFrameSchema(f)) {
            case MotorFrameSchema::Compact: applyCompact(); break;
            case MotorFrameSchema::Legacy:  applyLegacy();  break;
        }
    } catch (...) {
        return;  // malformed frame — ignore
    }

    updateLatchedMotorFault();
    mcuConnected_ = true;
    motorRxCount_++;
    if (serialDebug_) {
        std::cerr << "[SRD] M parsed"
                  << " ticksL=" << rawTicksLeft_
                  << " ticksR=" << rawTicksRight_
                  << " ticksM=" << rawTicksMow_
                  << " chgV=" << battery_.chargeVoltage
                  << " charger=" << (battery_.chargerConnected ? 1 : 0)
                  << " stop=" << (sensors_.stopButton ? 1 : 0)
                  << " lift=" << (sensors_.lift ? 1 : 0)
                  << " bumpL=" << (sensors_.bumperLeft ? 1 : 0)
                  << " bumpR=" << (sensors_.bumperRight ? 1 : 0)
                  << '\n';
    }
}

// Parse summary response frame (2 Hz) and update battery/sensor snapshots.
void SerialRobotDriver::parseSummaryFrame(const std::string& frame) {
    // S,f1..f14 legacy + optional f15..f18 detailed mow-fault causes (2 Hz)
    const auto f = csvSplit(frame);
    try {
        if (f.size() > 1)  battery_.voltage       = fieldFloat(f[1]);
        if (f.size() > 2)  setChargeVoltage(fieldFloat(f[2]));
        if (f.size() > 3)  battery_.chargeCurrent  = fieldFloat(f[3]);
        if (f.size() > 4)  sensors_.lift            = (fieldInt(f[4]) != 0);
        if (f.size() > 5)  {
            sensors_.bumperLeft  = (fieldInt(f[5]) != 0);
            sensors_.bumperRight = false;
        }
        if (f.size() > 6)  sensors_.rain      = (fieldInt(f[6])  != 0);
        setMotorFaultSummary((f.size() > 7) ? (fieldInt(f[7]) != 0) : false);
        // f[8]=mowCurr, f[9]=leftCurr, f[10]=rightCurr — not in SensorData/BatteryData
        if (f.size() > 11) battery_.batteryTemp   = fieldFloat(f[11]);
        sensors_.mowOvCheck = (f.size() > 12) ? (fieldInt(f[12]) != 0) : false;
        if (sensors_.mowOvCheck) setMotorFaultSummary(true);
        if (f.size() > 13) sensors_.bumperLeft  = (fieldInt(f[13]) != 0);
        if (f.size() > 14) sensors_.bumperRight = (fieldInt(f[14]) != 0);
        sensors_.mowFaultPin = (f.size() > 15) ? (fieldInt(f[15]) != 0) : false;
        sensors_.mowOverload = (f.size() > 16) ? (fieldInt(f[16]) != 0) : false;
        sensors_.mowPermanentFault = (f.size() > 17) ? (fieldInt(f[17]) != 0) : false;
        if (f.size() > 18) sensors_.mowOvCheck = (fieldInt(f[18]) != 0);
        if (sensors_.mowFaultPin || sensors_.mowOverload
            || sensors_.mowPermanentFault || sensors_.mowOvCheck) {
            setMotorFaultSummary(true);
        }
    } catch (...) {
        return;
    }

    updateLatchedMotorFault();
    summaryRxCount_++;
    if (serialDebug_) {
        std::cerr << "[SRD] S parsed"
                  << " batV=" << battery_.voltage
                  << " chgV=" << battery_.chargeVoltage
                  << " chgA=" << battery_.chargeCurrent
                  << " charger=" << (battery_.chargerConnected ? 1 : 0)
                  << " rain=" << (sensors_.rain ? 1 : 0)
                  << " lift=" << (sensors_.lift ? 1 : 0)
                  << " fault=" << (sensors_.motorFault ? 1 : 0)
                  << " mowPin=" << (sensors_.mowFaultPin ? 1 : 0)
                  << " overload=" << (sensors_.mowOverload ? 1 : 0)
                  << " perm=" << (sensors_.mowPermanentFault ? 1 : 0)
                  << " ov=" << (sensors_.mowOvCheck ? 1 : 0)
                  << " bumpL=" << (sensors_.bumperLeft ? 1 : 0)
                  << " bumpR=" << (sensors_.bumperRight ? 1 : 0)
                  << '\n';
    }
}

// Parse version response frame (firmware name/version).
void SerialRobotDriver::parseVersionFrame(const std::string& frame) {
    // V,name,version
    const auto f = csvSplit(frame);
    if (f.size() > 1) mcuFirmwareName_    = f[1];
    if (f.size() > 2) mcuFirmwareVersion_ = f[2];
    std::cerr << "[SRD] MCU firmware: " << mcuFirmwareName_
              << " v" << mcuFirmwareVersion_ << '\n';
}

// Debounce charger-connected state changes derived from charge voltage.
void SerialRobotDriver::updateChargerConnected(bool rawConnected) {
    rawChargerConnected_ = rawConnected;

    if (rawConnected) {
        ++chargerConnectedStableCount_;
        chargerDisconnectedStableCount_ = 0;
        if (!battery_.chargerConnected &&
            chargerConnectedStableCount_ >= kChargerDebounceSamples) {
            battery_.chargerConnected = true;
        }
        return;
    }

    ++chargerDisconnectedStableCount_;
    chargerConnectedStableCount_ = 0;
    if (battery_.chargerConnected &&
        chargerDisconnectedStableCount_ >= kChargerDebounceSamples) {
        battery_.chargerConnected = false;
    }
}

// Set charge voltage and update chargerConnected debounce state.
void SerialRobotDriver::setChargeVoltage(float v) {
    battery_.chargeVoltage = v;
    updateChargerConnected(chargerConnectedFromVoltage(v, chargerConnectedVoltageV_));
}

// Set fast fault bit (from AT+M).
void SerialRobotDriver::setMotorFaultFast(bool fault) {
    motorFaultFast_ = fault;
}

// Set summary fault bit (from AT+S).
void SerialRobotDriver::setMotorFaultSummary(bool fault) {
    motorFaultSummary_ = fault;
}

// Update exported sensors_.motorFault from fast+summary latch bits.
void SerialRobotDriver::updateLatchedMotorFault() {
    sensors_.motorFault = (motorFaultFast_ || motorFaultSummary_);
}

// ── Hardware helpers ──────────────────────────────────────────────────────────

// Select IMU mux channel (exclusive) before accessing IMU over I2C.
void SerialRobotDriver::selectImuChannel() {
    if (mux_) {
        bool success = mux_->selectChannel(static_cast<uint8_t>(imuMuxChannel_));
        if (!success && logger_) {
            logger_->error("SRD", "Failed to select IMU mux channel " + std::to_string(imuMuxChannel_));
        }
        // Add settling time for TCA9548A mux switch
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// Select EX3 mux channel (exclusive) before accessing LED panel expander.
void SerialRobotDriver::selectEx3Channel() {
    if (mux_) {
        bool success = mux_->selectChannel(static_cast<uint8_t>(ex3MuxChannel_));
        if (!success && logger_) {
            logger_->error("SRD", "Failed to select EX3 mux channel " + std::to_string(ex3MuxChannel_));
        }
        // Add settling time for TCA9548A mux switch
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// Select legacy mux channel (exclusive) for EX1/EX2 devices (fan, buzzer, etc).
void SerialRobotDriver::selectLegacyChannel() {
    if (mux_) {
        bool success = mux_->selectChannel(static_cast<uint8_t>(legacyMuxChannel_));
        if (!success && logger_) {
            logger_->error("SRD", "Failed to select legacy mux channel " + std::to_string(legacyMuxChannel_));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// Toggle fan power pin via EX1 port expander.
void SerialRobotDriver::setFanPower(bool on) {
    selectLegacyChannel();
    if (ex1_ && !ex1_->setPin(1, 7, on)) {
        static uint64_t lastFanWarn = 0;
        if (nowMs() - lastFanWarn > 10000) {
            std::cerr << "[SRD] WARN: fan power update failed (I2C error on EX1)\n";
            lastFanWarn = nowMs();
        }
    }
}

// Write one LED color state via EX3 expander; returns false on I2C failure.
bool SerialRobotDriver::writeLed(LedId id, LedState state) {
    if (!ex3_) return false;
    selectEx3Channel();  // exclusive: only LED channel active during EX3 access
    uint8_t gPin, rPin;
    switch (id) {
        case LedId::LED_1: gPin = 0; rPin = 1; break;  // WiFi  (bottom)
        case LedId::LED_2: gPin = 2; rPin = 3; break;  // Error (top)
        case LedId::LED_3: gPin = 4; rPin = 5; break;  // GPS   (middle)
        default: return false;
    }
    
    bool ok = true;
    if (!ex3_->setPin(0, gPin, state == LedState::GREEN)) ok = false;
    if (!ex3_->setPin(0, rPin, state == LedState::RED)) ok = false;
    
    if (!ok) {
        static uint64_t lastLedWarn = 0;
        if (nowMs() - lastLedWarn > 10000) {
            std::cerr << "[SRD] WARN: LED update failed (I2C error on EX3)\n";
            lastLedWarn = nowMs();
        }
    }
    return ok;
}

// Read CPU temperature from sysfs.
void SerialRobotDriver::updateCpuTemp() {
    const std::string s = shellRead("cat /sys/class/thermal/thermal_zone0/temp");
    if (!s.empty()) {
        try { cpuTemp_ = std::stof(s) / 1000.0f; } catch (...) {}
    }
}

// Read WiFi state from wpa_cli, log transitions, run recovery, update flags.
void SerialRobotDriver::updateWifi() {
    // wpa_state values: COMPLETED, DISCONNECTED, SCANNING, INACTIVE, …
    const std::string cmd = "wpa_cli -i " + wifiInterface_
        + " status 2>/dev/null | grep wpa_state | cut -d '=' -f2";
    wifiState_ = trim(shellRead(cmd.c_str()));
    if (wifiState_.empty()) {
        wifiState_ = "UNKNOWN";
    }
    if (wifiState_ != wifiLastLoggedState_) {
        std::cerr << "[SRD] WiFi state(" << wifiInterface_ << "): " << wifiState_ << '\n';
        wifiLastLoggedState_ = wifiState_;
    }
    wifiConnected_ = (wifiState_ == "COMPLETED");
    wifiInactive_  = (wifiState_ == "INACTIVE");
    recoverWifi();
}

// Recovery policy: periodic scan + reconnect when not connected.
void SerialRobotDriver::recoverWifi() {
    if (wifiConnected_) {
        wifiLastScanMs_ = 0;
        wifiLastReconnectMs_ = 0;
        return;
    }

    const uint64_t now = nowMs();
    if (now - wifiLastScanMs_ >= kWifiScanIntervalMs) {
        wifiLastScanMs_ = now;
        const bool ok = shellExec("wpa_cli -i " + wifiInterface_ + " scan >/dev/null 2>&1");
        std::cerr << "[SRD] WiFi recovery(" << wifiInterface_ << "): active scan requested"
                  << " while state=" << wifiState_ << (ok ? "" : " FAILED") << '\n';
    }
    if (now - wifiLastReconnectMs_ >= kWifiReconnectIntervalMs) {
        wifiLastReconnectMs_ = now;
        const bool ok = shellExec("wpa_cli -i " + wifiInterface_ + " reconnect >/dev/null 2>&1");
        std::cerr << "[SRD] WiFi recovery(" << wifiInterface_ << "): reconnect requested"
                  << " while state=" << wifiState_ << (ok ? "" : " FAILED") << '\n';
    }
}

// Drive WiFi LED based on wifiConnected_/wifiInactive_.
void SerialRobotDriver::updateWifiLed() {
    if      (wifiConnected_) writeLed(LedId::LED_1, LedState::GREEN);
    else if (wifiInactive_)  writeLed(LedId::LED_1, LedState::RED);
    else                     writeLed(LedId::LED_1, LedState::OFF);
}

// ── Utility ───────────────────────────────────────────────────────────────────

// Monotonic time base used for driver scheduling.
uint64_t SerialRobotDriver::nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// Run shell command and collect stdout (best-effort).
std::string SerialRobotDriver::shellRead(const char* cmd) {
    FILE* f = ::popen(cmd, "r");
    if (!f) return {};
    char buf[256];
    std::string result;
    while (::fgets(buf, sizeof(buf), f)) result += buf;
    ::pclose(f);
    return result;
}

// Run shell command; returns true on exit code 0.
bool SerialRobotDriver::shellExec(const std::string& cmd) {
    return std::system(cmd.c_str()) == 0;
}

// Trim whitespace/newlines from both ends.
std::string SerialRobotDriver::trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' ||
                          s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t first = 0;
    while (first < s.size() && (s[first] == ' ' || s[first] == '\t' ||
                                s[first] == '\n' || s[first] == '\r')) {
        ++first;
    }
    return s.substr(first);
}

// Debug helper: print bytes in hex plus ASCII.
void SerialRobotDriver::logSerialBytes(const char* prefix, const uint8_t* buf, size_t len) const {
    std::ostringstream oss;
    oss << prefix << ' ';
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(buf[i]) << ' ';
    }
    oss << std::dec << "| ";
    for (size_t i = 0; i < len; ++i) {
        const char ch = static_cast<char>(buf[i]);
        oss << ((ch >= 32 && ch <= 126) ? ch : '.');
    }
    std::cerr << oss.str() << '\n';
}

// Read I2C address from config string ("0x70") or int fallback; clamp to 7-bit.
uint8_t SerialRobotDriver::configI2cAddr(const std::string& key, uint8_t fallback) const {
    const std::string s = config_->get<std::string>(key, "");
    if (!s.empty()) {
        char* end = nullptr;
        const long v = std::strtol(s.c_str(), &end, 0);
        if (end && *end == '\0' && v >= 0 && v <= 0x7F) {
            return static_cast<uint8_t>(v);
        }
    }

    const int v = config_->get<int>(key, -1);
    if (v >= 0 && v <= 0x7F) {
        return static_cast<uint8_t>(v);
    }
    return fallback;
}

} // namespace sunray
