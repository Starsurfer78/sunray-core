// test_robot.cpp — Unit tests for Robot class (hardware-free).
//
// All tests use MockHardware — a complete HardwareInterface implementation
// that records calls and returns configurable sensor data.
// No serial port, no I2C, no filesystem access required.
//
// Test groups:
//   [construction]  — null-ptr guards, ownership transfer
//   [init]          — init() return value, LED reset, log output
//   [run]           — sensor reads, LED updates, safety stop, counter
//   [state]         — state transitions, command guards
//   [battery]       — low/critical battery → dock/shutdown
//   [loop]          — stop() exits loop(), motors zeroed on exit

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/Robot.h"
#include "core/Config.h"
#include "core/Logger.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <thread>

using namespace sunray;

// ── MockHardware ───────────────────────────────────────────────────────────────

struct MockHardware : public HardwareInterface {
    // Configurable return values
    bool         initResult   = true;
    OdometryData odometry;
    SensorData   sensors;
    BatteryData  battery;
    std::string  robotId      = "AA:BB:CC:DD:EE:FF";
    float        cpuTemp      = 45.0f;
    std::string  mcuFwName    = "alfred";
    std::string  mcuFwVersion = "1.2.3";

    // Call counters / log
    int initCalls     = 0;
    int runCalls      = 0;
    int resetFaultCalls = 0;

    struct SetMotorCall  { int left, right, mow; };
    struct SetLedCall    { LedId id; LedState state; };
    struct SetBuzzerCall { bool on; };

    std::vector<SetMotorCall>  motorCalls;
    std::vector<SetLedCall>    ledCalls;
    std::vector<SetBuzzerCall> buzzerCalls;
    bool keepPowerOnFlag = true;
    int  keepPowerOnCalls = 0;

    // HardwareInterface implementation
    bool         init()           override { ++initCalls; return initResult; }
    void         run()            override { ++runCalls; }
    void         setMotorPwm(int l, int r, int m) override { motorCalls.push_back({l,r,m}); }
    void         resetMotorFault()  override { ++resetFaultCalls; }
    OdometryData readOdometry()  override { return odometry; }
    SensorData   readSensors()   override { return sensors; }
    BatteryData  readBattery()   override { return battery; }
    void         setBuzzer(bool on) override { buzzerCalls.push_back({on}); }
    void         setLed(LedId id, LedState state) override { ledCalls.push_back({id,state}); }
    void         keepPowerOn(bool flag) override { keepPowerOnFlag = flag; ++keepPowerOnCalls; }
    float        getCpuTemperature()    override { return cpuTemp; }
    std::string  getRobotId()           override { return robotId; }
    std::string  getMcuFirmwareName()   override { return mcuFwName; }
    std::string  getMcuFirmwareVersion()override { return mcuFwVersion; }

    // Helpers for tests
    bool hadMotorStop() const {
        for (auto& c : motorCalls)
            if (c.left == 0 && c.right == 0 && c.mow == 0) return true;
        return false;
    }
};

// ── Helpers to build a Robot with mock dependencies ────────────────────────────

static std::shared_ptr<Config> makeConfig() {
    // Use /tmp path — NullLogger does not need a real file.
    // Config ctor falls back to built-in defaults if file missing.
    return std::make_shared<Config>("/tmp/sunray_test_robot_config.json");
}

static std::shared_ptr<Logger> makeLogger() {
    return std::make_shared<NullLogger>();
}

/// Builds a Robot and returns a raw pointer to the mock for assertions.
/// hw_raw is set to the mock; the unique_ptr is moved into Robot.
static std::pair<Robot, MockHardware*> makeRobot(bool initResult = true) {
    auto hw_owned = std::make_unique<MockHardware>();
    hw_owned->initResult = initResult;
    MockHardware* raw = hw_owned.get();
    return { Robot(std::move(hw_owned), makeConfig(), makeLogger()), raw };
}

// ── [construction] ─────────────────────────────────────────────────────────────

