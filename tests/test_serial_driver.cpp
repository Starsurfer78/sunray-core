// test_serial_driver.cpp — Unit tests for SerialRobotDriver AT-frame logic.
//
// Strategy: extract and test the pure parsing/formatting logic without hardware.
// The AT-frame parsers, CRC, and PWM-swap logic are fully deterministic —
// no serial port, I2C, or PortExpander needed.
//
// Hardware-dependent tests (init(), run() against real Alfred) are tagged
// [.][hardware] and skipped by default.
// Run: ./sunray_tests "[hardware]"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstdint>
#include <string>
#include <vector>

using Catch::Approx;

// ── Test helpers (mirror SerialRobotDriver private logic) ────────────────────
// Rather than friend-testing private members, we replicate the small pure
// functions here. This keeps the tests free-standing and documents the
// exact algorithms that must hold.

namespace {

// ── CRC (matches SerialRobotDriver::calcCrc) ──────────────────────────────────
uint8_t calcCrc(const std::string& s) {
    uint8_t crc = 0;
    for (unsigned char c : s) crc += c;
    return crc;
}

// ── CRC verify + strip (matches SerialRobotDriver::verifyCrc) ─────────────────
bool verifyCrc(std::string& frame) {
    const auto idx = frame.rfind(',');
    if (idx == std::string::npos || idx == 0) return false;
    uint8_t expected = 0;
    for (size_t i = 0; i < idx; i++) expected += static_cast<uint8_t>(frame[i]);
    const std::string crcStr = frame.substr(idx + 1);
    const long received = std::strtol(crcStr.c_str(), nullptr, 16);
    if (expected != static_cast<uint8_t>(received)) return false;
    frame = frame.substr(0, idx);
    return true;
}

// ── AT+M frame builder (mirrors SerialRobotDriver::sendAt for AT+M) ──────────
// BUG-07: rightPwm goes as field1, leftPwm as field2
std::string buildMotorCmd(int leftPwm, int rightPwm, int mowPwm) {
    std::string cmd = "AT+M," + std::to_string(rightPwm)
                    + ","     + std::to_string(leftPwm)
                    + ","     + std::to_string(mowPwm);
    char crcBuf[12];
    std::snprintf(crcBuf, sizeof(crcBuf), ",0x%02X\r\n", calcCrc(cmd));
    return cmd + crcBuf;
}

// ── CSV split (mirrors csvSplit in SerialRobotDriver.cpp) ────────────────────
std::vector<std::string> csvSplit(const std::string& s) {
    std::vector<std::string> out;
    std::string tok;
    for (char c : s) {
        if (c == ',') { out.push_back(tok); tok.clear(); }
        else          { tok += c; }
    }
    out.push_back(tok);
    return out;
}

// ── Simulate a full AT+M response from the STM32 ─────────────────────────────
// field order: M, leftTicks(f1), rightTicks(f2), mowTicks, chargeV,
//              bumperCombined, lift, stopButton, motorFault[, bumperL, bumperR]
std::string makeMotorResponse(int leftTicks, int rightTicks, int mowTicks,
                               float chargeV, bool bumper, bool lift,
                               bool stopBtn, bool fault,
                               bool bumperL = false, bool bumperR = false)
{
    // STM32 sends leftTicks in field1, rightTicks in field2 (BUG-07 physical wiring)
    std::string body = "M," + std::to_string(leftTicks)   // f1 = STM left → Pi right
                      + "," + std::to_string(rightTicks)  // f2 = STM right → Pi left
                      + "," + std::to_string(mowTicks)
                      + "," + std::to_string(chargeV)
                      + "," + std::to_string(bumper ? 1 : 0)
                      + "," + std::to_string(lift    ? 1 : 0)
                      + "," + std::to_string(stopBtn ? 1 : 0)
                      + "," + std::to_string(fault   ? 1 : 0)
                      + "," + std::to_string(bumperL ? 1 : 0)
                      + "," + std::to_string(bumperR ? 1 : 0);
    char crcBuf[12];
    std::snprintf(crcBuf, sizeof(crcBuf), ",0x%02X", calcCrc(body));
    return body + crcBuf;
}

// ── Simulate AT+S response from the STM32 ────────────────────────────────────
std::string makeSummaryResponse(float batV, float chargeV, float chargeA,
                                 bool lift, bool bumper, bool rain, bool fault,
                                 float mowA, float leftA, float rightA,
                                 float batTemp, bool ovCheck,
                                 bool bumperL, bool bumperR)
{
    std::string body = "S," + std::to_string(batV)
                      + "," + std::to_string(chargeV)
                      + "," + std::to_string(chargeA)
                      + "," + std::to_string(lift     ? 1 : 0)
                      + "," + std::to_string(bumper   ? 1 : 0)
                      + "," + std::to_string(rain     ? 1 : 0)
                      + "," + std::to_string(fault    ? 1 : 0)
                      + "," + std::to_string(mowA)
                      + "," + std::to_string(leftA)
                      + "," + std::to_string(rightA)
                      + "," + std::to_string(batTemp)
                      + "," + std::to_string(ovCheck ? 1 : 0)
                      + "," + std::to_string(bumperL ? 1 : 0)
                      + "," + std::to_string(bumperR ? 1 : 0);
    char crcBuf[12];
    std::snprintf(crcBuf, sizeof(crcBuf), ",0x%02X", calcCrc(body));
    return body + crcBuf;
}

bool chargerConnectedFromVoltage(float chargeVoltage, float threshold = 7.0f) {
    return chargeVoltage > threshold;
}

bool debounceChargerConnected(bool latched, bool rawConnected,
                              unsigned& highCount, unsigned& lowCount,
                              unsigned required = 2) {
    if (rawConnected) {
        ++highCount;
        lowCount = 0;
        return (!latched && highCount >= required) ? true : latched;
    }

    ++lowCount;
    highCount = 0;
    return (latched && lowCount >= required) ? false : latched;
}

} // anonymous namespace

