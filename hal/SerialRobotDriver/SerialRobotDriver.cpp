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
#include <vector>

namespace sunray {

// ── Construction ──────────────────────────────────────────────────────────────

SerialRobotDriver::SerialRobotDriver(std::shared_ptr<Config> config)
    : config_(std::move(config))
{}

SerialRobotDriver::~SerialRobotDriver() {
    // Platform objects closed by their own destructors.
    // Fan stays in whatever state it was — keepPowerOn handles graceful shutdown.
}

// ── HardwareInterface: init() ─────────────────────────────────────────────────

bool SerialRobotDriver::init() {
    serialDebug_ = config_->get<bool>("serial_debug", false);
    rxGapMs_     = static_cast<uint64_t>(config_->get<int>("serial_rx_gap_ms", 50));

    // ── UART to STM32 ─────────────────────────────────────────────────────────
    const std::string port = config_->get<std::string>("driver_port", "/dev/ttyS0");
    const int         baud = config_->get<int>        ("driver_baud", 115200);
    try {
        uart_ = std::make_unique<platform::Serial>(port, static_cast<unsigned>(baud));
    } catch (const std::runtime_error& e) {
        std::cerr << "[SRD] UART open failed: " << e.what() << '\n';
        return false;
    }

    // ── I2C bus ───────────────────────────────────────────────────────────────
    const std::string i2cBus = config_->get<std::string>("i2c_bus", "/dev/i2c-1");
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
        std::cerr << "[SRD] I2C open failed: " << e.what() << '\n';
        return false;
    }

    // ── Robot ID (eth0 MAC address) ───────────────────────────────────────────
    robotId_ = shellRead("ip link show eth0 | grep link/ether | awk '{print $2}'");
    if (!robotId_.empty() && robotId_.back() == '\n') robotId_.pop_back();
    std::cerr << "[SRD] Robot ID: " << robotId_ << '\n';

    // ── IMU + Fan power via EX1 ───────────────────────────────────────────────
    // PCA9555 EX1: IO1.6 = IMU power, IO1.7 = Fan power
    ex1_->setPin(1, 6, true);   // IMU ON
    setFanPower(true);           // Fan ON

    if (mux_) {
        uint8_t mask = 0;
        const auto addChannel = [&mask](int channel) {
            if (channel >= 0 && channel <= 7) {
                mask = static_cast<uint8_t>(mask | (1u << channel));
            }
        };
        addChannel(config_->get<int>("i2c_mux_legacy_channel", 0));
        addChannel(config_->get<int>("imu_mux_channel", 4));
        addChannel(config_->get<int>("eeprom_mux_channel", 5));
        addChannel(config_->get<int>("adc_mux_channel", 6));
        if (!mux_->setEnabledMask(mask)) {
            std::cerr << "[SRD] WARN: failed to configure TCA9548A mux\n";
        }
    }

    // ── MPU-6050 IMU ──────────────────────────────────────────────────────────
    const uint8_t imuAddr = configI2cAddr("imu_i2c_addr", 0x69);
    imu_ = std::make_unique<Mpu6050Driver>(*i2c_, imuAddr);
    bool imuOk = imu_->init();
    if (!imuOk && imuAddr != 0x68) {
        imu_ = std::make_unique<Mpu6050Driver>(*i2c_, 0x68);
        imuOk = imu_->init();
    }
    if (!imuOk) {
        std::cerr << "[SRD] IMU init failed (maybe not connected to /dev/i2c-1?)\n";
        // Do not return false — robot can drive without IMU (GPS+Odo only)
    }

    // ── Panel LEDs — startup green ────────────────────────────────────────────
    bool ledOk = writeLed(LedId::LED_1, LedState::GREEN);
    writeLed(LedId::LED_2, LedState::GREEN);
    writeLed(LedId::LED_3, LedState::GREEN);
    if (!ledOk) {
        std::cerr << "[SRD] LED panel not responding — assuming not installed\n";
    }

    // ── Initial battery data default (safe fallback if MCU not yet connected) ─
    battery_.voltage = 28.0f;  // prevents immediate shutdown on startup

