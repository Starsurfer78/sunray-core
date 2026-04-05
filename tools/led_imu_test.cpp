// led_imu_test.cpp — Focused diagnostic for panel LEDs (EX3) and IMU (MPU-6050).
//
// Sequence:
//   1. All LEDs off
//   2. Each LED individually red  — user confirms yes/no
//   3. Each LED individually green — user confirms yes/no
//   4. Mux channel scan for IMU (WHO_AM_I on all 8 channels)
//   5. IMU init + 10 samples at 5 Hz
//
// Usage:
//   sudo ./led_imu_test
//
// I2C topology:
//   Mux TCA9548A at 0x70
//   Ch 0: EX1 (0x21, IMU power IO1.6), EX3 (0x22, LEDs port 0)
//   Ch 4: MPU-6050 at 0x69 (fallback 0x68)
//
// LED bitmask on EX3 port 0:
//   bit0 = LED1 green, bit1 = LED1 red
//   bit2 = LED2 green, bit3 = LED2 red
//   bit4 = LED3 green, bit5 = LED3 red

#include "platform/I2C.h"
#include "platform/I2cMux.h"
#include "platform/PortExpander.h"
#include "hal/Imu/Mpu6050Driver.h"
#include "core/Logger.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

// ── Konfiguration ─────────────────────────────────────────────────────────────

static constexpr const char* kI2cBus    = "/dev/i2c-1";
static constexpr uint8_t     kAddrMux   = 0x70;
static constexpr uint8_t     kAddrEx1   = 0x21;
static constexpr uint8_t     kAddrEx3   = 0x22;
static constexpr uint8_t     kAddrImu69 = 0x69;
static constexpr uint8_t     kAddrImu68 = 0x68;
static constexpr uint8_t     kMuxChLegacy = 0;
static constexpr uint8_t     kMuxChImu    = 4;

// LED-Bitmasks (EX3 port 0)
static constexpr uint8_t kLed1Red   = 0x02;
static constexpr uint8_t kLed2Red   = 0x08;
static constexpr uint8_t kLed3Red   = 0x20;
static constexpr uint8_t kLed1Green = 0x01;
static constexpr uint8_t kLed2Green = 0x04;
static constexpr uint8_t kLed3Green = 0x10;
static constexpr uint8_t kAllLeds   = 0x3F;

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────

static bool confirm(const std::string& prompt) {
    std::printf("  %s [j/n]: ", prompt.c_str());
    std::fflush(stdout);
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    return !line.empty() && (line[0] == 'j' || line[0] == 'J' || line[0] == 'y' || line[0] == 'Y');
}

static void setLeds(sunray::platform::I2cMux& mux,
                    sunray::platform::PortExpander& ex3,
                    uint8_t mask) {
    mux.selectChannel(kMuxChLegacy);
    ex3.setOutputPort(0, mask);
}

static void sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ── Phase 1: LEDs ────────────────────────────────────────────────────────────