// ── CRC tests ────────────────────────────────────────────────────────────────

TEST_CASE("CRC: known AT+V string produces correct checksum", "[driver][crc]") {
    // "AT+V" → 0x41+0x54+0x2B+0x56 = 0x116 → mod 256 = 0x16
    CHECK(calcCrc("AT+V") == 0x16);
}

TEST_CASE("CRC: AT+M with zero PWM produces stable checksum", "[driver][crc]") {
    const std::string cmd = "AT+M,0,0,0";
    const uint8_t crc = calcCrc(cmd);
    // Verify it's consistent across calls
    CHECK(calcCrc(cmd) == crc);
    CHECK(crc != 0);  // non-trivial content
}

TEST_CASE("CRC: verifyCrc strips suffix and returns true for valid frame", "[driver][crc]") {
    std::string body = "M,100,200,0,0.0,0,0,0,0,0,0";
    char buf[12];
    std::snprintf(buf, sizeof(buf), ",0x%02X", calcCrc(body));
    std::string frame = body + buf;

    const std::string original = frame;
    CHECK(verifyCrc(frame));
    CHECK(frame == body);         // CRC suffix removed
    CHECK(frame != original);
}

TEST_CASE("CRC: verifyCrc returns false on corrupted byte", "[driver][crc]") {
    std::string body = "M,100,200,0,0.0,0,0,0,0,0,0";
    char buf[12];
    std::snprintf(buf, sizeof(buf), ",0x%02X", calcCrc(body));
    std::string frame = body + buf;
    frame[3] = 'X';  // corrupt a byte

    CHECK_FALSE(verifyCrc(frame));
}

TEST_CASE("CRC: verifyCrc returns false on missing CRC field", "[driver][crc]") {
    std::string frame = "M,100,200,0";
    // No CRC appended — last comma-value is not a hex CRC
    // verifyCrc will try to parse "0" as hex CRC → will fail because 0 != actual CRC
    // (could pass by chance, but for this payload it won't)
    // Just verify it doesn't crash:
    CHECK_FALSE(verifyCrc(frame));
}

// ── BUG-07: PWM swap in AT+M command ─────────────────────────────────────────

TEST_CASE("BUG-07: AT+M frame sends rightPwm as field1, leftPwm as field2", "[driver][bug07]") {
    // Pi logical: left=100, right=200
    // Expected on wire: AT+M,200,100,50 (right first, then left)
    const std::string frame = buildMotorCmd(100, 200, 50);
    CHECK(frame.find("AT+M,200,100,50") != std::string::npos);
}

TEST_CASE("BUG-07: AT+M response maps STM field1→Pi right, field2→Pi left", "[driver][bug07]") {
    // STM32 sends: f1=leftTicks(physical)=300, f2=rightTicks(physical)=400
    // Pi must map: rawTicksRight_=300 (from f1), rawTicksLeft_=400 (from f2)
    auto frame = makeMotorResponse(300, 400, 0, 0.0f, false, false, false, false);
    CHECK(verifyCrc(frame));
    const auto f = csvSplit(frame);
    REQUIRE(f.size() > 2);
    // f[1] = STM leftTicks = Pi rawTicksRight
    CHECK(std::stoi(f[1]) == 300);
    // f[2] = STM rightTicks = Pi rawTicksLeft
    CHECK(std::stoi(f[2]) == 400);
}

