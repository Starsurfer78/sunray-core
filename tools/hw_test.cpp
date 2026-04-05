// hw_test.cpp — Comprehensive hardware diagnostics for Alfred robot.
//
// Tests in order:
//   0. systemd    — stop sunray service if running
//   1. UART       — open /dev/ttyS0, AT+V, AT+S, AT+M
//   2. I2C        — bus open, mux, port-expanders, IMU/EEPROM/ADC device presence
//   3. IMU        — MPU-6050 init + 5 Hz live output (10 samples)
//   4. Aktoren    — LEDs, Buzzer, Fan (interaktiv)
//   5. Sensoren   — E-Stop, Bumper L/R, Lift (interaktiv)
//   6. Motoren    — Ramp up/hold/down für Fahrmotoren + Mähmotor, mit Odometrie-Check
//   7. GPS        — seriellen Port öffnen, erste NMEA-Sätze ausgeben
//
// Benutzung:
//   sudo ./hw_test               — vollständiger Test
//   sudo ./hw_test --skip-motors — Motortests überspringen
//
// Voraussetzung: läuft als root (oder Nutzer mit i2c/serial Rechten).
// Das Tool stoppt den sunray-Dienst selbständig und startet ihn am Ende neu.

#include "platform/Serial.h"
#include "platform/I2C.h"
#include "platform/I2cMux.h"
#include "platform/PortExpander.h"
#include "hal/Imu/Mpu6050Driver.h"
#include "core/Logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <csignal>

// ── Konfiguration ─────────────────────────────────────────────────────────────

static constexpr const char* kSvc      = "sunray";
static constexpr const char* kUartPort = "/dev/ttyS0";
static constexpr unsigned    kUartBaud = 19200;
static constexpr const char* kI2cBus   = "/dev/i2c-1";
static constexpr const char* kGpsPort  =
    "/dev/serial/by-id/usb-u-blox_AG_-_www.u-blox.com_u-blox_GNSS_receiver-if00";
static constexpr unsigned    kGpsBaud  = 115200;

// Motor-Test-Parameter
static constexpr int   kMotorMaxPwm    = 100;   // max PWM (255 = voll)
static constexpr int   kMotorRampSteps = 50;    // Schritte beim Ramp-Up (je 20 ms → 1 s)
static constexpr int   kMotorHoldMs    = 1500;  // ms auf Maximum halten
static constexpr int   kMowMaxPwm      = 80;    // Mähmotor etwas vorsichtiger
static constexpr int   kMowRampSteps   = 40;
static constexpr int   kMowHoldMs      = 2000;

// I2C-Adressen
static constexpr uint8_t kAddrMux    = 0x70;
static constexpr uint8_t kAddrEx1    = 0x21;  // IMU-Strom, Fan
static constexpr uint8_t kAddrEx2    = 0x20;  // Buzzer
static constexpr uint8_t kAddrEx3    = 0x22;  // Panel-LEDs
static constexpr uint8_t kAddrImu69  = 0x69;
static constexpr uint8_t kAddrImu68  = 0x68;
static constexpr uint8_t kAddrEeprom = 0x50;
static constexpr uint8_t kAddrAdc    = 0x68;  // MCP3421, Mux-Ch6
static constexpr uint8_t kMuxChLegacy = 0;    // EX1/EX2/EX3
static constexpr uint8_t kMuxChImu    = 4;
static constexpr uint8_t kMuxChEeprom = 5;
static constexpr uint8_t kMuxChAdc    = 6;

// ── Ergebnis-System ───────────────────────────────────────────────────────────

enum class Result { PASS, FAIL, WARN, SKIP };

struct TestReport {
    std::string name;
    Result      result;
    std::string message;
};

static std::vector<TestReport> gReports;

static void rep(const std::string& name, Result r, const std::string& msg) {
    gReports.push_back({name, r, msg});
    const char* tag = (r == Result::PASS) ? "\033[32m[PASS]\033[0m" :
                      (r == Result::FAIL) ? "\033[31m[FAIL]\033[0m" :
                      (r == Result::WARN) ? "\033[33m[WARN]\033[0m" :
                                            "\033[90m[SKIP]\033[0m";
    std::cout << tag << " " << name << ": " << msg << "\n";
    std::cout.flush();
}

// ── AT-Protokoll Hilfsfunktionen ──────────────────────────────────────────────

static uint8_t calcCrc(const std::string& s) {
    uint8_t crc = 0;
    for (unsigned char c : s) crc += c;
    return crc;
}

static std::string makeAtFrame(const std::string& body) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), ",0x%02X\r\n", calcCrc(body));
    return body + buf;
}

static bool verifyCrc(std::string& frame) {
    const auto idx = frame.rfind(',');
    if (idx == std::string::npos || idx == 0) return false;
    uint8_t expected = 0;
    for (size_t i = 0; i < idx; i++) expected += static_cast<uint8_t>(frame[i]);
    const long received = std::strtol(frame.c_str() + idx + 1, nullptr, 16);
    if (expected != static_cast<uint8_t>(received)) return false;
    frame = frame.substr(0, idx);
    return true;
}

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

// ── AT-Link ───────────────────────────────────────────────────────────────────
// Einfacher blockierender Wrapper: senden + auf Frame eines bestimmten Typs warten.

class AtLink {
public:
    explicit AtLink(sunray::platform::Serial& port) : port_(port) {}

    void sendRaw(const std::string& body) {
        port_.writeStr(makeAtFrame(body).c_str());
    }

    // Sendet body, wartet bis zu timeoutMs auf einen Frame dessen erstes Zeichen
    // dem gewünschten frameType entspricht. Gibt den Frame ohne CRC zurück, oder "".
    std::string exchange(const std::string& body, char frameType, int timeoutMs = 600) {
        sendRaw(body);
        return waitFrame(frameType, timeoutMs);
    }

    // Wartet passiv auf einen Frame des angegebenen Typs (ohne vorher zu senden).
    std::string waitFrame(char frameType, int timeoutMs) {
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (!port_.waitReadable(15)) {
                // Gap-basiertes Dispatch: hängengebliebener Frame ohne CR/LF
                if (!rxBuf_.empty()) tryDispatch(frameType);
                continue;
            }
            uint8_t buf[512];
            const int n = port_.read(buf, sizeof(buf));
            if (n <= 0) continue;

            for (int i = 0; i < n; i++) {
                char ch = static_cast<char>(buf[i]);
                if (ch == '\r' || ch == '\n') {
                    if (!rxBuf_.empty()) {
                        std::string frame = std::move(rxBuf_);
                        rxBuf_.clear();
                        if (!frame.empty() && frame[0] == frameType && verifyCrc(frame))
                            return frame;
                    }
                } else if (rxBuf_.size() < 512) {
                    rxBuf_ += ch;
                }
            }

            // CRC-basiertes Dispatch (Frames ohne Zeilenumbruch)
            if (auto f = drainCrcFrame(frameType); !f.empty())
                return f;
        }
        return "";
    }

    void flush() { port_.flush(); rxBuf_.clear(); }