static void phaseLeds(sunray::platform::I2cMux& mux,
                      sunray::platform::PortExpander& ex3) {
    std::printf("\n\033[1m--- LEDs ---\033[0m\n");

    // EX3 port 0: alle Pins als Ausgang konfigurieren
    mux.selectChannel(kMuxChLegacy);
    ex3.setConfigPort(0, 0x00);  // 0x00 = alle Pins Output

    // Alle LEDs aus
    setLeds(mux, ex3, 0x00);
    std::printf("  Alle LEDs aus (500 ms)...\n");
    sleepMs(500);

    // Rot einzeln
    struct LedEntry { const char* name; uint8_t mask; };
    const LedEntry reds[]   = {{"LED 1 ROT",   kLed1Red},
                                {"LED 2 ROT",   kLed2Red},
                                {"LED 3 ROT",   kLed3Red}};
    const LedEntry greens[] = {{"LED 1 GRÜN",  kLed1Green},
                                {"LED 2 GRÜN",  kLed2Green},
                                {"LED 3 GRÜN",  kLed3Green}};

    int pass = 0, fail = 0;

    std::printf("\n  -- Rot-Test --\n");
    for (const auto& e : reds) {
        setLeds(mux, ex3, e.mask);
        sleepMs(100);
        const bool ok = confirm(std::string(e.name) + " leuchtet?");
        std::printf("  %s %s\n", ok ? "[PASS]" : "[FAIL]", e.name);
        ok ? ++pass : ++fail;
        setLeds(mux, ex3, 0x00);
        sleepMs(300);
    }

    std::printf("\n  -- Grün-Test --\n");
    for (const auto& e : greens) {
        setLeds(mux, ex3, e.mask);
        sleepMs(100);
        const bool ok = confirm(std::string(e.name) + " leuchtet?");
        std::printf("  %s %s\n", ok ? "[PASS]" : "[FAIL]", e.name);
        ok ? ++pass : ++fail;
        setLeds(mux, ex3, 0x00);
        sleepMs(300);
    }

    setLeds(mux, ex3, 0x00);
    std::printf("\n  Ergebnis LEDs: %d/%d bestätigt\n", pass, pass + fail);
}

// ── Phase 2: IMU Mux-Scan ────────────────────────────────────────────────────

static int phaseImuScan(sunray::platform::I2C& bus,
                        sunray::platform::I2cMux& mux) {
    std::printf("\n\033[1m--- IMU Mux-Channel Scan ---\033[0m\n");
    std::printf("  Suche MPU-6050 (WHO_AM_I=0x68) auf allen 8 Kanälen...\n\n");

    static constexpr uint8_t kRegWhoAmI = 0x75;
    int foundChannel = -1;

    for (int ch = 0; ch < 8; ++ch) {
        mux.selectChannel(static_cast<uint8_t>(ch));
        sleepMs(5);

        // Versuche 0x69 und 0x68
        for (uint8_t addr : {kAddrImu69, kAddrImu68}) {
            uint8_t who = 0;
            const bool wOk = bus.write(addr, &kRegWhoAmI, 1);
            const bool rOk = wOk && bus.read(addr, &who, 1);

            if (rOk && who == 0x68) {
                std::printf("  ch %d, 0x%02X: WHO_AM_I=0x%02X  \033[32m<-- MPU-6050 gefunden!\033[0m\n",
                            ch, addr, who);
                if (foundChannel < 0) foundChannel = ch;
            } else if (rOk) {
                std::printf("  ch %d, 0x%02X: WHO_AM_I=0x%02X  (kein MPU-6050)\n",
                            ch, addr, who);
            }
            // Kein ACK → stille Ausgabe (nur wenn Erwartungskanal)
            else if (ch == kMuxChImu) {
                std::printf("  ch %d, 0x%02X: keine Antwort (write=%s)\n",
                            ch, addr, wOk ? "OK" : "FAIL");
            }
        }
    }

    if (foundChannel < 0) {
        std::printf("\n  [FAIL] MPU-6050 auf keinem Kanal gefunden\n");
    } else {
        std::printf("\n  [PASS] IMU gefunden auf Mux-Kanal %d\n", foundChannel);
    }

    return foundChannel;
}

// ── Phase 3: IMU Live-Test ────────────────────────────────────────────────────

