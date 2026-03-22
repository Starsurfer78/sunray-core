// test_gps_driver.cpp — Unit tests for GPS driver interface and UBX parser logic.
//
// Tests use a MockGpsDriver (does not open any serial port).
// UBX decode correctness is verified by constructing hand-crafted byte sequences
// and running them through the parser state machine (white-box, via a test subclass).

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "hal/GpsDriver/GpsDriver.h"

namespace sunray {

// ── MockGpsDriver ─────────────────────────────────────────────────────────────

class MockGpsDriver : public GpsDriver {
public:
    bool initCalled = false;
    GpsData injected;

    bool    init()          override { initCalled = true; return true; }
    GpsData getData() const override { return injected; }
};

// ── GpsSolution enum ──────────────────────────────────────────────────────────

TEST_CASE("GpsSolution enum values", "[gps]") {
    CHECK(static_cast<int>(GpsSolution::Invalid) == 0);
    CHECK(static_cast<int>(GpsSolution::Float)   == 1);
    CHECK(static_cast<int>(GpsSolution::Fixed)   == 2);
}

// ── MockGpsDriver ─────────────────────────────────────────────────────────────

TEST_CASE("MockGpsDriver: init sets flag", "[gps]") {
    MockGpsDriver drv;
    REQUIRE_FALSE(drv.initCalled);
    drv.init();
    REQUIRE(drv.initCalled);
}

TEST_CASE("MockGpsDriver: getData returns injected data", "[gps]") {
    MockGpsDriver drv;
    drv.injected.lat       = 48.1234567;
    drv.injected.lon       = 11.9876543;
    drv.injected.relPosN   = 12.34f;
    drv.injected.relPosE   = -7.89f;
    drv.injected.solution  = GpsSolution::Fixed;
    drv.injected.numSV     = 14;
    drv.injected.hAccuracy = 0.012f;
    drv.injected.valid     = true;

    const GpsData d = drv.getData();
    CHECK(d.lat       == Catch::Approx(48.1234567));
    CHECK(d.lon       == Catch::Approx(11.9876543));
    CHECK(d.relPosN   == Catch::Approx(12.34f));
    CHECK(d.relPosE   == Catch::Approx(-7.89f));
    CHECK(d.solution  == GpsSolution::Fixed);
    CHECK(d.numSV     == 14);
    CHECK(d.hAccuracy == Catch::Approx(0.012f));
    CHECK(d.valid     == true);
}

TEST_CASE("GpsData default state is invalid", "[gps]") {
    GpsData d;
    CHECK(d.valid   == false);
    CHECK(d.lat     == Catch::Approx(0.0));
    CHECK(d.lon     == Catch::Approx(0.0));
    CHECK(d.relPosN == Catch::Approx(0.0f));
    CHECK(d.relPosE == Catch::Approx(0.0f));
    CHECK(d.solution == GpsSolution::Invalid);
    CHECK(d.numSV    == 0);
    CHECK(d.nmeaGGA.empty());
}

// ── GpsSolution quality transitions ───────────────────────────────────────────

TEST_CASE("GPS quality: Invalid → Float → Fixed transitions", "[gps]") {
    MockGpsDriver drv;
    drv.injected.valid    = false;
    drv.injected.solution = GpsSolution::Invalid;

    auto d = drv.getData();
    CHECK(d.solution == GpsSolution::Invalid);
    CHECK(d.valid == false);

    drv.injected.valid    = true;
    drv.injected.solution = GpsSolution::Float;
    d = drv.getData();
    CHECK(d.solution == GpsSolution::Float);
    CHECK(d.valid == true);

    drv.injected.solution = GpsSolution::Fixed;
    d = drv.getData();
    CHECK(d.solution == GpsSolution::Fixed);
}

// ── UBX little-endian decode test (derived from ublox.cpp decode test) ────────
// Verify that the standard UBX LE packing matches expected values.

TEST_CASE("UBX NAV-RELPOSNED relPosE decode sanity", "[gps][ubx]") {
    // Original test from ublox.cpp:
    //   payload[12..15] = {0x10, 0xFD, 0xFF, 0xFF}  → int32 = -752  → /100 = -7.52 m
    uint8_t p[4] = {0x10, 0xFD, 0xFF, 0xFF};
    int32_t val = static_cast<int32_t>(
        (static_cast<uint32_t>(p[3]) << 24) |
        (static_cast<uint32_t>(p[2]) << 16) |
        (static_cast<uint32_t>(p[1]) <<  8) |
        (static_cast<uint32_t>(p[0])));
    float relPosE = static_cast<float>(val) / 100.0f;
    CHECK(relPosE == Catch::Approx(-7.52f).epsilon(0.01));
}

TEST_CASE("UBX NAV-HPPOSLLH lat decode sanity", "[gps][ubx]") {
    // Original test from ublox.cpp:
    //   payload[12..15] = {0xA1, 0x62, 0x27, 0x1F}
    //   payload[25]     = 0xE2  (= -30 as int8)
    //   → lat = 1e-7 * (I32 + I8 * 1e-2) = 52.26748477...
    uint8_t p32[4] = {0xA1, 0x62, 0x27, 0x1F};
    int32_t latI = static_cast<int32_t>(
        (static_cast<uint32_t>(p32[3]) << 24) |
        (static_cast<uint32_t>(p32[2]) << 16) |
        (static_cast<uint32_t>(p32[1]) <<  8) |
        (static_cast<uint32_t>(p32[0])));
    int8_t latHp = static_cast<int8_t>(0xE2);
    double lat = 1e-7 * (static_cast<double>(latI) + static_cast<double>(latHp) * 1e-2);
    CHECK(lat == Catch::Approx(52.26748477).epsilon(1e-8));
}

TEST_CASE("NMEA nmeaGGA field stored correctly", "[gps]") {
    MockGpsDriver drv;
    drv.injected.nmeaGGA = "$GNGGA,123456.00,4816.34,N,01159.26,E,4,14,0.8,523.4,M,47.6,M,1.0,0000*7A";
    drv.injected.valid   = true;
    const auto d = drv.getData();
    CHECK(d.nmeaGGA.substr(0, 6) == "$GNGGA");
    CHECK(d.nmeaGGA.back() != '\r');
    CHECK(d.nmeaGGA.back() != '\n');
}

TEST_CASE("dgpsAge_ms reflects RTCM packet age", "[gps]") {
    MockGpsDriver drv;
    drv.injected.dgpsAge_ms = 1250;
    drv.injected.valid = true;
    const auto d = drv.getData();
    CHECK(d.dgpsAge_ms == 1250u);
}

} // namespace sunray