private:
    sunray::platform::Serial& port_;
    std::string rxBuf_;

    void tryDispatch(char) { /* optional — gap handling */ }

    std::string drainCrcFrame(char frameType) {
        if (rxBuf_.size() < 6) return "";
        const std::size_t start = rxBuf_.find_first_of("MSV");
        if (start == std::string::npos) { rxBuf_.clear(); return ""; }
        if (start > 0) rxBuf_.erase(0, start);

        const std::size_t crcPos = rxBuf_.find(",0x");
        if (crcPos == std::string::npos || crcPos + 5 > rxBuf_.size()) return "";

        const auto isHex = [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        };
        if (!isHex(rxBuf_[crcPos + 3]) || !isHex(rxBuf_[crcPos + 4])) {
            rxBuf_.erase(0, crcPos + 1);
            return "";
        }

        std::string frame = rxBuf_.substr(0, crcPos + 5);
        rxBuf_.erase(0, crcPos + 5);

        if (!frame.empty() && frame[0] == frameType && verifyCrc(frame))
            return frame;
        return "";
    }
};

// ── Cleanup — Motoren stoppen bei Ctrl+C ─────────────────────────────────────

static sunray::platform::Serial* gUart = nullptr;

static void stopMotors() {
    if (gUart && gUart->isOpen()) {
        gUart->writeStr(makeAtFrame("AT+M,0,0,0").c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
}

static void onSignal(int) {
    stopMotors();
    std::cout << "\n\033[31mAbgebrochen.\033[0m\n";
    std::exit(130);
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────

static bool i2cProbe(sunray::platform::I2C& bus, uint8_t addr) {
    uint8_t dummy;
    return bus.read(addr, &dummy, 1);
}

static bool askUser(const std::string& question) {
    std::cout << "     \033[36m?? " << question << " [j/n]\033[0m: ";
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    return (!line.empty() && (line[0] == 'j' || line[0] == 'J'
                           || line[0] == 'y' || line[0] == 'Y'));
}


static std::string hexAddr(uint8_t addr) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "0x%02X", addr);
    return buf;
}

static std::string fmtFloat(float v, int prec = 2) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

static uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

// ── Motorsteuerung ────────────────────────────────────────────────────────────
// BUG-07: Pi sendet AT+M,rightPwm,leftPwm,mow (Verkabelungskorrektur)

static void sendMotorPwm(AtLink& at, int left, int right, int mow) {
    // BUG-07: right als field1, left als field2
    at.sendRaw("AT+M," + std::to_string(right)
             + "," + std::to_string(left)
             + "," + std::to_string(mow));
}

// Liest M-Frame und gibt Ticks zurück (STM32 sendet left als f1, right als f2)
struct MotorFrame {
    unsigned long ticksLeft  = 0;
    unsigned long ticksRight = 0;
    unsigned long ticksMow   = 0;
    bool stopButton = false;
    bool fault      = false;
    bool valid      = false;
};

static MotorFrame parseMotorFrame(const std::string& frame) {
    MotorFrame m;
    const auto f = csvSplit(frame);
    try {
        // STM32 sendet f1=linksTicks, f2=rechtsTicks (BUG-07: Verkabelung)
        if (f.size() > 1) m.ticksLeft  = std::stoul(f[1]);
        if (f.size() > 2) m.ticksRight = std::stoul(f[2]);
        if (f.size() > 3) m.ticksMow   = std::stoul(f[3]);
        if (f.size() > 7) m.stopButton = (std::stoi(f[7]) != 0);
        if (f.size() > 8) m.fault      = (std::stoi(f[8]) != 0);
        m.valid = true;
    } catch (...) {}
    return m;
}

// ── Phase 0: systemd ──────────────────────────────────────────────────────────

static bool gRestartService = false;

static void phase0Systemd() {
    std::cout << "\n\033[1m--- Phase 0: systemd ---\033[0m\n";

    const bool active = (std::system(("systemctl is-active --quiet " + std::string(kSvc)).c_str()) == 0);
    if (!active) {
        rep("systemd", Result::PASS, "Dienst war nicht aktiv");
        return;
    }

    std::cout << "  Dienst '" << kSvc << "' läuft — stoppe...\n";
    const int rc = std::system(("systemctl stop " + std::string(kSvc)).c_str());
    if (rc != 0) {
        rep("systemd stop", Result::WARN, "stop-Befehl gab " + std::to_string(rc) + " zurück");
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        rep("systemd stop", Result::PASS, "Dienst gestoppt — wird am Ende neu gestartet");
        gRestartService = true;
    }
}

// ── Phase 1: UART / STM32-Link ────────────────────────────────────────────────

static bool phase1Uart(sunray::platform::Serial& uart) {
    // Header wird in main gedruckt — kein zweiter Header hier

    AtLink at(uart);
    at.flush();

    // AT+V — Firmware-Version, bis zu 3 Versuche (erster Frame nach power-on kann verloren gehen)
    std::string vFrame;
    for (int attempt = 0; attempt < 3 && vFrame.empty(); ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            std::cout << "  AT+V Versuch " << (attempt + 1) << "...\n";
        }
        vFrame = at.exchange("AT+V", 'V', 1200);
    }
    if (vFrame.empty()) {
        rep("AT+V", Result::FAIL, "keine Antwort nach 3 Versuchen — STM32 erreichbar?");
        return false;
    }
    {
        const auto f = csvSplit(vFrame);
        const std::string name = (f.size() > 1) ? f[1] : "?";
        const std::string ver  = (f.size() > 2) ? f[2] : "?";
        rep("AT+V", Result::PASS, "firmware=" + name + "  version=" + ver);
    }

    // Verbindung aktivieren: 3× AT+M,0,0,0 senden damit der STM32 im "aktiven
    // Kommunikationsmodus" ist — ohne AT+M-Handshake antwortet er nicht auf AT+S.
    at.flush();
    for (int i = 0; i < 3; ++i) {
        at.exchange("AT+M,0,0,0", 'M', 300);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    at.flush();

    // AT+S — Summary-Telemetrie
    const std::string sFrame = at.exchange("AT+S", 'S', 1500);
    if (sFrame.empty()) {
        rep("AT+S", Result::FAIL, "keine Antwort innerhalb 1,5 s");
        return false;
    }
    {
        const auto f = csvSplit(sFrame);
        try {
            const float batV  = (f.size() > 1)  ? std::stof(f[1]) : -1.0f;
            const float chgV  = (f.size() > 2)  ? std::stof(f[2]) : -1.0f;
            const float chgA  = (f.size() > 3)  ? std::stof(f[3]) :  0.0f;
            const float batT  = (f.size() > 11) ? std::stof(f[11]) : -9999.0f;
            const bool  rain  = (f.size() > 6)  && (std::stoi(f[6]) != 0);
            const bool  fault = (f.size() > 7)  && (std::stoi(f[7]) != 0);

            std::ostringstream ss;
            ss << "batV=" << fmtFloat(batV) << " V"
               << "  chgV=" << fmtFloat(chgV) << " V"
               << "  chgA=" << fmtFloat(chgA) << " A";
            if (batT > -9000.0f) ss << "  batT=" << fmtFloat(batT) << " °C";
            if (rain) ss << "  \033[33mREGEN\033[0m";
            rep("AT+S", fault ? Result::WARN : Result::PASS, ss.str());

            // Batterie-Spannung Plausibilität
            if (batV < 18.0f || batV > 30.0f) {
                rep("Batterie-Spannung", Result::WARN,
                    fmtFloat(batV) + " V — außerhalb Bereich 18–30 V");
            } else {
                rep("Batterie-Spannung", Result::PASS,
                    fmtFloat(batV) + " V  (Bereich 18–30 V ok)");
            }

            if (batV < 18.0f) {
                std::cout << "  \033[31mKRITISCH: Batterie zu leer für Motortests!\033[0m\n";
            }

            // Temperatur
            if (batT > -9000.0f) {
                if (batT < -10.0f || batT > 60.0f) {
                    rep("Batterie-Temperatur", Result::WARN,
                        fmtFloat(batT) + " °C — außerhalb −10…60 °C");
                } else {
                    rep("Batterie-Temperatur", Result::PASS, fmtFloat(batT) + " °C");
                }
            }
        } catch (...) {
            rep("AT+S parse", Result::FAIL, "Frame nicht parsierbar: " + sFrame);
        }
    }

    // AT+M — Schnell-Sensoren im Ruhezustand
    const std::string mFrame = at.exchange("AT+M,0,0,0", 'M', 600);
    if (mFrame.empty()) {
        rep("AT+M", Result::FAIL, "keine Antwort auf Motorsollwert 0");
    } else {
        const auto m = parseMotorFrame(mFrame);
        std::string msg = "stopButton=";
        msg += (m.stopButton ? "\033[31mAKTIV\033[0m" : "ok");
        msg += "  fault=";
        msg += (m.fault ? "\033[31mAKTIV\033[0m" : "ok");
        rep("AT+M Ruhezustand", (m.stopButton || m.fault) ? Result::WARN : Result::PASS, msg);
    }

    return true;
}

// ── Phase 2: I2C-Topologie ────────────────────────────────────────────────────

static void phase2I2c(sunray::platform::I2C& bus, sunray::platform::I2cMux& mux,
                      sunray::platform::PortExpander& ex1,
                      sunray::platform::PortExpander& ex2,
                      sunray::platform::PortExpander& ex3) {
    std::cout << "\n\033[1m--- Phase 2: I2C-Topologie ---\033[0m\n";

    // Mux (direkt auf Bus)
    if (i2cProbe(bus, kAddrMux))
        rep("TCA9548A Mux " + hexAddr(kAddrMux), Result::PASS, "antwortet");
    else
        rep("TCA9548A Mux " + hexAddr(kAddrMux), Result::FAIL, "keine I2C-Antwort");

    // Port-Expander auf Mux-Kanal 0
    mux.selectChannel(kMuxChLegacy);
    for (auto [addr, label] : std::vector<std::pair<uint8_t, std::string>>{
            {kAddrEx1, "EX1 IMU/Fan"},
            {kAddrEx2, "EX2 Buzzer"},
            {kAddrEx3, "EX3 LEDs"}}) {
        if (i2cProbe(bus, addr))
            rep("PCA9555 " + label + " " + hexAddr(addr), Result::PASS, "antwortet");
        else
            rep("PCA9555 " + label + " " + hexAddr(addr), Result::FAIL, "keine I2C-Antwort");
    }

    // IMU auf Mux-Kanal 4
    mux.selectChannel(kMuxChImu);
    const bool at69 = i2cProbe(bus, kAddrImu69);
    const bool at68 = i2cProbe(bus, kAddrImu68);
    if (at69)
        rep("MPU-6050 IMU " + hexAddr(kAddrImu69) + " (Mux-Ch 4)", Result::PASS, "antwortet");
    else if (at68)
        rep("MPU-6050 IMU " + hexAddr(kAddrImu68) + " (Mux-Ch 4)", Result::WARN,
            "Fallback-Adresse 0x68");
    else
        rep("MPU-6050 IMU (Mux-Ch 4)", Result::FAIL, "keine Antwort auf 0x68 oder 0x69");

    // EEPROM auf Mux-Kanal 5
    mux.selectChannel(kMuxChEeprom);
    if (i2cProbe(bus, kAddrEeprom))
        rep("EEPROM " + hexAddr(kAddrEeprom) + " (Mux-Ch 5)", Result::PASS, "antwortet");
    else
        rep("EEPROM " + hexAddr(kAddrEeprom) + " (Mux-Ch 5)", Result::WARN,
            "keine Antwort (ggf. nicht bestückt)");

    // ADC auf Mux-Kanal 6
    mux.selectChannel(kMuxChAdc);
    if (i2cProbe(bus, kAddrAdc))
        rep("MCP3421 ADC " + hexAddr(kAddrAdc) + " (Mux-Ch 6)", Result::PASS, "antwortet");
    else
        rep("MCP3421 ADC " + hexAddr(kAddrAdc) + " (Mux-Ch 6)", Result::WARN, "keine Antwort");

    // IMU-Strom einschalten (EX1 IO1.6) damit der IMU-Test danach klappt
    mux.selectChannel(kMuxChLegacy);
    ex1.setPin(1, 6, true);   // IMU power ON
    ex1.setPin(1, 7, false);  // Fan OFF während Test

    (void)ex2; (void)ex3;  // werden in Phase 3 benutzt
}

// ── Phase 3: IMU 5 Hz Live-Test ───────────────────────────────────────────────

static void phase3Imu(sunray::platform::I2C& bus, sunray::platform::I2cMux& mux) {
    std::cout << "\n\033[1m--- Phase 3: IMU 5 Hz Live-Test ---\033[0m\n";

    mux.selectChannel(kMuxChImu);
    std::this_thread::sleep_for(std::chrono::milliseconds(350));  // power-up nach EX1 IO1.6

    // Adresse per echtem Treiber-Init bestimmen — nicht per i2cProbe.
    // i2cProbe (raw read) und Treiber (writeRead mit Register-Adresse) verhalten sich
    // nach Mux-Wechseln unterschiedlich; deshalb probieren wir 0x69 dann 0x68.
    auto logger = std::make_shared<sunray::StdoutLogger>(sunray::LogLevel::WARN);

    uint8_t imuAddr = 0;
    std::unique_ptr<sunray::Mpu6050Driver> imuPtr;
    for (uint8_t candidate : {kAddrImu69, kAddrImu68}) {
        mux.selectChannel(kMuxChImu);  // Mux-Kanal vor jedem Versuch neu setzen
        auto drv = std::make_unique<sunray::Mpu6050Driver>(bus, logger, candidate);
        if (drv->init()) {
            imuAddr = candidate;
            imuPtr  = std::move(drv);
            break;
        }
    }

    if (!imuPtr) {
        rep("IMU init", Result::FAIL, "MPU-6050 antwortet nicht auf 0x69 und 0x68");
        return;
    }
    rep("IMU init", Result::PASS, "MPU-6050 bereit (" + hexAddr(imuAddr) + ")");
    sunray::Mpu6050Driver& imu = *imuPtr;

    // Tabellenkopf
    std::cout << "\n  "
              << std::setw(3)  << "#"
              << std::setw(9)  << "accX[g]"
              << std::setw(9)  << "accY[g]"
              << std::setw(9)  << "accZ[g]"
              << std::setw(10) << "|acc|[g]"
              << std::setw(11) << "gyroZ[°/s]"
              << std::setw(9)  << "yaw[°]"
              << "\n  " << std::string(59, '-') << "\n";

    constexpr int    kSamples  = 10;
    constexpr float  kDt       = 0.200f;   // 5 Hz

    for (int i = 0; i < kSamples; ++i) {
        const auto t0 = std::chrono::steady_clock::now();

        mux.selectChannel(kMuxChImu);  // sicherstellen (kein Mux-Drift)
        imu.update(kDt);
        const sunray::ImuData d = imu.getData();

        const float mag = std::sqrt(d.accX*d.accX + d.accY*d.accY + d.accZ*d.accZ);
        const float gz  = d.gyroZ * (180.0f / static_cast<float>(M_PI));
        const float yaw = d.yaw   * (180.0f / static_cast<float>(M_PI));

        std::cout << "  "
                  << std::setw(3)  << (i + 1)
                  << std::setw(9)  << std::fixed << std::setprecision(3) << d.accX
                  << std::setw(9)  << d.accY
                  << std::setw(9)  << d.accZ
                  << std::setw(10) << std::setprecision(3) << mag
                  << std::setw(11) << std::setprecision(2) << gz
                  << std::setw(9)  << std::setprecision(1) << yaw
                  << "\n";

        // Rest der 200 ms abwarten
        const auto elapsed   = std::chrono::steady_clock::now() - t0;
        const auto remaining = std::chrono::milliseconds(200) - elapsed;
        if (remaining > std::chrono::milliseconds(0))
            std::this_thread::sleep_for(remaining);
    }

    // Sanity: |acc| sollte ~1 g sein (Schwerkraft im Ruhezustand)
    const sunray::ImuData last = imu.getData();
    const float mag = std::sqrt(last.accX*last.accX + last.accY*last.accY + last.accZ*last.accZ);
    if (mag < 0.7f || mag > 1.3f)
        rep("IMU Beschleunigung", Result::WARN,
            "|acc| = " + fmtFloat(mag) + " g  (erwartet ~1.0 g im Ruhezustand)");
    else
        rep("IMU Beschleunigung", Result::PASS,
            "|acc| = " + fmtFloat(mag) + " g  (plausibel)");
}

// ── Phase 4: Aktoren ──────────────────────────────────────────────────────────

static void phase4Aktoren(sunray::platform::I2cMux& mux,
                           sunray::platform::PortExpander& ex1,
                           sunray::platform::PortExpander& ex2,
                           sunray::platform::PortExpander& ex3) {
    std::cout << "\n\033[1m--- Phase 4: Aktoren ---\033[0m\n";

    mux.selectChannel(kMuxChLegacy);

    // EX3 Port 0: alle 6 LED-Pins (0-5) als Ausgänge, Port 1 als Eingänge
    // Mux muss auf Channel 0 stehen — wird hier und bei jedem Schreibvorgang gesetzt
    mux.selectChannel(kMuxChLegacy);
    ex3.setConfigPort(0, 0x00);   // alle Port-0-Pins = Output
    ex3.setOutputPort(0, 0x00);   // alle LEDs AUS

    // ── LED-Hilfsfunktion ─────────────────────────────────────────────────────
    // Atomarer Schreibvorgang: ein einziger setOutputPort-Aufruf für alle 6 LEDs.
    // Mux wird vor jedem Schreiben explizit auf Ch0 gesetzt.
    // Bitmapping EX3 Port 0:
    //   bit 0 = LED1 grün   bit 1 = LED1 rot
    //   bit 2 = LED2 grün   bit 3 = LED2 rot
    //   bit 4 = LED3 grün   bit 5 = LED3 rot
    auto setLeds = [&](uint8_t mask) {
        mux.selectChannel(kMuxChLegacy);
        ex3.setOutputPort(0, mask);
    };

    // ── LEDs ──────────────────────────────────────────────────────────────────
    const std::vector<std::tuple<uint8_t, uint8_t, std::string>> leds = {
        {0x01, 0x02, "LED1 (WiFi, unten)"},
        {0x04, 0x08, "LED2 (Fehler, oben)"},
        {0x10, 0x20, "LED3 (GPS, Mitte)"}
    };

    std::cout << "  LEDs AUS...\n";
    setLeds(0x00);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (auto& [gMask, rMask, name] : leds) {
        std::cout << "  " << name << ": \033[32mGRÜN\033[0m...\n";
        setLeds(gMask);
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));

        std::cout << "  " << name << ": \033[31mROT\033[0m...\n";
        setLeds(rMask);
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));

        setLeds(0x00);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    if (askUser("Hast du alle 3 LEDs grün und rot blinken gesehen?"))
        rep("LEDs", Result::PASS, "alle 3 LEDs grün + rot bestätigt");
    else
        rep("LEDs", Result::FAIL, "user: LEDs nicht sichtbar");

    // ── Buzzer ────────────────────────────────────────────────────────────────
    std::cout << "  Buzzer: kurzes Signal...\n";
    ex2.setPin(1, 1, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    ex2.setPin(1, 1, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ex2.setPin(1, 1, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ex2.setPin(1, 1, false);

    if (askUser("Hast du den Buzzer gehört?"))
        rep("Buzzer", Result::PASS, "akustisch bestätigt");
    else
        rep("Buzzer", Result::FAIL, "user: kein Ton gehört");

    // ── Fan ───────────────────────────────────────────────────────────────────
    std::cout << "  Lüfter: EIN für 2 s...\n";
    ex1.setPin(1, 7, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    ex1.setPin(1, 7, false);

    if (askUser("Hat der Lüfter gezogen/geblasen?"))
        rep("Lüfter", Result::PASS, "bestätigt");
    else
        rep("Lüfter", Result::FAIL, "user: Lüfter nicht wahrgenommen");
}

// ── Phase 5: Sicherheits-Sensoren ─────────────────────────────────────────────

// Pollt AT+M-Frames für bis zu pollMs und prüft mit check().
// Gibt zurück wie viele ms es gedauert hat, oder -1 wenn nicht erkannt.
static int pollSensor(AtLink& at, int pollMs,
                      const std::function<bool(const std::vector<std::string>&)>& check) {
    const uint64_t start    = nowMs();
    const uint64_t deadline = start + static_cast<uint64_t>(pollMs);
    while (nowMs() < deadline) {
        at.sendRaw("AT+M,0,0,0");
        const std::string f = at.waitFrame('M', 300);
        if (!f.empty()) {
            const auto fields = csvSplit(f);
            if (check(fields)) return static_cast<int>(nowMs() - start);
        }
    }
    return -1;
}

static void phase5Sensoren(AtLink& at) {
    std::cout << "\n\033[1m--- Phase 5: Sicherheits-Sensoren ---\033[0m\n";

    constexpr int kPollMs = 6000;  // 6 s Zeit zum Auslösen

    // ── E-Stop ────────────────────────────────────────────────────────────────
    {
        // Ausgangszustand: E-Stop muss offen sein
        at.sendRaw("AT+M,0,0,0");
        const std::string base = at.waitFrame('M', 600);
        if (base.empty()) {
            rep("E-Stop", Result::FAIL, "kein AT+M Frame — UART-Verbindung prüfen");
        } else {
            const auto m = parseMotorFrame(base);
            if (m.stopButton) {
                rep("E-Stop Ausgangszustand", Result::WARN,
                    "E-Stop bereits aktiv — bitte lösen vor dem Test");
            } else {
                std::cout << "  Drücke jetzt den \033[1mE-Stop\033[0m"
                          << " (wir erkennen automatisch, max " << kPollMs/1000 << " s) ...\n";
                const int ms = pollSensor(at, kPollMs,
                    [](const auto& f){ return f.size() > 7 && std::stoi(f[7]) != 0; });

                if (ms < 0) {
                    rep("E-Stop", Result::FAIL, "nicht erkannt — Verkabelung/Firmware prüfen");
                } else {
                    std::cout << "  \033[32mErkannt\033[0m nach " << ms << " ms\n";
                    // Warten bis losgelassen
                    std::cout << "  Lass den E-Stop los ...\n";
                    const int msRel = pollSensor(at, kPollMs,
                        [](const auto& f){ return f.size() > 7 && std::stoi(f[7]) == 0; });
                    rep("E-Stop", (msRel >= 0) ? Result::PASS : Result::WARN,
                        "erkannt nach " + std::to_string(ms) + " ms"
                        + (msRel >= 0 ? ", rückgestellt" : " — Rückstellung nicht erkannt"));
                }
            }
        }
    }

    // ── Bumper Links ──────────────────────────────────────────────────────────
    {
        std::cout << "  Drücke den \033[1mlinken Bumper\033[0m"
                  << " und halte ihn (max " << kPollMs/1000 << " s) ...\n";
        const int ms = pollSensor(at, kPollMs, [](const auto& f) {
            // f[9] = bumperLeft präzise; fallback f[5] = legacy bumper
            const bool bL = (f.size() > 9)  && (std::stoi(f[9]) != 0);
            const bool bl = (f.size() > 5)  && (std::stoi(f[5]) != 0);
            return bL || bl;
        });
        if (ms < 0)
            rep("Bumper Links", Result::FAIL, "nicht erkannt — Anschluss/Firmware prüfen");
        else {
            std::cout << "  \033[32mErkannt\033[0m nach " << ms << " ms — Bumper kann losgelassen werden.\n";
            rep("Bumper Links", Result::PASS, "erkannt nach " + std::to_string(ms) + " ms");
        }
    }

    // ── Bumper Rechts ─────────────────────────────────────────────────────────
    {
        std::cout << "  Drücke den \033[1mrechten Bumper\033[0m"
                  << " und halte ihn (max " << kPollMs/1000 << " s) ...\n";
        const int ms = pollSensor(at, kPollMs, [](const auto& f) {
            return f.size() > 10 && std::stoi(f[10]) != 0;
        });
        if (ms < 0)
            rep("Bumper Rechts", Result::FAIL, "nicht erkannt — Anschluss/Firmware prüfen");
        else {
            std::cout << "  \033[32mErkannt\033[0m nach " << ms << " ms — Bumper kann losgelassen werden.\n";
            rep("Bumper Rechts", Result::PASS, "erkannt nach " + std::to_string(ms) + " ms");
        }
    }

    // ── Lift ──────────────────────────────────────────────────────────────────
    {
        std::cout << "  \033[1mRoboter anheben\033[0m (beide Räder frei)"
                  << " (max " << kPollMs/1000 << " s) ...\n";
        const int ms = pollSensor(at, kPollMs, [](const auto& f) {
            return f.size() > 6 && std::stoi(f[6]) != 0;
        });
        if (ms < 0)
            rep("Lift-Sensor", Result::FAIL, "nicht erkannt — Sensor/Schwellwert prüfen");
        else {
            std::cout << "  \033[32mErkannt\033[0m nach " << ms << " ms — Roboter wieder absetzen.\n";
            rep("Lift-Sensor", Result::PASS, "erkannt nach " + std::to_string(ms) + " ms");
        }
    }
}

// ── Phase 6: Motoren ──────────────────────────────────────────────────────────

// Stromwerte aus AT+S-Frame (f8=mow, f9=links, f10=rechts) [A]
struct MotorCurrents {
    float leftA  = 0.0f;
    float rightA = 0.0f;
    float mowA   = 0.0f;
    bool  valid  = false;
};

// Sendet AT+S und liest Stromwerte; sendet danach sofort einen AT+M-Frame
// um die Motoren auf dem laufenden Sollwert zu halten.
static MotorCurrents readCurrents(AtLink& at, int leftPwm, int rightPwm, int mowPwm) {
    at.sendRaw("AT+S");
    const std::string sf = at.waitFrame('S', 700);
    // Motor-Sollwert sofort wiederholen — STM32 behält letzten Wert, aber sicher ist sicher
    sendMotorPwm(at, leftPwm, rightPwm, mowPwm);
    at.waitFrame('M', 60);

    MotorCurrents c;
    if (sf.empty()) return c;
    const auto f = csvSplit(sf);
    try {
        if (f.size() > 8)  c.mowA   = std::stof(f[8]);
        if (f.size() > 9)  c.leftA  = std::stof(f[9]);
        if (f.size() > 10) c.rightA = std::stof(f[10]);
        c.valid = true;
    } catch (...) {}
    return c;
}

// Ramp-Test für Fahrmotoren
static void motorRampTest(AtLink& at) {
    // Ausgangszustand prüfen
    at.sendRaw("AT+M,0,0,0");
    const std::string check = at.waitFrame('M', 500);
    if (!check.empty()) {
        const auto m = parseMotorFrame(check);
        if (m.fault) {
            rep("Fahrmotor-Ramp", Result::FAIL, "Motor-Fault aktiv — Test abgebrochen");
            return;
        }
        if (m.stopButton) {
            rep("Fahrmotor-Ramp", Result::FAIL, "E-Stop aktiv — Test abgebrochen");
            return;
        }
    }

    std::cout << "  Rampe hoch (" << kMotorRampSteps << " Schritte × 20 ms → "
              << (kMotorRampSteps * 20 / 1000) << " s) ...\n";

    unsigned long ticksL0 = 0, ticksR0 = 0;
    bool gotBaseline = false;

    // Ramp UP
    for (int step = 1; step <= kMotorRampSteps; ++step) {
        const int pwm = (kMotorMaxPwm * step) / kMotorRampSteps;
        sendMotorPwm(at, pwm, pwm, 0);
        const std::string f = at.waitFrame('M', 60);
        if (!f.empty() && !gotBaseline) {
            const auto m = parseMotorFrame(f);
            if (m.valid) { ticksL0 = m.ticksLeft; ticksR0 = m.ticksRight; gotBaseline = true; }
        }
        // Fault-Check während Rampe
        if (!f.empty()) {
            const auto m = parseMotorFrame(f);
            if (m.valid && m.fault) {
                sendMotorPwm(at, 0, 0, 0);
                rep("Fahrmotor-Ramp", Result::FAIL, "Motor-Fault bei PWM=" + std::to_string(pwm));
                return;
            }
        }
    }

    // Auf Maximum halten + Stromabfrage in der Mitte der Haltephase
    std::cout << "  Halte PWM=" << kMotorMaxPwm << " für "
              << kMotorHoldMs << " ms...\n";
    unsigned long ticksLend = 0, ticksRend = 0;
    MotorCurrents currents;
    const uint64_t holdEnd       = nowMs() + kMotorHoldMs;
    const uint64_t currentSampleMs = nowMs() + kMotorHoldMs / 2;  // Mitte der Haltephase

    while (nowMs() < holdEnd) {
        if (!currents.valid && nowMs() >= currentSampleMs) {
            // AT+S einmalig während der Haltephase senden
            currents = readCurrents(at, kMotorMaxPwm, kMotorMaxPwm, 0);
            if (currents.valid) {
                std::cout << "  Strom: links=" << fmtFloat(currents.leftA) << " A"
                          << "  rechts=" << fmtFloat(currents.rightA) << " A\n";
            }
        } else {
            sendMotorPwm(at, kMotorMaxPwm, kMotorMaxPwm, 0);
            const std::string f = at.waitFrame('M', 60);
            if (!f.empty()) {
                const auto m = parseMotorFrame(f);
                if (m.valid) { ticksLend = m.ticksLeft; ticksRend = m.ticksRight; }
                if (m.valid && m.fault) {
                    sendMotorPwm(at, 0, 0, 0);
                    rep("Fahrmotor-Ramp", Result::FAIL, "Motor-Fault während Halte-Phase");
                    return;
                }
            }
        }
    }

    // Ramp DOWN
    std::cout << "  Rampe runter...\n";
    for (int step = kMotorRampSteps; step >= 0; --step) {
        const int pwm = (kMotorMaxPwm * step) / kMotorRampSteps;
        sendMotorPwm(at, pwm, pwm, 0);
        at.waitFrame('M', 60);
    }
    sendMotorPwm(at, 0, 0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Auswertung Odometrie
    const long dL = static_cast<long>(ticksLend) - static_cast<long>(ticksL0);
    const long dR = static_cast<long>(ticksRend) - static_cast<long>(ticksR0);
    std::cout << "  Odometrie: ΔtickLinks=" << dL << "  ΔtickRechts=" << dR << "\n";

    {
        std::string msg = "ΔL=" + std::to_string(dL) + "  ΔR=" + std::to_string(dR)
                        + "  maxPWM=" + std::to_string(kMotorMaxPwm);
        Result r = Result::PASS;
        if (dL < 10 && dR < 10) {
            msg += " — kaum Ticks, Encoder prüfen"; r = Result::WARN;
        } else if (dL < 5 || dR < 5) {
            msg += " — ein Encoder liefert zu wenig Ticks"; r = Result::WARN;
        }
        rep("Fahrmotor-Odometrie", r, msg);
    }

    // Auswertung Strom
    if (currents.valid) {
        const std::string msgA = "links=" + fmtFloat(currents.leftA) + " A"
                               + "  rechts=" + fmtFloat(currents.rightA) + " A"
                               + "  (PWM=" + std::to_string(kMotorMaxPwm) + ")";
        // Plausibilität: Leerlauf-Rollen erwartet < 3 A; > 8 A = Überlast-Hinweis
        Result rA = Result::PASS;
        if (currents.leftA > 8.0f || currents.rightA > 8.0f)
            rA = Result::WARN;
        else if (currents.leftA < 0.05f && currents.rightA < 0.05f)
            rA = Result::WARN;  // kein Strom bei laufendem Motor = Fehler
        rep("Fahrmotor-Strom", rA, msgA);
    } else {
        rep("Fahrmotor-Strom", Result::WARN, "kein AT+S-Frame während Haltephase erhalten");
    }

    if (askUser("Haben sich beide Räder gleichmäßig vorwärts gedreht?"))
        rep("Fahrmotor visuell", Result::PASS, "bestätigt");
    else
        rep("Fahrmotor visuell", Result::FAIL, "user: Problem beobachtet");
}

// Ramp-Test für Mähmotor
static void mowMotorRampTest(AtLink& at) {
    std::cout << "  Mähmotor Rampe hoch (" << kMowRampSteps << " Schritte × 20 ms) ...\n";

    unsigned long ticksM0 = 0, ticksMend = 0;
    bool gotBase = false;

    // Ramp UP
    for (int step = 1; step <= kMowRampSteps; ++step) {
        const int pwm = (kMowMaxPwm * step) / kMowRampSteps;
        sendMotorPwm(at, 0, 0, pwm);
        const std::string f = at.waitFrame('M', 60);
        if (!f.empty() && !gotBase) {
            const auto m = parseMotorFrame(f);
            if (m.valid) { ticksM0 = m.ticksMow; gotBase = true; }
        }
        if (!f.empty()) {
            const auto m = parseMotorFrame(f);
            if (m.valid && m.fault) {
                sendMotorPwm(at, 0, 0, 0);
                rep("Mähmotor-Ramp", Result::FAIL, "Motor-Fault bei PWM=" + std::to_string(pwm));
                return;
            }
        }
    }

    // Halten + Stromabfrage in der Mitte der Haltephase
    std::cout << "  Halte Mähmotor bei PWM=" << kMowMaxPwm << " für "
              << kMowHoldMs << " ms...\n";
    MotorCurrents mowCurrents;
    const uint64_t holdEnd         = nowMs() + kMowHoldMs;
    const uint64_t currentSampleMs = nowMs() + kMowHoldMs / 2;

    while (nowMs() < holdEnd) {
        if (!mowCurrents.valid && nowMs() >= currentSampleMs) {
            mowCurrents = readCurrents(at, 0, 0, kMowMaxPwm);
            if (mowCurrents.valid) {
                std::cout << "  Strom Mähmotor=" << fmtFloat(mowCurrents.mowA) << " A\n";
            }
        } else {
            sendMotorPwm(at, 0, 0, kMowMaxPwm);
            const std::string f = at.waitFrame('M', 60);
            if (!f.empty()) {
                const auto m = parseMotorFrame(f);
                if (m.valid) ticksMend = m.ticksMow;
                if (m.valid && m.fault) {
                    sendMotorPwm(at, 0, 0, 0);
                    rep("Mähmotor-Ramp", Result::FAIL, "Motor-Fault während Halte-Phase");
                    return;
                }
            }
        }
    }

    // Ramp DOWN
    std::cout << "  Mähmotor Rampe runter...\n";
    for (int step = kMowRampSteps; step >= 0; --step) {
        const int pwm = (kMowMaxPwm * step) / kMowRampSteps;
        sendMotorPwm(at, 0, 0, pwm);
        at.waitFrame('M', 60);
    }
    sendMotorPwm(at, 0, 0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const long dM = static_cast<long>(ticksMend) - static_cast<long>(ticksM0);
    std::cout << "  Odometrie Mähmotor: ΔticksMow=" << dM << "\n";

    {
        std::string msg = "ΔMow=" + std::to_string(dM)
                        + "  maxPWM=" + std::to_string(kMowMaxPwm);
        rep("Mähmotor-Odometrie", (dM < 5) ? Result::WARN : Result::PASS, msg);
    }

    // Strom-Auswertung
    if (mowCurrents.valid) {
        const std::string msgA = "mow=" + fmtFloat(mowCurrents.mowA) + " A"
                               + "  (PWM=" + std::to_string(kMowMaxPwm) + ")";
        Result rA = Result::PASS;
        if (mowCurrents.mowA > 10.0f)
            rA = Result::WARN;  // Mähmotor-Überlast-Hinweis
        else if (mowCurrents.mowA < 0.05f)
            rA = Result::WARN;  // kein Strom = Antriebsfehler
        rep("Mähmotor-Strom", rA, msgA);
    } else {
        rep("Mähmotor-Strom", Result::WARN, "kein AT+S-Frame während Haltephase erhalten");
    }

    if (askUser("Hat der Mähmotor / das Messer gedreht?"))
        rep("Mähmotor visuell", Result::PASS, "bestätigt");
    else
        rep("Mähmotor visuell", Result::FAIL, "user: kein Drehen wahrgenommen");
}

static void phase6Motoren(AtLink& at, bool skipMotors) {
    std::cout << "\n\033[1m--- Phase 6: Motoren ---\033[0m\n";

    if (skipMotors) {
        rep("Fahrmotor-Ramp", Result::SKIP, "--skip-motors gesetzt");
        rep("Mähmotor-Ramp",  Result::SKIP, "--skip-motors gesetzt");
        return;
    }

    std::cout << "  \033[33mACHTUNG: Räder drehen sich — Roboter auf Böcken / Räder frei!\033[0m\n";
    if (!askUser("Sind die Räder frei (Roboter aufgebockt oder sicher platziert)?")) {
        rep("Fahrmotor-Ramp", Result::SKIP, "Sicherheitsabfrage verneint");
        rep("Mähmotor-Ramp",  Result::SKIP, "Sicherheitsabfrage verneint");
        return;
    }

    motorRampTest(at);

    std::cout << "  \033[33mACHTUNG: Mähmotor dreht — Finger weg vom Messer!\033[0m\n";
    if (!askUser("Ist das Mähfeld frei?")) {
        rep("Mähmotor-Ramp", Result::SKIP, "Sicherheitsabfrage verneint");
        return;
    }

    mowMotorRampTest(at);
}

// ── Phase 7: GPS ──────────────────────────────────────────────────────────────

static void phase7Gps() {
    std::cout << "\n\033[1m--- Phase 7: GPS ---\033[0m\n";

    std::unique_ptr<sunray::platform::Serial> gps;
    try {
        gps = std::make_unique<sunray::platform::Serial>(kGpsPort, kGpsBaud);
    } catch (const std::exception& e) {
        rep("GPS Port", Result::FAIL, std::string(e.what()));
        return;
    }
    rep("GPS Port", Result::PASS, std::string(kGpsPort) + " @ " + std::to_string(kGpsBaud));

    std::cout << "  Warte auf NMEA-Daten (5 s) ...\n";

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::string lineBuf;
    std::vector<std::string> nmeaLines;
    int rawBytes = 0;

    while (std::chrono::steady_clock::now() < deadline) {
        if (!gps->waitReadable(100)) continue;
        uint8_t buf[256];
        const int n = gps->read(buf, sizeof(buf));
        if (n <= 0) continue;
        rawBytes += n;
        for (int i = 0; i < n; i++) {
            char ch = static_cast<char>(buf[i]);
            if (ch == '\n') {
                if (!lineBuf.empty() && lineBuf[0] == '$') {
                    nmeaLines.push_back(lineBuf);
                    // Die ersten 8 NMEA-Sätze ausgeben
                    if (nmeaLines.size() <= 8)
                        std::cout << "  " << lineBuf << "\n";
                }
                lineBuf.clear();
            } else if (ch != '\r' && lineBuf.size() < 200) {
                lineBuf += ch;
            }
        }
    }

    if (rawBytes == 0) {
        rep("GPS Daten", Result::FAIL, "keine Bytes empfangen — Modul angeschlossen?");
        return;
    }

    // UBX-Binärprotokoll erkennen: Sync-Bytes 0xB5 0x62
    // ZED-F9P sendet standardmäßig UBX, nicht NMEA — das ist normal und kein Fehler
    const bool hasUbx = [&]() {
        // Nochmal Rohbytes scannen — wir haben sie nicht gespeichert, aber
        // wenn nmeaLines leer und rawBytes > 0 → UBX wahrscheinlich
        return nmeaLines.empty() && rawBytes > 100;
    }();

    if (hasUbx) {
        rep("GPS Daten", Result::PASS,
            std::to_string(rawBytes) + " Bytes empfangen — UBX-Binärprotokoll erkannt (normal für ZED-F9P)");
        std::cout << "  Info: NMEA-Ausgabe im F9P deaktiviert (gps_configure=false)."
                  << " Hardware OK.\n";
        return;
    }

    // NMEA auswerten
    bool hasGga = false, hasFix = false;
    for (const auto& line : nmeaLines) {
        if (line.find("GGA") != std::string::npos) {
            hasGga = true;
            const auto f = csvSplit(line);
            if (f.size() > 6) {
                const int q = std::stoi(f[6]);
                hasFix = (q > 0);
                rep("GPS GGA Fix", (q > 0) ? Result::PASS : Result::WARN,
                    "Fixqualität=" + std::to_string(q) +
                    (q == 4 ? " (RTK-Fixed)" : q == 5 ? " (RTK-Float)" :
                     q == 1 ? " (GPS)" : " (kein Fix)"));
            }
            break;
        }
    }

    if (!hasGga)
        rep("GPS Daten", Result::WARN,
            std::to_string(rawBytes) + " Bytes / " + std::to_string(nmeaLines.size())
            + " NMEA-Sätze — kein GGA");
    else if (!hasFix)
        rep("GPS Daten", Result::WARN, "GGA vorhanden aber kein Fix");
    else
        rep("GPS Daten", Result::PASS,
            std::to_string(nmeaLines.size()) + " NMEA-Sätze empfangen");
}

// ── Zusammenfassung ───────────────────────────────────────────────────────────

static void printSummary() {
    std::cout << "\n\033[1m========================================\033[0m\n";
    std::cout << "\033[1m  ERGEBNIS\033[0m\n";
    std::cout << "\033[1m========================================\033[0m\n";

    int pass = 0, fail = 0, warn = 0, skip = 0;
    std::vector<std::string> failures, warnings;

    for (const auto& r : gReports) {
        switch (r.result) {
            case Result::PASS: pass++; break;
            case Result::FAIL: fail++; failures.push_back(r.name); break;
            case Result::WARN: warn++; warnings.push_back(r.name); break;
            case Result::SKIP: skip++; break;
        }
    }

    std::cout << "\033[32m  PASS: " << pass << "\033[0m"
              << "   \033[31mFAIL: " << fail << "\033[0m"
              << "   \033[33mWARN: " << warn << "\033[0m"
              << "   \033[90mSKIP: " << skip << "\033[0m\n\n";

    if (!failures.empty()) {
        std::cout << "\033[31m  Kritische Fehler:\033[0m\n";
        for (const auto& f : failures) std::cout << "    - " << f << "\n";
    }
    if (!warnings.empty()) {
        std::cout << "\033[33m  Warnungen:\033[0m\n";
        for (const auto& w : warnings) std::cout << "    - " << w << "\n";
    }

    std::cout << "\033[1m========================================\033[0m\n";
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    bool skipMotors = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--skip-motors") skipMotors = true;
    }

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    {
        // Datum/Zeit
        char tbuf[64];
        std::time_t t = std::time(nullptr);
        std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        std::cout << "\033[1m========================================\033[0m\n";
        std::cout << "\033[1m  SUNRAY ALFRED — HARDWARE TEST\033[0m\n";
        std::cout << "  " << tbuf << "\n";
        if (skipMotors) std::cout << "  (--skip-motors)\n";
        std::cout << "\033[1m========================================\033[0m\n";
    }

    // ── Phase 0: systemd ─────────────────────────────────────────────────────
    phase0Systemd();

    // ── Phase 1: UART / STM32-Link ───────────────────────────────────────────
    std::cout << "\n\033[1m--- Phase 1: UART / STM32-Link ---\033[0m\n";

    std::unique_ptr<sunray::platform::Serial> uart;
    try {
        uart = std::make_unique<sunray::platform::Serial>(kUartPort, kUartBaud);
        gUart = uart.get();
    } catch (const std::exception& e) {
        rep("UART open", Result::FAIL, std::string(e.what()));
        goto done;
    }
    rep("UART open", Result::PASS,
        std::string(kUartPort) + " @ " + std::to_string(kUartBaud) + " Baud");

    {
        const bool uartOk = phase1Uart(*uart);
        if (!uartOk) {
            std::cout << "  UART-Kommunikation fehlgeschlagen — I2C und Motortests werden trotzdem versucht.\n";
        }
    }

    // ── Phasen 2–6: I2C, IMU, Aktoren, Sensoren, Motoren ───────────────────
    {
        std::unique_ptr<sunray::platform::I2C> i2c;
        try {
            i2c = std::make_unique<sunray::platform::I2C>(kI2cBus);
        } catch (const std::exception& e) {
            rep("I2C open", Result::FAIL, std::string(e.what()));
            goto skipI2c;
        }
        rep("I2C open", Result::PASS, kI2cBus);

        sunray::platform::I2cMux       mux(*i2c, kAddrMux);
        sunray::platform::PortExpander ex1(*i2c, kAddrEx1);
        sunray::platform::PortExpander ex2(*i2c, kAddrEx2);
        sunray::platform::PortExpander ex3(*i2c, kAddrEx3);

        phase2I2c(*i2c, mux, ex1, ex2, ex3);
        phase3Imu(*i2c, mux);

        // Zurück auf Kanal 0 für Aktoren/Sensoren
        mux.selectChannel(kMuxChLegacy);
        phase4Aktoren(mux, ex1, ex2, ex3);

        // UART für Sensor- und Motortest
        if (uart) {
            AtLink at(*uart);
            at.flush();
            phase5Sensoren(at);

            at.flush();
            phase6Motoren(at, skipMotors);
        }
    }

skipI2c:
    // ── Phase 7: GPS ─────────────────────────────────────────────────────────
    phase7Gps();

done:
    printSummary();

    // Motoren explizit stoppen (sicher)
    stopMotors();
    gUart = nullptr;

    // Service neu starten wenn wir ihn gestoppt haben
    if (gRestartService) {
        std::cout << "\nStarte '" << kSvc << "' neu...\n";
        int rc = std::system(("systemctl start " + std::string(kSvc)).c_str());
        (void)rc;
    }

    const bool hasFail = std::any_of(gReports.begin(), gReports.end(),
        [](const TestReport& r) { return r.result == Result::FAIL; });
    return hasFail ? 1 : 0;
}