    // Avoid an immediate false communication warning on the very first run().
    const uint64_t now = nowMs();
    nextMotorMs_   = now + 20;
    nextSummaryMs_ = now + 500;
    nextConsoleMs_ = now + 1000;
    nextLedMs_     = now + 3000;
    nextTempMs_    = now + 59000;
    nextWifiMs_    = now + 7000;

    std::cerr << "[SRD] init complete — UART=" << port
              << " @" << baud << " I2C=" << i2cBus << '\n';
    return true;
}

// ── HardwareInterface: run() ──────────────────────────────────────────────────

void SerialRobotDriver::run() {
    if (!uart_) return;

    pollRx();

    const uint64_t now = nowMs();

    // 50 Hz — IMU update
    if (imu_ && now >= nextImuMs_) {
        const float dt = (lastImuMs_ == 0) ? 0.02f : (now - lastImuMs_) / 1000.0f;
        imu_->update(dt);
        lastImuMs_ = now;
        nextImuMs_ = now + 20;
    }

    // 50 Hz — AT+M motor command
    if (now >= nextMotorMs_) {
        nextMotorMs_ = now + 20;
        // BUG-07: send rightPwm as field1, leftPwm as field2 (cross-wiring compensation)
        std::string cmd = "AT+M," + std::to_string(pwmRight_)
                        + ","     + std::to_string(pwmLeft_)
                        + ","     + std::to_string(pwmMow_);
        sendAt(cmd);
        motorTxCount_++;
    }

    // 2 Hz — AT+S summary
    if (now >= nextSummaryMs_) {
        nextSummaryMs_ = now + 500;
        sendAt("AT+S");
        summaryTxCount_++;
    }

    // 1 Hz — comms health check + AT+V if not yet received
    if (now >= nextConsoleMs_) {
        nextConsoleMs_ = now + 1000;
        if (mcuConnected_ && mcuFirmwareName_.empty()) {
            sendAt("AT+V");
        }
        if (motorTxCount_ > 0 && motorRxCount_ == 0) {
            std::cerr << "[SRD] WARN: MCU not responding — resetting ticks\n";
            resetTicks_   = true;
            mcuConnected_ = false;
        }
        if (motorRxCount_ < 30) {
            std::cerr << "[SRD] WARN: motor freq " << motorRxCount_
                      << "/50  summary " << summaryRxCount_ << "/2\n";
        }
        motorTxCount_ = motorRxCount_ = summaryTxCount_ = summaryRxCount_ = 0;
    }

    // 3 s — WiFi LED update
    if (now >= nextLedMs_) {
        nextLedMs_ = now + 3000;
        updateWifiLed();
    }

    // 59 s — CPU temperature + fan control
    if (now >= nextTempMs_) {
        nextTempMs_ = now + 59000;
        updateCpuTemp();
        if      (cpuTemp_ < 60.0f) setFanPower(false);
        else if (cpuTemp_ > 65.0f) setFanPower(true);
    }

    // 7 s — WiFi state poll (used by updateWifiLed)
    if (now >= nextWifiMs_) {
        nextWifiMs_ = now + 7000;
        updateWifi();
    }

    // Shutdown sequencing
    if (shutdownPending_ && now >= shutdownAt_) {
        shutdownAt_ = now + 10000;  // re-arm in case shutdown command is slow
        std::cerr << "[SRD] LINUX SHUTDOWN\n";
        setFanPower(false);
        std::system("shutdown now");
    }
}

// ── HardwareInterface: motor ──────────────────────────────────────────────────

void SerialRobotDriver::setMotorPwm(int left, int right, int mow) {
    pwmLeft_  = left;
    pwmRight_ = right;
    pwmMow_   = mow;
}

void SerialRobotDriver::resetMotorFault() {
    // Alfred STM32 auto-recovers from motor fault — nothing to send.
    std::cerr << "[SRD] resetMotorFault called (no-op on Alfred)\n";
}

// ── HardwareInterface: sensors ────────────────────────────────────────────────

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
    data.leftTicks  = (dLeft  >= 0 && dLeft  <= 1000) ? static_cast<int>(dLeft)  : 0;
    data.rightTicks = (dRight >= 0 && dRight <= 1000) ? static_cast<int>(dRight) : 0;
    data.mowTicks   = (dMow   >= 0 && dMow   <= 1000) ? static_cast<int>(dMow)   : 0;

    lastTicksLeft_  = rawTicksLeft_;
    lastTicksRight_ = rawTicksRight_;
    lastTicksMow_   = rawTicksMow_;

    return data;
}