static void phaseImuLive(sunray::platform::I2C& bus,
                         sunray::platform::I2cMux& mux,
                         int imuChannel) {
    std::printf("\n\033[1m--- IMU 5 Hz Live-Test ---\033[0m\n");

    const uint8_t ch = static_cast<uint8_t>(imuChannel >= 0 ? imuChannel : kMuxChImu);
    mux.selectChannel(ch);
    sleepMs(50);

    auto logger = std::make_shared<sunray::StdoutLogger>(sunray::LogLevel::WARN);

    std::unique_ptr<sunray::Mpu6050Driver> imuPtr;
    uint8_t imuAddr = 0;
    for (uint8_t candidate : {kAddrImu69, kAddrImu68}) {
        mux.selectChannel(ch);
        auto drv = std::make_unique<sunray::Mpu6050Driver>(bus, logger, candidate);
        if (drv->init()) {
            imuAddr = candidate;
            imuPtr  = std::move(drv);
            break;
        }
    }

    if (!imuPtr) {
        std::printf("  [FAIL] MPU-6050 init fehlgeschlagen auf 0x69 und 0x68\n");
        return;
    }
    std::printf("  [PASS] MPU-6050 bereit (0x%02X, Kanal %d)\n\n", imuAddr, ch);

    std::printf("  %3s  %8s  %8s  %8s  %9s  %10s  %8s\n",
                "#", "accX[g]", "accY[g]", "accZ[g]", "|acc|[g]", "gyroZ[°/s]", "yaw[°]");
    std::printf("  %s\n", std::string(62, '-').c_str());

    constexpr int   kSamples = 10;
    constexpr float kDt      = 0.200f;

    for (int i = 0; i < kSamples; ++i) {
        const auto t0 = std::chrono::steady_clock::now();

        mux.selectChannel(ch);
        imuPtr->update(kDt);
        const sunray::ImuData d = imuPtr->getData();

        const float mag = std::sqrt(d.accX*d.accX + d.accY*d.accY + d.accZ*d.accZ);
        const float gz  = d.gyroZ * (180.0f / static_cast<float>(M_PI));
        const float yaw = d.yaw   * (180.0f / static_cast<float>(M_PI));

        std::printf("  %3d  %8.3f  %8.3f  %8.3f  %9.3f  %10.2f  %8.1f\n",
                    i + 1, d.accX, d.accY, d.accZ, mag, gz, yaw);

        const auto elapsed = std::chrono::steady_clock::now() - t0;
        const auto remain  = std::chrono::milliseconds(200) - elapsed;
        if (remain > std::chrono::milliseconds(0))
            std::this_thread::sleep_for(remain);
    }

    // Plausibilitätsprüfung
    mux.selectChannel(ch);
    imuPtr->update(kDt);
    const sunray::ImuData d = imuPtr->getData();
    const float mag = std::sqrt(d.accX*d.accX + d.accY*d.accY + d.accZ*d.accZ);

    std::printf("\n");
    if (mag > 0.5f && mag < 2.0f)
        std::printf("  [PASS] |acc| = %.3f g (plausibel)\n", mag);
    else if (mag == 0.0f)
        std::printf("  [FAIL] |acc| = 0.000 g — Sensor schläft noch oder falscher Kanal\n");
    else
        std::printf("  [WARN] |acc| = %.3f g (außerhalb 0.5–2.0 g)\n", mag);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::printf("\033[1m========================================\033[0m\n");
    std::printf("\033[1m  LED + IMU DIAGNOSE\033[0m\n");
    std::printf("\033[1m========================================\033[0m\n");

    try {
        sunray::platform::I2C           bus(kI2cBus);
        sunray::platform::I2cMux        mux(bus, kAddrMux);
        sunray::platform::PortExpander  ex1(bus, kAddrEx1);
        sunray::platform::PortExpander  ex3(bus, kAddrEx3);

        // IMU-Strom einschalten (EX1 IO1.6) damit MPU-6050 erreichbar ist
        mux.selectChannel(kMuxChLegacy);
        ex1.setPin(1, 6, true);
        sleepMs(300);  // Power-Up-Zeit

        // Phase 1: LEDs
        phaseLeds(mux, ex3);

        // Phase 2: IMU Mux-Scan
        const int imuChannel = phaseImuScan(bus, mux);

        // Phase 3: IMU Live
        phaseImuLive(bus, mux, imuChannel);

        std::printf("\n\033[1m========================================\033[0m\n");
        std::printf("\033[1m  FERTIG\033[0m\n");
        std::printf("\033[1m========================================\033[0m\n\n");

    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nFATAL: %s\n", e.what());
        return 1;
    }

    return 0;
}