TEST_CASE("Robot: null hw throws", "[construction]") {
    REQUIRE_THROWS_AS(
        Robot(nullptr, makeConfig(), makeLogger()),
        std::invalid_argument
    );
}

TEST_CASE("Robot: null config throws", "[construction]") {
    auto hw = std::make_unique<MockHardware>();
    REQUIRE_THROWS_AS(
        Robot(std::move(hw), nullptr, makeLogger()),
        std::invalid_argument
    );
}

TEST_CASE("Robot: null logger throws", "[construction]") {
    auto hw = std::make_unique<MockHardware>();
    REQUIRE_THROWS_AS(
        Robot(std::move(hw), makeConfig(), nullptr),
        std::invalid_argument
    );
}

// ── [init] ────────────────────────────────────────────────────────────────────

TEST_CASE("Robot: init() calls hw->init() once", "[init]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    REQUIRE(hw->initCalls == 1);
}

TEST_CASE("Robot: init() returns true when hw succeeds", "[init]") {
    auto [robot, hw] = makeRobot(true);
    REQUIRE(robot.init() == true);
}

TEST_CASE("Robot: init() returns false when hw fails", "[init]") {
    auto [robot, hw] = makeRobot(false);
    REQUIRE(robot.init() == false);
}

TEST_CASE("Robot: init() resets all three LEDs to OFF", "[init]") {
    auto [robot, hw] = makeRobot();
    hw->ledCalls.clear();
    robot.init();

    // Expect at least one OFF call per LED
    bool led1Off = false, led2Off = false, led3Off = false;
    for (auto& c : hw->ledCalls) {
        if (c.id == LedId::LED_1 && c.state == LedState::OFF) led1Off = true;
        if (c.id == LedId::LED_2 && c.state == LedState::OFF) led2Off = true;
        if (c.id == LedId::LED_3 && c.state == LedState::OFF) led3Off = true;
    }
    REQUIRE(led1Off);
    REQUIRE(led2Off);
    REQUIRE(led3Off);
}

TEST_CASE("Robot: isRunning() is false before loop()", "[init]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    REQUIRE(robot.isRunning() == false);
}

// ── [run] ─────────────────────────────────────────────────────────────────────

TEST_CASE("Robot: run() calls hw->run()", "[run]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    hw->runCalls = 0;
    robot.run();
    REQUIRE(hw->runCalls == 1);
}

TEST_CASE("Robot: run() increments controlLoops", "[run]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    REQUIRE(robot.controlLoops() == 0);
    robot.run();
    REQUIRE(robot.controlLoops() == 1);
    robot.run();
    REQUIRE(robot.controlLoops() == 2);
}

TEST_CASE("Robot: run() exposes sensor snapshot", "[run]") {
    auto [robot, hw] = makeRobot();
    robot.init();

    hw->odometry.leftTicks  = 42;
    hw->odometry.rightTicks = 7;
    hw->sensors.bumperLeft  = false;
    hw->battery.voltage     = 24.5f;

    robot.run();

    REQUIRE(robot.lastOdometry().leftTicks  == 42);
    REQUIRE(robot.lastOdometry().rightTicks == 7);
    REQUIRE(robot.lastSensors().bumperLeft  == false);
    REQUIRE(robot.lastBattery().voltage == Catch::Approx(24.5f));
}

TEST_CASE("Robot: bumper triggers safety motor stop", "[run]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    robot.startMowing();

    hw->sensors.bumperLeft = true;
    hw->motorCalls.clear();
    robot.run();

    REQUIRE(hw->hadMotorStop());
    REQUIRE(robot.getState() == RobotState::IDLE);
}

TEST_CASE("Robot: lift sensor triggers safety motor stop", "[run]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    robot.startMowing();

    hw->sensors.lift = true;
    hw->motorCalls.clear();
    robot.run();

    REQUIRE(hw->hadMotorStop());
}