SensorData SerialRobotDriver::readSensors() {
    return sensors_;
}

BatteryData SerialRobotDriver::readBattery() {
    return battery_;
}

ImuData SerialRobotDriver::readImu() {
    if (imu_) return imu_->getData();
    return {};
}

void SerialRobotDriver::calibrateImu() {
    if (imu_) imu_->startCalibration();
}

// ── HardwareInterface: actuators ──────────────────────────────────────────────

void SerialRobotDriver::setBuzzer(bool on) {
    if (ex2_) ex2_->setPin(1, 1, on);   // EX2 IO1.1 = Buzzer
}

void SerialRobotDriver::setLed(LedId id, LedState state) {
    writeLed(id, state);
}

void SerialRobotDriver::keepPowerOn(bool flag) {
    if (flag) {
        shutdownPending_ = false;
        shutdownAt_      = 0;
    } else {
        if (!shutdownPending_) {
            shutdownPending_ = true;
            shutdownAt_      = nowMs() + 5000;  // 5 s grace period
            // All LEDs off immediately to signal imminent shutdown
            writeLed(LedId::LED_1, LedState::OFF);
            writeLed(LedId::LED_2, LedState::OFF);
            writeLed(LedId::LED_3, LedState::OFF);
        }
    }
}

// ── HardwareInterface: system info ───────────────────────────────────────────

float SerialRobotDriver::getCpuTemperature() { return cpuTemp_; }

std::string SerialRobotDriver::getRobotId()           { return robotId_;           }
std::string SerialRobotDriver::getMcuFirmwareName()   { return mcuFirmwareName_;   }
std::string SerialRobotDriver::getMcuFirmwareVersion(){ return mcuFirmwareVersion_; }

// ── AT protocol — send ────────────────────────────────────────────────────────

uint8_t SerialRobotDriver::calcCrc(const std::string& s) {
    uint8_t crc = 0;
    for (unsigned char c : s) crc += c;
    return crc;
}

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

void SerialRobotDriver::parseMotorFrame(const std::string& frame) {
    // M,f1,f2,f3,f4,f5,f6,f7,f8[,f9,f10]
    // BUG-07: STM32 sends leftTicks as f1, rightTicks as f2
    //         Pi maps f1→rawTicksRight_, f2→rawTicksLeft_ (cross-wiring compensation)
    const auto f = csvSplit(frame);
    try {
        if (f.size() > 1) rawTicksRight_ = fieldULong(f[1]);  // BUG-003 fix
        if (f.size() > 2) rawTicksLeft_  = fieldULong(f[2]);  // BUG-003 fix
        if (f.size() > 3) rawTicksMow_   = fieldULong(f[3]);  // BUG-003 fix
        if (f.size() > 4) battery_.chargeVoltage  = fieldFloat(f[4]);
        if (f.size() > 5) {
            // Legacy combined bumper field — overridden by f[9]/f[10] if present
            sensors_.bumperLeft  = (fieldInt(f[5]) != 0);
            sensors_.bumperRight = false;
        }
        if (f.size() > 6) sensors_.lift       = (fieldInt(f[6]) != 0);
        if (f.size() > 7) sensors_.stopButton = (fieldInt(f[7]) != 0);
        if (f.size() > 8) sensors_.motorFault = (fieldInt(f[8]) != 0);
        if (f.size() > 9) sensors_.bumperLeft  = (fieldInt(f[9])  != 0);  // precise
        if (f.size() > 10) sensors_.bumperRight = (fieldInt(f[10]) != 0); // precise
    } catch (...) {
        return;  // malformed frame — ignore
    }

    battery_.chargerConnected = (battery_.chargeVoltage > 2.0f);
    mcuConnected_ = true;
    motorRxCount_++;
}

