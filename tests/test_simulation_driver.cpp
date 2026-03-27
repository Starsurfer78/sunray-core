// test_simulation_driver.cpp — Unit tests for SimulationDriver (no hardware).
//
// Test groups:
//   [init]        — init() resets all state cleanly
//   [kinematics]  — PWM → pose update, encoder ticks, differential turn
//   [odometry]    — readOdometry() resets delta; MCU always connected
//   [sensors]     — manual bumper/lift injection; obstacle polygon trigger
//   [battery]     — constant voltage, charger not connected
//   [obstacles]   — polygon hit/miss, clearObstacles, multiple polygons
//   [gps]         — quality switching (no crash; state persists)
//   [thread]      — concurrent setMotorPwm + run() (sanitizer target)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "hal/SimulationDriver/SimulationDriver.h"

#include <cmath>
#include <thread>
#include <chrono>

using namespace sunray;
using Approx = Catch::Approx;

// ── Helpers ───────────────────────────────────────────────────────────────────

/// Drive the sim for approximately n milliseconds by calling run() in a loop.
/// Uses real wall time (steady_clock-based dt inside run()).
static void runFor(SimulationDriver& sim, int ms) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < deadline) {
        sim.run();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// ── [init] ────────────────────────────────────────────────────────────────────

TEST_CASE("SimulationDriver: init() returns true", "[init]") {
    SimulationDriver sim;
    REQUIRE(sim.init() == true);
}

TEST_CASE("SimulationDriver: init() resets pose to origin", "[init]") {
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(200, 200, 0);
    runFor(sim, 50);

    sim.init();  // reset
    auto pose = sim.getPose();
    REQUIRE(pose.x       == Approx(0.0).margin(1e-9));
    REQUIRE(pose.y       == Approx(0.0).margin(1e-9));
    REQUIRE(pose.heading == Approx(0.0).margin(1e-9));
}

TEST_CASE("SimulationDriver: init() clears pending ticks", "[init]") {
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(200, 200, 0);
    runFor(sim, 50);

    sim.init();
    auto odo = sim.readOdometry();
    REQUIRE(odo.leftTicks  == 0);
    REQUIRE(odo.rightTicks == 0);
}

TEST_CASE("SimulationDriver: init() clears injected faults", "[init]") {
    SimulationDriver sim;
    sim.init();
    sim.setBumperLeft(true);
    sim.setLift(true);
    sim.init();
    auto sensors = sim.readSensors();
    REQUIRE(sensors.bumperLeft == false);
    REQUIRE(sensors.lift       == false);
}

// ── [kinematics] ──────────────────────────────────────────────────────────────

TEST_CASE("SimulationDriver: straight forward moves x+", "[kinematics]") {
    // Heading 0 = east (+x direction in the current model)
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(255, 255, 0);
    runFor(sim, 200);

    auto pose = sim.getPose();
    // With maxSpeed 0.5 m/s and 200ms we expect ~0.1m forward
    REQUIRE(pose.x > 0.05);
    REQUIRE(std::abs(pose.y) < 0.01);  // no lateral drift
}

TEST_CASE("SimulationDriver: reverse moves x-", "[kinematics]") {
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(-255, -255, 0);
    runFor(sim, 200);

    auto pose = sim.getPose();
    REQUIRE(pose.x < -0.05);
}

TEST_CASE("SimulationDriver: spin right (right slower) turns clockwise", "[kinematics]") {
    // Right wheel slower → turn right (clockwise, negative heading in the current model)
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(200, 100, 0);
    runFor(sim, 200);

    auto pose = sim.getPose();
    REQUIRE(pose.heading < 0.0);
}

TEST_CASE("SimulationDriver: spin left turns counter-clockwise", "[kinematics]") {
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(100, 200, 0);
    runFor(sim, 200);

    auto pose = sim.getPose();
    REQUIRE(pose.heading > 0.0);
}

TEST_CASE("SimulationDriver: stopped motors — pose unchanged after run()", "[kinematics]") {
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(0, 0, 0);

    // prime the clock
    sim.run();
    auto poseBefore = sim.getPose();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sim.run();
    auto poseAfter = sim.getPose();

    REQUIRE(poseAfter.x       == Approx(poseBefore.x).margin(1e-9));
    REQUIRE(poseAfter.y       == Approx(poseBefore.y).margin(1e-9));
    REQUIRE(poseAfter.heading == Approx(poseBefore.heading).margin(1e-9));
}

TEST_CASE("SimulationDriver: setPose overrides position", "[kinematics]") {
    SimulationDriver sim;
    sim.init();
    sim.setPose({10.0, 20.0, 1.5});
    auto pose = sim.getPose();
    REQUIRE(pose.x       == Approx(10.0));
    REQUIRE(pose.y       == Approx(20.0));
    REQUIRE(pose.heading == Approx(1.5));
}

// ── [odometry] ────────────────────────────────────────────────────────────────

TEST_CASE("SimulationDriver: readOdometry() resets delta on each call", "[odometry]") {
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(255, 255, 0);
    runFor(sim, 100);

    auto first  = sim.readOdometry();
    auto second = sim.readOdometry();  // delta was consumed

    REQUIRE(first.leftTicks   > 0);
    REQUIRE(second.leftTicks  == 0);
    REQUIRE(second.rightTicks == 0);
}

TEST_CASE("SimulationDriver: MCU always reported connected", "[odometry]") {
    SimulationDriver sim;
    sim.init();
    auto odo = sim.readOdometry();
    REQUIRE(odo.mcuConnected == true);
}

TEST_CASE("SimulationDriver: forward motion generates positive ticks on both wheels", "[odometry]") {
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(255, 255, 0);
    runFor(sim, 200);

    auto odo = sim.readOdometry();
    REQUIRE(odo.leftTicks  > 0);
    REQUIRE(odo.rightTicks > 0);
}

TEST_CASE("SimulationDriver: mow motor running generates mow ticks", "[odometry]") {
    SimulationDriver sim;
    sim.init();
    sim.setMotorPwm(0, 0, 200);
    runFor(sim, 200);

    auto odo = sim.readOdometry();
    REQUIRE(odo.mowTicks > 0);
}

// ── [sensors] ─────────────────────────────────────────────────────────────────

TEST_CASE("SimulationDriver: bumperLeft injection", "[sensors]") {
    SimulationDriver sim;
    sim.init();
    sim.setBumperLeft(true);
    REQUIRE(sim.readSensors().bumperLeft == true);
    sim.setBumperLeft(false);
    REQUIRE(sim.readSensors().bumperLeft == false);
}

TEST_CASE("SimulationDriver: bumperRight injection", "[sensors]") {
    SimulationDriver sim;
    sim.init();
    sim.setBumperRight(true);
    REQUIRE(sim.readSensors().bumperRight == true);
}

TEST_CASE("SimulationDriver: lift injection", "[sensors]") {
    SimulationDriver sim;
    sim.init();
    sim.setLift(true);
    REQUIRE(sim.readSensors().lift == true);
    sim.setLift(false);
    REQUIRE(sim.readSensors().lift == false);
}

TEST_CASE("SimulationDriver: no faults by default", "[sensors]") {
    SimulationDriver sim;
    sim.init();
    sim.run();
    auto s = sim.readSensors();
    REQUIRE(s.bumperLeft  == false);
    REQUIRE(s.bumperRight == false);
    REQUIRE(s.lift        == false);
    REQUIRE(s.motorFault  == false);
    REQUIRE(s.stopButton  == false);
    REQUIRE(s.rain        == false);
}

// ── [battery] ─────────────────────────────────────────────────────────────────

TEST_CASE("SimulationDriver: readBattery() returns nominal voltage", "[battery]") {
    SimulationDriver sim;
    sim.init();
    auto bat = sim.readBattery();
    REQUIRE(bat.voltage > 20.0f);
    REQUIRE(bat.chargerConnected == false);
}

// ── [obstacles] ───────────────────────────────────────────────────────────────

TEST_CASE("SimulationDriver: obstacle polygon triggers bumper when robot is inside", "[obstacles]") {
    SimulationDriver sim;
    sim.init();

    // Place a 2x2m square around the origin
    sim.addObstacle({{-1,  1}, {1,  1}, {1, -1}, {-1, -1}});

    sim.run();  // robot at (0,0) — inside polygon
    auto s = sim.readSensors();
    REQUIRE(s.bumperLeft  == true);
    REQUIRE(s.bumperRight == true);
    REQUIRE(s.nearObstacle == true);
}

TEST_CASE("SimulationDriver: obstacle polygon does not trigger when robot is outside", "[obstacles]") {
    SimulationDriver sim;
    sim.init();
    sim.setPose({5.0, 5.0, 0.0});

    // Obstacle is at origin
    sim.addObstacle({{-1,  1}, {1,  1}, {1, -1}, {-1, -1}});

    sim.run();
    auto s = sim.readSensors();
    REQUIRE(s.bumperLeft   == false);
    REQUIRE(s.nearObstacle == false);
}

TEST_CASE("SimulationDriver: clearObstacles() removes all polygons", "[obstacles]") {
    SimulationDriver sim;
    sim.init();
    sim.addObstacle({{-1,  1}, {1,  1}, {1, -1}, {-1, -1}});
    sim.run();
    REQUIRE(sim.readSensors().bumperLeft == true);

    sim.clearObstacles();
    sim.run();
    REQUIRE(sim.readSensors().bumperLeft == false);
}

TEST_CASE("SimulationDriver: point-in-polygon — degenerate polygon (<3 vertices) — no crash", "[obstacles]") {
    SimulationDriver sim;
    sim.init();
    sim.addObstacle({{0, 0}, {1, 1}});  // only 2 vertices — should not match
    REQUIRE_NOTHROW(sim.run());
    REQUIRE(sim.readSensors().bumperLeft == false);
}

// ── [gps] ─────────────────────────────────────────────────────────────────────

TEST_CASE("SimulationDriver: default GPS quality is FIX", "[gps]") {
    SimulationDriver sim;
    sim.init();
    REQUIRE(sim.getGpsQuality() == GpsQuality::FIX);
}

TEST_CASE("SimulationDriver: GPS quality switching persists", "[gps]") {
    SimulationDriver sim;
    sim.init();
    sim.setGpsQuality(GpsQuality::FLOAT);
    REQUIRE(sim.getGpsQuality() == GpsQuality::FLOAT);
    sim.setGpsQuality(GpsQuality::NO_FIX);
    REQUIRE(sim.getGpsQuality() == GpsQuality::NO_FIX);
    sim.setGpsQuality(GpsQuality::FIX);
    REQUIRE(sim.getGpsQuality() == GpsQuality::FIX);
}

// ── [thread] ──────────────────────────────────────────────────────────────────

TEST_CASE("SimulationDriver: concurrent setMotorPwm + run() — no data race", "[thread]") {
    // This test is primarily a ThreadSanitizer target.
    // It verifies the mutex protects shared state without deadlocking.
    SimulationDriver sim;
    sim.init();

    std::atomic<bool> stop{false};

    std::thread writer([&]() {
        int pwm = 0;
        while (!stop.load()) {
            sim.setMotorPwm(pwm, pwm, 0);
            pwm = (pwm + 50) % 256;
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    });

    std::thread reader([&]() {
        while (!stop.load()) {
            sim.run();
            sim.readOdometry();
            sim.readSensors();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);
    writer.join();
    reader.join();

    // If we reach here without crashing/deadlocking the test passes.
    REQUIRE(true);
}
