// test_i2c.cpp — Unit tests for platform::I2C (hardware-free).
//
// Real I2C device tests require Pi hardware and are tagged [.][hardware].
// Run hardware tests with: ./sunray_tests "[hardware]"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

#include "../platform/I2C.h"

using sunray::platform::I2C;

// ── Error paths ───────────────────────────────────────────────────────────────

TEST_CASE("I2C: non-existent bus throws runtime_error", "[i2c]") {
    CHECK_THROWS_AS(I2C("/dev/i2c-99"), std::runtime_error);
}

TEST_CASE("I2C: error message contains bus path", "[i2c]") {
    try {
        I2C bus("/dev/i2c-no_such_bus");
        FAIL("Expected exception not thrown");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        CHECK(msg.find("/dev/i2c-no_such_bus") != std::string::npos);
    }
}

// ── Type traits ───────────────────────────────────────────────────────────────

TEST_CASE("I2C: type is non-copyable", "[i2c]") {
    CHECK_FALSE(std::is_copy_constructible_v<I2C>);
    CHECK_FALSE(std::is_copy_assignable_v<I2C>);
}

TEST_CASE("I2C: type is move-constructible and move-assignable", "[i2c]") {
    CHECK(std::is_move_constructible_v<I2C>);
    CHECK(std::is_move_assignable_v<I2C>);
}

// ── Hardware tests (tagged [.][hardware] — skipped by default) ────────────────
//
// To run on Raspberry Pi with Alfred connected:
//   ./sunray_tests "[hardware]"
//
// Prerequisites:
//   - /dev/i2c-1 accessible (sudo usermod -aG i2c $USER or run as root)
//   - PCA9555 at 0x22 (EX3 LED panel) present and powered

TEST_CASE("I2C: [hardware] open bus succeeds", "[.][hardware]") {
    I2C bus("/dev/i2c-1");
    CHECK(bus.isOpen());
    CHECK(bus.busPath() == "/dev/i2c-1");
}

TEST_CASE("I2C: [hardware] PCA9555 EX3 — configure port 0 as output", "[.][hardware]") {
    // PCA9555 command register 0x06 = Configuration Port 0 (0x00 = all outputs)
    I2C bus("/dev/i2c-1");
    uint8_t cmd[2] = {0x06, 0x00};
    CHECK(bus.write(0x22, cmd, sizeof(cmd)));
}

TEST_CASE("I2C: [hardware] PCA9555 EX3 — read output latch register", "[.][hardware]") {
    // PCA9555 Output Port 0 register address = 0x02
    I2C bus("/dev/i2c-1");
    uint8_t reg = 0x02;
    uint8_t val = 0xFF;
    CHECK(bus.writeRead(0x22, &reg, 1, &val, 1));
    // Value is valid hardware state — just check it returned without error
    // (bits 0..5 control LEDs, bits 6/7 are don't-care outputs)
    SUCCEED("Read output register without error");
}