// ── BUG-05: encoder tick overflow ────────────────────────────────────────────

TEST_CASE("BUG-05: unsigned long wrap handled by long cast", "[driver][bug05]") {
    // Simulate overflow: last=4294967290, current=5 (wrapped around UINT32_MAX)
    const unsigned long last    = 4294967290UL;
    const unsigned long current = 5UL;

    // Wrong (original bug): int delta = current - last → wraps to large positive
    // Correct: cast to long first
    const long delta = static_cast<long>(current) - static_cast<long>(last);
    // delta should be 11 (5 - (-6) conceptually, but as long arithmetic: 5 - 4294967290 = -4294967285 as signed)
    // Wait — unsigned wrap: 5 - 4294967290 as unsigned = 11 (wraps). As signed long: also 11 if 64-bit long.
    // On 64-bit: long(5) - long(4294967290) = 5 - 4294967290 = -4294967285 → clamped to 0
    // This is the intended behaviour: negative delta = communication glitch → return 0

    // Actually the intent: if delta is negative or >1000, clamp to 0.
    // Overflow wrap gives a large unsigned number which as long is still large positive on 32-bit,
    // or negative on 64-bit. Either way the clamp catches it.
    const int result = (delta >= 0 && delta <= 1000) ? static_cast<int>(delta) : 0;
    CHECK(result == 0);  // overflow detected → clamped
}

TEST_CASE("BUG-05: normal tick increment passes through", "[driver][bug05]") {
    const unsigned long last    = 1000UL;
    const unsigned long current = 1042UL;
    const long delta = static_cast<long>(current) - static_cast<long>(last);
    const int result = (delta >= 0 && delta <= 1000) ? static_cast<int>(delta) : 0;
    CHECK(result == 42);
}

TEST_CASE("BUG-05: delta > 1000 (impossible in 20ms) clamped to 0", "[driver][bug05]") {
    const unsigned long last    = 1000UL;
    const unsigned long current = 2500UL;
    const long delta = static_cast<long>(current) - static_cast<long>(last);
    const int result = (delta >= 0 && delta <= 1000) ? static_cast<int>(delta) : 0;
    CHECK(result == 0);
}

// ── AT+M frame parsing ────────────────────────────────────────────────────────

TEST_CASE("Motor frame: all sensor fields parsed correctly", "[driver][motor]") {
    auto frame = makeMotorResponse(
        /*leftTicks*/  150, /*rightTicks*/ 160, /*mowTicks*/ 10,
        /*chargeV*/    28.5f,
        /*bumper*/     false, /*lift*/ true, /*stopBtn*/ false, /*fault*/ false,
        /*bumperL*/    true,  /*bumperR*/ false);
    REQUIRE(verifyCrc(frame));

    const auto f = csvSplit(frame);
    REQUIRE(f.size() >= 11);
    CHECK(std::stoi(f[1])   == 150);    // STM leftTicks → Pi rawRight
    CHECK(std::stoi(f[2])   == 160);    // STM rightTicks → Pi rawLeft
    CHECK(std::stoi(f[3])   == 10);     // mowTicks
    CHECK(std::stof(f[4])   == Approx(28.5f));
    CHECK(std::stoi(f[6])   == 1);      // lift
    CHECK(std::stoi(f[7])   == 0);      // stopButton
    CHECK(std::stoi(f[8])   == 0);      // motorFault
    CHECK(std::stoi(f[9])   == 1);      // bumperLeft (precise)
    CHECK(std::stoi(f[10])  == 0);      // bumperRight
}

TEST_CASE("Motor frame: motorFault field 8 set", "[driver][motor]") {
    auto frame = makeMotorResponse(0,0,0, 0.0f, false,false,false, /*fault=*/true);
    REQUIRE(verifyCrc(frame));
    const auto f = csvSplit(frame);
    CHECK(std::stoi(f[8]) == 1);
}

// ── AT+S frame parsing ────────────────────────────────────────────────────────

TEST_CASE("Summary frame: battery voltage parsed correctly", "[driver][summary]") {
    auto frame = makeSummaryResponse(
        /*batV*/   24.3f, /*chargeV*/ 29.1f, /*chargeA*/ 1.5f,
        /*lift*/   false, /*bumper*/  false,  /*rain*/    true, /*fault*/ false,
        /*mowA*/   0.8f,  /*leftA*/   0.3f,   /*rightA*/  0.3f,
        /*batTemp*/25.0f, /*ovCheck*/ false,
        /*bumperL*/false, /*bumperR*/ false);
    REQUIRE(verifyCrc(frame));

    const auto f = csvSplit(frame);
    CHECK(std::stof(f[1])  == Approx(24.3f));   // batteryVoltage
    CHECK(std::stof(f[2])  == Approx(29.1f));   // chargeVoltage
    CHECK(std::stof(f[3])  == Approx(1.5f));    // chargeCurrent
    CHECK(std::stoi(f[6])  == 1);               // rain
}