void SerialRobotDriver::parseSummaryFrame(const std::string& frame) {
    // S,f1..f14  (2 Hz)
    const auto f = csvSplit(frame);
    try {
        if (f.size() > 1)  battery_.voltage       = fieldFloat(f[1]);
        if (f.size() > 2)  battery_.chargeVoltage  = fieldFloat(f[2]);
        if (f.size() > 3)  battery_.chargeCurrent  = fieldFloat(f[3]);
        if (f.size() > 4)  sensors_.lift            = (fieldInt(f[4]) != 0);
        if (f.size() > 5)  {
            sensors_.bumperLeft  = (fieldInt(f[5]) != 0);
            sensors_.bumperRight = false;
        }
        if (f.size() > 6)  sensors_.rain      = (fieldInt(f[6])  != 0);
        if (f.size() > 7)  sensors_.motorFault = (fieldInt(f[7]) != 0);
        // f[8]=mowCurr, f[9]=leftCurr, f[10]=rightCurr — not in SensorData/BatteryData
        if (f.size() > 11) battery_.batteryTemp   = fieldFloat(f[11]);
        if (f.size() > 12 && fieldInt(f[12]) != 0)
            sensors_.motorFault = true;  // IMP-01: hardware overvoltage signal
        if (f.size() > 13) sensors_.bumperLeft  = (fieldInt(f[13]) != 0);
        if (f.size() > 14) sensors_.bumperRight = (fieldInt(f[14]) != 0);
    } catch (...) {
        return;
    }

    battery_.chargerConnected = (battery_.chargeVoltage > 2.0f);
    summaryRxCount_++;
}

void SerialRobotDriver::parseVersionFrame(const std::string& frame) {
    // V,name,version
    const auto f = csvSplit(frame);
    if (f.size() > 1) mcuFirmwareName_    = f[1];
    if (f.size() > 2) mcuFirmwareVersion_ = f[2];
    std::cerr << "[SRD] MCU firmware: " << mcuFirmwareName_
              << " v" << mcuFirmwareVersion_ << '\n';
}

// ── Hardware helpers ──────────────────────────────────────────────────────────

void SerialRobotDriver::setFanPower(bool on) {
    if (ex1_) ex1_->setPin(1, 7, on);  // EX1 IO1.7 = Fan power
}

bool SerialRobotDriver::writeLed(LedId id, LedState state) {
    if (!ex3_) return false;
    uint8_t gPin, rPin;
    switch (id) {
        case LedId::LED_1: gPin = 0; rPin = 1; break;  // WiFi  (bottom)
        case LedId::LED_2: gPin = 2; rPin = 3; break;  // Error (top)
        case LedId::LED_3: gPin = 4; rPin = 5; break;  // GPS   (middle)
        default: return false;
    }
    ex3_->setPin(0, gPin, state == LedState::GREEN);
    return ex3_->setPin(0, rPin, state == LedState::RED);
}

void SerialRobotDriver::updateCpuTemp() {
    const std::string s = shellRead("cat /sys/class/thermal/thermal_zone0/temp");
    if (!s.empty()) {
        try { cpuTemp_ = std::stof(s) / 1000.0f; } catch (...) {}
    }
}

void SerialRobotDriver::updateWifi() {
    // wpa_state values: COMPLETED, DISCONNECTED, SCANNING, INACTIVE, …
    std::string s = shellRead(
        "wpa_cli -i wlan0 status 2>/dev/null | grep wpa_state | cut -d '=' -f2");
    if (!s.empty() && s.back() == '\n') s.pop_back();
    wifiConnected_ = (s == "COMPLETED");
    wifiInactive_  = (s == "INACTIVE");
}

void SerialRobotDriver::updateWifiLed() {
    if      (wifiConnected_) writeLed(LedId::LED_1, LedState::GREEN);
    else if (wifiInactive_)  writeLed(LedId::LED_1, LedState::RED);
    else                     writeLed(LedId::LED_1, LedState::OFF);
}

// ── Utility ───────────────────────────────────────────────────────────────────

uint64_t SerialRobotDriver::nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

std::string SerialRobotDriver::shellRead(const char* cmd) {
    FILE* f = ::popen(cmd, "r");
    if (!f) return {};
    char buf[256];
    std::string result;
    while (::fgets(buf, sizeof(buf), f)) result += buf;
    ::pclose(f);
    return result;
}

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