TEST_CASE("Robot: motor fault triggers safety stop", "[run]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    robot.startMowing();

    hw->sensors.motorFault = true;
    hw->motorCalls.clear();
    robot.run();

    REQUIRE(hw->hadMotorStop());
}

// ── [state] ───────────────────────────────────────────────────────────────────

TEST_CASE("Robot: initial state is IDLE", "[state]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    REQUIRE(robot.getState() == RobotState::IDLE);
}

TEST_CASE("Robot: startMowing() transitions IDLE->MOWING", "[state]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    robot.startMowing();
    robot.run();
    REQUIRE(robot.getState() == RobotState::MOWING);
}

TEST_CASE("Robot: startDocking() transitions MOWING->DOCKING", "[state]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    robot.startMowing();
    robot.run();  // -> MOWING
    robot.startDocking();
    robot.run();  // -> DOCKING
    REQUIRE(robot.getState() == RobotState::DOCKING);
}

TEST_CASE("Robot: emergencyStop() resets to IDLE", "[state]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    robot.startMowing();
    robot.run();  // -> MOWING
    robot.emergencyStop();
    REQUIRE(robot.getState() == RobotState::IDLE);
}

TEST_CASE("Robot: emergencyStop() sends motor stop", "[state]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    hw->motorCalls.clear();
    robot.emergencyStop();
    REQUIRE(hw->hadMotorStop());
}

// ── [battery] ─────────────────────────────────────────────────────────────────

TEST_CASE("Robot: low battery triggers dock request", "[battery]") {
    auto [robot, hw] = makeRobot();
    robot.init();
    robot.startMowing();
    robot.run();  // -> MOWING

    // Set battery below battery_low_v default (21.0V), above critical (20.0V)
    hw->battery.voltage          = 20.5f;
    hw->battery.chargerConnected = false;
    robot.run();  // triggers dock
    robot.run();  // applies dock transition

    REQUIRE(robot.getState() == RobotState::DOCKING);
}

TEST_CASE("Robot: critical battery stops loop and calls keepPowerOn(false)", "[battery]") {
    auto [robot, hw] = makeRobot();
    robot.init();

    hw->battery.voltage          = 19.5f;  // below critical (20.0V)
    hw->battery.chargerConnected = false;
    robot.run();

    REQUIRE(hw->keepPowerOnFlag == false);
    REQUIRE(robot.isRunning() == false);
    REQUIRE(robot.getState() == RobotState::ERROR);
}

TEST_CASE("Robot: battery voltage==0 (no MCU) does not trigger shutdown", "[battery]") {
    auto [robot, hw] = makeRobot();
    robot.init();

    hw->battery.voltage = 0.0f;  // no MCU — guard: voltage > 0.1f
    robot.run();

    REQUIRE(hw->keepPowerOnCalls == 0);
    REQUIRE(robot.getState() == RobotState::IDLE);
}

TEST_CASE("Robot: charger connected suppresses low battery dock", "[battery]") {
    auto [robot, hw] = makeRobot();
    robot.init();

    hw->battery.voltage          = 20.5f;
    hw->battery.chargerConnected = true;
    robot.run();
    robot.run();

    // Should NOT have docked
    REQUIRE(robot.getState() == RobotState::IDLE);
}

// ── [loop] ────────────────────────────────────────────────────────────────────

TEST_CASE("Robot: stop() causes loop() to exit", "[loop]") {
    auto hw_owned = std::make_unique<MockHardware>();
    MockHardware* hw = hw_owned.get();
    Robot robot(std::move(hw_owned), makeConfig(), makeLogger());
    robot.init();

    std::thread loopThread([&robot]() {
        robot.loop();
    });

    // Give the loop a moment to start, then stop it
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    robot.stop();
    loopThread.join();

    REQUIRE(robot.controlLoops() > 0);      // at least one iteration ran
    REQUIRE(robot.isRunning() == false);
    REQUIRE(hw->keepPowerOnFlag == false);   // shutdown sequence was called
    REQUIRE(hw->hadMotorStop());             // motors were zeroed on exit
}