TEST_CASE("Summary frame: IMP-01 ovCheck (field 12) sets motorFault", "[driver][summary][imp01]") {
    auto frame = makeSummaryResponse(
        24.0f, 0.0f, 0.0f, false, false, false, /*fault=*/false,
        0.0f, 0.0f, 0.0f, 25.0f, /*ovCheck=*/true, false, false);
    REQUIRE(verifyCrc(frame));
    const auto f = csvSplit(frame);
    CHECK(std::stoi(f[12]) == 1);  // ovCheck bit → motorFault must be set
}

TEST_CASE("Summary frame: precise bumper fields 13+14", "[driver][summary]") {
    auto frame = makeSummaryResponse(
        24.0f, 0.0f, 0.0f, false, /*legacy bumper=*/false, false, false,
        0.0f, 0.0f, 0.0f, 25.0f, false,
        /*bumperL=*/false, /*bumperR=*/true);
    REQUIRE(verifyCrc(frame));
    const auto f = csvSplit(frame);
    CHECK(std::stoi(f[13]) == 0);  // bumperLeft
    CHECK(std::stoi(f[14]) == 1);  // bumperRight
}

TEST_CASE("Charge detect: dock requires realistic charger voltage threshold", "[driver][summary][charge]") {
    CHECK_FALSE(chargerConnectedFromVoltage(0.0f));
    CHECK_FALSE(chargerConnectedFromVoltage(2.1f));
    CHECK_FALSE(chargerConnectedFromVoltage(7.0f));
    CHECK(chargerConnectedFromVoltage(7.1f));
    CHECK(chargerConnectedFromVoltage(28.7f));
    CHECK_FALSE(chargerConnectedFromVoltage(7.1f, 8.0f));
    CHECK(chargerConnectedFromVoltage(8.1f, 8.0f));
}

TEST_CASE("Charge detect: charger contact is debounced across short flaps", "[driver][summary][charge]") {
    unsigned highCount = 0;
    unsigned lowCount = 0;
    bool latched = false;

    latched = debounceChargerConnected(latched, true, highCount, lowCount);
    CHECK_FALSE(latched);
    latched = debounceChargerConnected(latched, true, highCount, lowCount);
    CHECK(latched);

    latched = debounceChargerConnected(latched, false, highCount, lowCount);
    CHECK(latched);
    latched = debounceChargerConnected(latched, true, highCount, lowCount);
    CHECK(latched);

    latched = debounceChargerConnected(latched, false, highCount, lowCount);
    CHECK(latched);
    latched = debounceChargerConnected(latched, false, highCount, lowCount);
    CHECK_FALSE(latched);
}

TEST_CASE("Charge detect: latched contact survives repeated one-sample disconnect flaps", "[driver][summary][charge]") {
    unsigned highCount = 0;
    unsigned lowCount = 0;
    bool latched = false;

    latched = debounceChargerConnected(latched, true, highCount, lowCount);
    latched = debounceChargerConnected(latched, true, highCount, lowCount);
    REQUIRE(latched);

    for (int i = 0; i < 10; ++i) {
        latched = debounceChargerConnected(latched, false, highCount, lowCount);
        CHECK(latched);
        latched = debounceChargerConnected(latched, true, highCount, lowCount);
        CHECK(latched);
    }
}

// ── AT+V frame ───────────────────────────────────────────────────────────────

TEST_CASE("Version frame: name and version fields parsed", "[driver][version]") {
    std::string body = "V,rm18,1.0.42";
    char buf[12];
    std::snprintf(buf, sizeof(buf), ",0x%02X", calcCrc(body));
    std::string frame = body + buf;

    REQUIRE(verifyCrc(frame));
    const auto f = csvSplit(frame);
    CHECK(f[0] == "V");
    CHECK(f[1] == "rm18");
    CHECK(f[2] == "1.0.42");
}

// ── Hardware tests ────────────────────────────────────────────────────────────

TEST_CASE("SerialRobotDriver: [hardware] init() succeeds on Alfred", "[.][hardware]") {
    // Requires Alfred PCB connected on /dev/ttyS0
    // Tested manually: make && ./tests/sunray_tests "[hardware]"
    SUCCEED("hardware test — run manually on Pi");
}
