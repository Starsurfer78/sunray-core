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
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>
#include <future>

using namespace sunray;

namespace sunray
{
    struct RobotTelemetryAccess
    {
        static WebSocketServer::TelemetryData build(const Robot &robot)
        {
            return robot.buildTelemetry();
        }

        static void advanceTimeMs(Robot &robot, unsigned long ms)
        {
            robot.startTime_ -= std::chrono::milliseconds(ms);
        }

        static void armDiag(Robot &robot, const std::string &motor, float pwm, unsigned duration_ms)
        {
            std::lock_guard<std::mutex> lk(robot.diagMutex_);
            robot.diagReq_ = Robot::DiagReq{};
            robot.diagReq_.motor = motor;
            robot.diagReq_.duration_ms = duration_ms;
            robot.diagReq_.active = true;
            if (motor == "left")
                robot.diagReq_.leftPwm = pwm;
            if (motor == "right")
                robot.diagReq_.rightPwm = pwm;
            if (motor == "mow")
                robot.diagReq_.mowPwm = pwm;
        }
    };
}

// ── MockHardware ───────────────────────────────────────────────────────────────

struct MockHardware : public HardwareInterface
{
    // Configurable return values
    bool initResult = true;
    OdometryData odometry;
    SensorData sensors;
    BatteryData battery;
    ImuData imu;
    std::string robotId = "AA:BB:CC:DD:EE:FF";
    float cpuTemp = 45.0f;
    std::string mcuFwName = "alfred";
    std::string mcuFwVersion = "1.2.3";

    // Call counters / log
    int initCalls = 0;
    int runCalls = 0;
    int resetFaultCalls = 0;

    struct SetMotorCall
    {
        int left, right, mow;
    };
    struct SetLedCall
    {
        LedId id;
        LedState state;
    };
    struct SetBuzzerCall
    {
        bool on;
    };

    std::vector<SetMotorCall> motorCalls;
    std::vector<SetLedCall> ledCalls;
    std::vector<SetBuzzerCall> buzzerCalls;
    bool keepPowerOnFlag = true;
    int keepPowerOnCalls = 0;

    // HardwareInterface implementation
    bool init() override
    {
        ++initCalls;
        return initResult;
    }
    void run() override { ++runCalls; }
    void setMotorPwm(int l, int r, int m) override { motorCalls.push_back({l, r, m}); }
    void resetMotorFault() override { ++resetFaultCalls; }
    OdometryData readOdometry() override { return odometry; }
    SensorData readSensors() override { return sensors; }
    BatteryData readBattery() override { return battery; }
    ImuData readImu() override { return imu; }
    void calibrateImu() override {}
    void setBuzzer(bool on) override { buzzerCalls.push_back({on}); }
    void setLed(LedId id, LedState state) override { ledCalls.push_back({id, state}); }
    void keepPowerOn(bool flag) override
    {
        keepPowerOnFlag = flag;
        ++keepPowerOnCalls;
    }
    float getCpuTemperature() override { return cpuTemp; }
    std::string getRobotId() override { return robotId; }
    std::string getMcuFirmwareName() override { return mcuFwName; }
    std::string getMcuFirmwareVersion() override { return mcuFwVersion; }

    // Helpers for tests
    bool hadMotorStop() const
    {
        for (auto &c : motorCalls)
            if (c.left == 0 && c.right == 0 && c.mow == 0)
                return true;
        return false;
    }
};

// ── Helpers to build a Robot with mock dependencies ────────────────────────────

static std::shared_ptr<Config> makeConfig()
{
    // Use /tmp path — NullLogger does not need a real file.
    // Config ctor falls back to built-in defaults if file missing.
    return std::make_shared<Config>("/tmp/sunray_test_robot_config.json");
}

static std::shared_ptr<Logger> makeLogger()
{
    return std::make_shared<NullLogger>();
}

struct CapturingLogger : public Logger
{
    struct Entry
    {
        LogLevel level;
        std::string module;
        std::string msg;
    };

    std::vector<Entry> entries;

    void log(LogLevel level, const std::string &module, const std::string &msg) override
    {
        entries.push_back({level, module, msg});
    }
};

static std::filesystem::path writeSimpleMap(const std::string &name)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(path);
    f << R"({
  "perimeter": [[-5,-5], [5,-5], [5,5], [-5,5]],
  "mow": [[0,0], [1,0], [1,1]],
  "dock": [[0,-1]],
  "exclusions": []
})";
    return path;
}

static std::filesystem::path writeZoneMap(const std::string &name)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(path);
    f << R"({
    "perimeter": [[0,0], [10,0], [10,10], [0,10]],
    "mow": [[1,1], [2,1], [8,8]],
    "dock": [[0,-1]],
    "exclusions": [],
    "zones": [
        {"id":"zone-a","order":1,"polygon":[[0,0],[4,0],[4,4],[0,4]]},
        {"id":"zone-b","order":2,"polygon":[[6,6],[10,6],[10,10],[6,10]]}
    ]
})";
    return path;
}

/// Builds a Robot and returns a raw pointer to the mock for assertions.
/// hw_raw is set to the mock; the unique_ptr is moved into Robot.
struct RobotFixture
{
    std::unique_ptr<Robot> robot;
    MockHardware *hw = nullptr;
};

static RobotFixture makeRobot(bool initResult = true)
{
    auto hw_owned = std::make_unique<MockHardware>();
    hw_owned->initResult = initResult;
    MockHardware *raw = hw_owned.get();
    return {std::make_unique<Robot>(std::move(hw_owned), makeConfig(), makeLogger()), raw};
}

struct ThrowingHardware : public MockHardware
{
    bool throwOnRun = false;
    bool throwOnReadSensors = false;

    void run() override
    {
        ++runCalls;
        if (throwOnRun)
            throw std::runtime_error("hw run exploded");
    }

    SensorData readSensors() override
    {
        if (throwOnReadSensors)
            throw std::runtime_error("sensor read exploded");
        return sensors;
    }
};

// ── [construction] ─────────────────────────────────────────────────────────────

TEST_CASE("Robot: null hw throws", "[construction]")
{
    REQUIRE_THROWS_AS(
        Robot(nullptr, makeConfig(), makeLogger()),
        std::invalid_argument);
}

TEST_CASE("Robot: null config throws", "[construction]")
{
    auto hw = std::make_unique<MockHardware>();
    REQUIRE_THROWS_AS(
        Robot(std::move(hw), nullptr, makeLogger()),
        std::invalid_argument);
}

TEST_CASE("Robot: null logger throws", "[construction]")
{
    auto hw = std::make_unique<MockHardware>();
    REQUIRE_THROWS_AS(
        Robot(std::move(hw), makeConfig(), nullptr),
        std::invalid_argument);
}

// ── [init] ────────────────────────────────────────────────────────────────────

TEST_CASE("Robot: init() calls hw->init() once", "[init]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    REQUIRE(hw->initCalls == 1);
}

TEST_CASE("Robot: init() returns true when hw succeeds", "[init]")
{
    auto [robot, hw] = makeRobot(true);
    REQUIRE(robot->init() == true);
}

TEST_CASE("Robot: init() returns false when hw fails", "[init]")
{
    auto [robot, hw] = makeRobot(false);
    REQUIRE(robot->init() == false);
}

TEST_CASE("Robot: init() resets all three LEDs to OFF", "[init]")
{
    auto [robot, hw] = makeRobot();
    hw->ledCalls.clear();
    robot->init();

    // Expect at least one OFF call per LED
    bool led1Off = false, led2Off = false, led3Off = false;
    for (auto &c : hw->ledCalls)
    {
        if (c.id == LedId::LED_1 && c.state == LedState::OFF)
            led1Off = true;
        if (c.id == LedId::LED_2 && c.state == LedState::OFF)
            led2Off = true;
        if (c.id == LedId::LED_3 && c.state == LedState::OFF)
            led3Off = true;
    }
    REQUIRE(led1Off);
    REQUIRE(led2Off);
    REQUIRE(led3Off);
}

TEST_CASE("Robot: isRunning() is false before loop()", "[init]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    REQUIRE(robot->isRunning() == false);
}

// ── [run] ─────────────────────────────────────────────────────────────────────

TEST_CASE("Robot: run() calls hw->run()", "[run]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    hw->runCalls = 0;
    robot->run();
    REQUIRE(hw->runCalls == 1);
}

TEST_CASE("Robot: run() increments controlLoops", "[run]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    REQUIRE(robot->controlLoops() == 0);
    robot->run();
    REQUIRE(robot->controlLoops() == 1);
    robot->run();
    REQUIRE(robot->controlLoops() == 2);
}

TEST_CASE("Robot: run() catches hardware exceptions and stops safely", "[run][a3]")
{
    auto hw_owned = std::make_unique<ThrowingHardware>();
    ThrowingHardware *hw = hw_owned.get();
    auto logger = std::make_shared<CapturingLogger>();
    Robot robot(std::move(hw_owned), makeConfig(), logger);

    REQUIRE(robot.init());
    hw->throwOnRun = true;

    REQUIRE_NOTHROW(robot.run());
    REQUIRE_FALSE(robot.isRunning());
    REQUIRE(hw->hadMotorStop());

    bool sawError = false;
    for (const auto &e : logger->entries)
    {
        if (e.level == LogLevel::ERROR &&
            e.msg.find("Unhandled exception in run()") != std::string::npos)
        {
            sawError = true;
            break;
        }
    }
    REQUIRE(sawError);
}

TEST_CASE("Robot: run() catches sensor read exceptions and stops safely", "[run][a3]")
{
    auto hw_owned = std::make_unique<ThrowingHardware>();
    ThrowingHardware *hw = hw_owned.get();
    auto logger = std::make_shared<CapturingLogger>();
    Robot robot(std::move(hw_owned), makeConfig(), logger);

    REQUIRE(robot.init());
    hw->throwOnReadSensors = true;

    REQUIRE_NOTHROW(robot.run());
    REQUIRE_FALSE(robot.isRunning());
    REQUIRE(hw->hadMotorStop());
}

TEST_CASE("Robot: startup without MCU connection does not enter error immediately", "[run][safety]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    hw->odometry.mcuConnected = false;
    robot->run();

    REQUIRE(robot->activeOpName() == "Idle");
}

TEST_CASE("Robot: missing MCU connection blinks system LED red", "[run][safety]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    hw->odometry.mcuConnected = false;
    hw->ledCalls.clear();
    robot->run();

    REQUIRE(hw->ledCalls.size() >= 2);
    REQUIRE(hw->ledCalls[0].id == LedId::LED_2);
    REQUIRE(hw->ledCalls[0].state == LedState::RED);

    RobotTelemetryAccess::advanceTimeMs(*robot, 600);
    hw->ledCalls.clear();
    robot->run();

    REQUIRE(hw->ledCalls.size() >= 2);
    REQUIRE(hw->ledCalls[0].id == LedId::LED_2);
    REQUIRE(hw->ledCalls[0].state == LedState::OFF);
}

TEST_CASE("Robot: run() exposes sensor snapshot", "[run]")
{
    auto [robot, hw] = makeRobot();
    robot->init();

    hw->odometry.leftTicks = 42;
    hw->odometry.rightTicks = 7;
    hw->sensors.bumperLeft = false;
    hw->battery.voltage = 24.5f;

    robot->run();

    REQUIRE(robot->lastOdometry().leftTicks == 42);
    REQUIRE(robot->lastOdometry().rightTicks == 7);
    REQUIRE(robot->lastSensors().bumperLeft == false);
    REQUIRE(robot->lastBattery().voltage == Catch::Approx(24.5f));
}

TEST_CASE("Robot: telemetry smoke test freezes current business semantics", "[run][telemetry]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    SECTION("Idle telemetry uses stable defaults")
    {
        robot->run();

        const auto td = RobotTelemetryAccess::build(*robot);
        REQUIRE(robot->activeOpName() == "Idle");
        REQUIRE(td.op == "Idle");
        REQUIRE(td.runtime_health == "ok");
        REQUIRE(td.mcu_connected == false);
        REQUIRE(td.mcu_comm_loss == false);
        REQUIRE(td.recovery_active == false);
        REQUIRE(td.state_phase == "idle");
        REQUIRE(td.resume_target.empty());
        REQUIRE(td.event_reason == "none");
        REQUIRE(td.error_code.empty());
    }

    SECTION("Start mowing enters NavToStart telemetry state first")
    {
        REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_telemetry_map.json")));

        robot->startMowing();
        robot->run();

        const auto td = RobotTelemetryAccess::build(*robot);
        REQUIRE(robot->activeOpName() == "NavToStart");
        REQUIRE(td.op == "NavToStart");
        REQUIRE(td.runtime_health == "ok");
        REQUIRE(td.recovery_active == false);
        REQUIRE(td.state_phase == "navigating_to_start");
        REQUIRE(td.event_reason == "navigating_to_start");
        REQUIRE(td.error_code.empty());
    }

    SECTION("Error telemetry keeps resume target empty")
    {
        REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_telemetry_error_map.json")));

        robot->startMowing();
        robot->run();
        robot->run();
        REQUIRE(robot->activeOpName() == "Mow");

        hw->sensors.lift = true;
        robot->run();
        robot->run();

        const auto td = RobotTelemetryAccess::build(*robot);
        REQUIRE(robot->activeOpName() == "Error");
        REQUIRE(td.resume_target.empty());
        REQUIRE(td.runtime_health == "fault");
        REQUIRE(td.recovery_active == false);
        REQUIRE(td.state_phase == "fault");
        REQUIRE(td.error_code == "ERR_LIFT");
    }
}

TEST_CASE("Robot: diag early-return skips normal loop completion", "[run][diag]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    hw->odometry.leftTicks = 7;
    RobotTelemetryAccess::armDiag(*robot, "left", 0.25f, 1000);

    REQUIRE(robot->controlLoops() == 0);
    robot->run();

    REQUIRE(robot->controlLoops() == 0);
    REQUIRE_FALSE(hw->hadMotorStop());
    REQUIRE_FALSE(hw->ledCalls.empty());
    const auto td = RobotTelemetryAccess::build(*robot);
    REQUIRE(td.diag_active == true);
    REQUIRE(td.diag_ticks == 7);
}

TEST_CASE("Robot: bumper triggers safety motor stop", "[run]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_bumper_map.json")));
    robot->startMowing();

    hw->sensors.bumperLeft = true;
    hw->motorCalls.clear();
    robot->run();
    robot->run();

    REQUIRE(hw->hadMotorStop());
    REQUIRE(robot->activeOpName() == "EscapeReverse");
}

TEST_CASE("Robot: lift sensor triggers safety motor stop", "[run]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    robot->startMowing();

    hw->sensors.lift = true;
    hw->motorCalls.clear();
    robot->run();

    REQUIRE(hw->hadMotorStop());
}

TEST_CASE("Robot: motor fault triggers safety stop", "[run]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    robot->startMowing();

    hw->sensors.motorFault = true;
    hw->motorCalls.clear();
    robot->run();

    REQUIRE(hw->hadMotorStop());
}

TEST_CASE("Robot: stop button hold logic matches Alfred command thresholds", "[run][button]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_button_map.json")));

    SECTION("button press during mowing immediately triggers emergency stop")
    {
        robot->startMowing();
        robot->run();
        robot->run();
        REQUIRE(robot->activeOpName() == "Mow");

        hw->sensors.stopButton = true;
        robot->run();
        robot->run();

        REQUIRE(robot->activeOpName() == "Idle");
        REQUIRE(hw->hadMotorStop());
    }

    SECTION("5 second hold starts docking on release")
    {
        hw->sensors.stopButton = true;
        robot->run();
        RobotTelemetryAccess::advanceTimeMs(*robot, 5100);
        robot->run();
        hw->sensors.stopButton = false;
        robot->run();
        robot->run();

        REQUIRE(robot->activeOpName() == "Dock");
    }

    SECTION("6 second hold starts mowing on release")
    {
        hw->sensors.stopButton = true;
        robot->run();
        RobotTelemetryAccess::advanceTimeMs(*robot, 6100);
        robot->run();
        hw->sensors.stopButton = false;
        robot->run();
        robot->run();

        REQUIRE(robot->activeOpName() == "NavToStart");
    }

    SECTION("9 second hold requests shutdown on release")
    {
        hw->sensors.stopButton = true;
        robot->run();
        RobotTelemetryAccess::advanceTimeMs(*robot, 9100);
        robot->run();
        hw->sensors.stopButton = false;
        robot->run();

        REQUIRE(hw->keepPowerOnFlag == false);
        REQUIRE(robot->isRunning() == false);
        REQUIRE(hw->hadMotorStop());
    }
}

// ── [state] ───────────────────────────────────────────────────────────────────

TEST_CASE("Robot: initial state is IDLE", "[state]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    REQUIRE(robot->activeOpName() == "Idle");
}

TEST_CASE("Robot: startMowing() transitions IDLE->NAV_TO_START->MOWING", "[state]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_start_map.json")));
    robot->startMowing();
    robot->run();
    REQUIRE(robot->activeOpName() == "NavToStart");
    robot->run();
    REQUIRE(robot->activeOpName() == "Mow");
}

TEST_CASE("Robot: startMowingZones() activates compiled route before NavToStart", "[state]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeZoneMap("sunray_test_robot_zone_start_map.json")));

    robot->startMowingZones({"zone-b"});
    robot->run();

    REQUIRE(robot->activeOpName() == "NavToStart");

    const auto telemetry = RobotTelemetryAccess::build(*robot);
    REQUIRE(telemetry.mission_zone_count == 1);
}

TEST_CASE("Robot: startDocking() transitions MOWING->DOCKING", "[state]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_dock_map.json")));
    robot->startMowing();
    robot->run(); // -> MOWING
    robot->startDocking();
    robot->run(); // -> DOCKING
    REQUIRE(robot->activeOpName() == "Dock");
}

TEST_CASE("Robot: emergencyStop() resets to IDLE", "[state]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_estop_map.json")));
    robot->startMowing();
    robot->run(); // -> MOWING
    robot->emergencyStop();
    robot->run();
    REQUIRE(robot->activeOpName() == "Idle");
}

TEST_CASE("Robot: emergencyStop() sends motor stop", "[state]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    hw->motorCalls.clear();
    robot->emergencyStop();
    REQUIRE(hw->hadMotorStop());
}

TEST_CASE("Robot: diagDriveStraight returns tick and distance metrics", "[diag]")
{
    auto [robot, hw] = makeRobot();
    Robot *robotPtr = robot.get();
    REQUIRE(robot->init());

    hw->odometry.leftTicks = 20;
    hw->odometry.rightTicks = 20;
    hw->odometry.mcuConnected = true;

    std::thread loopThread([robotPtr]()
                           { robotPtr->loop(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto future = std::async(std::launch::async, [robotPtr]()
                             { return robotPtr->diagDriveStraight(0.5f, 0.15f); });

    const auto result = future.get();
    robot->stop();
    loopThread.join();

    REQUIRE(result["ok"].get<bool>() == true);
    REQUIRE(result["left_ticks"].get<long>() > 0);
    REQUIRE(result["right_ticks"].get<long>() > 0);
    REQUIRE(result["left_ticks_target"].get<int>() > 0);
    REQUIRE(result["distance_target_m"].get<float>() == Catch::Approx(0.5f));
}

TEST_CASE("Robot: diagTurnInPlace returns target angle and left/right ticks", "[diag]")
{
    auto [robot, hw] = makeRobot();
    Robot *robotPtr = robot.get();
    REQUIRE(robot->init());

    hw->odometry.leftTicks = 15;
    hw->odometry.rightTicks = 15;
    hw->odometry.mcuConnected = true;

    std::thread loopThread([robotPtr]()
                           { robotPtr->loop(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto future = std::async(std::launch::async, [robotPtr]()
                             { return robotPtr->diagTurnInPlace(90.0f, 0.15f); });

    const auto result = future.get();
    robot->stop();
    loopThread.join();

    REQUIRE(result["ok"].get<bool>() == true);
    REQUIRE(result["left_ticks"].get<long>() > 0);
    REQUIRE(result["right_ticks"].get<long>() > 0);
    REQUIRE(result["target_angle_deg"].get<float>() == Catch::Approx(90.0f));
}

TEST_CASE("Robot: stop button cancels active diagnostics immediately", "[diag][safety]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    RobotTelemetryAccess::armDiag(*robot, "left", 0.25f, 3000);
    hw->motorCalls.clear();
    hw->sensors.stopButton = true;

    robot->run();

    REQUIRE(hw->motorCalls.size() == 1);
    REQUIRE(hw->motorCalls.front().left == 0);
    REQUIRE(hw->motorCalls.front().right == 0);
    REQUIRE(hw->motorCalls.front().mow == 0);

    const auto telemetry = RobotTelemetryAccess::build(*robot);
    REQUIRE(telemetry.diag_active == false);
}

// ── [battery] ─────────────────────────────────────────────────────────────────

TEST_CASE("Robot: low battery triggers dock request", "[battery]")
{
    auto [robot, hw] = makeRobot();
    robot->init();
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_lowbat_map.json")));
    robot->startMowing();
    robot->run(); // -> MOWING

    // Set battery below Alfred low-battery default (25.5V), above critical (18.9V)
    hw->battery.voltage = 25.0f;
    hw->battery.chargerConnected = false;
    robot->run(); // triggers dock
    robot->run(); // applies dock transition

    REQUIRE(robot->activeOpName() == "Dock");
}

TEST_CASE("Robot: critical battery stops loop and calls keepPowerOn(false)", "[battery]")
{
    auto [robot, hw] = makeRobot();
    robot->init();

    hw->battery.voltage = 18.5f; // below critical (18.9V)
    hw->battery.chargerConnected = false;
    robot->run();

    REQUIRE(hw->keepPowerOnFlag == false);
    REQUIRE(robot->isRunning() == false);
    REQUIRE(robot->activeOpName() == "Error");
}

TEST_CASE("Robot: perimeter violation during mowing requests docking", "[run][safety]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_perimeter_mow_map.json")));

    robot->startMowing();
    robot->run();
    robot->run();
    REQUIRE(robot->activeOpName() == "Mow");

    robot->setPose(6.0f, 0.0f, 0.0f);
    hw->motorCalls.clear();
    robot->run();
    robot->run();

    REQUIRE(hw->hadMotorStop());
    REQUIRE(robot->activeOpName() == "Dock");
}

TEST_CASE("Robot: perimeter violation during nav-to-start requests docking", "[run][safety]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_perimeter_nav_map.json")));

    robot->startMowing();
    robot->run();
    REQUIRE(robot->activeOpName() == "NavToStart");

    robot->setPose(6.0f, 0.0f, 0.0f);
    hw->motorCalls.clear();
    robot->run();
    robot->run();

    REQUIRE(hw->hadMotorStop());
    REQUIRE(robot->activeOpName() == "Dock");
}

TEST_CASE("Robot: MCU comm loss during mowing transitions to error and stops motors", "[run][safety]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_mcu_comm_loss_map.json")));

    hw->odometry.mcuConnected = true;
    robot->run();

    robot->startMowing();
    robot->run();
    robot->run();
    REQUIRE(robot->activeOpName() == "Mow");

    hw->odometry.mcuConnected = false;
    hw->motorCalls.clear();
    robot->run();

    REQUIRE(hw->hadMotorStop());
    REQUIRE(robot->activeOpName() == "Error");

    const auto telemetry = RobotTelemetryAccess::build(*robot);
    REQUIRE(telemetry.error_code == "ERR_MCU_COMMS");
    REQUIRE(telemetry.runtime_health == "fault");
    REQUIRE(telemetry.mcu_connected == false);
    REQUIRE(telemetry.mcu_comm_loss == true);
}

TEST_CASE("Robot: STM flash maintenance suppresses transient MCU comm-loss fault", "[run][safety]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_robot_stm_flash_grace_map.json")));

    hw->odometry.mcuConnected = true;
    robot->run();

    robot->startMowing();
    robot->run();
    robot->run();
    REQUIRE(robot->activeOpName() == "Mow");

    robot->setStmFlashMaintenance(true);
    hw->odometry.mcuConnected = false;
    hw->motorCalls.clear();
    robot->run();

    REQUIRE_FALSE(hw->hadMotorStop());
    REQUIRE(robot->activeOpName() == "Mow");

    robot->setStmFlashMaintenance(false, 0);
    robot->run();

    REQUIRE(hw->hadMotorStop());
    REQUIRE(robot->activeOpName() == "Error");
}

TEST_CASE("Robot: stuck detection during mowing transitions to EscapeReverse", "[run][safety]")
{
    auto hw_owned = std::make_unique<MockHardware>();
    MockHardware *hw = hw_owned.get();
    auto config = makeConfig();
    config->set("stuck_detect_timeout_ms", 1);
    config->set("stuck_detect_min_speed_ms", 0.03);
    Robot robot(std::move(hw_owned), config, makeLogger());

    REQUIRE(robot.init());
    REQUIRE(robot.loadMap(writeSimpleMap("sunray_test_robot_stuck_mow_map.json")));

    hw->odometry.mcuConnected = true;
    robot.startMowing();
    robot.run();
    robot.run();
    REQUIRE(robot.activeOpName() == "Mow");

    robot.run();
    RobotTelemetryAccess::advanceTimeMs(robot, 5);
    robot.run();
    REQUIRE(robot.activeOpName() == "Mow");
    RobotTelemetryAccess::advanceTimeMs(robot, 5);
    robot.run();

    REQUIRE(robot.activeOpName() == "EscapeReverse");
}

TEST_CASE("Robot: repeated stuck recovery exhaustion escalates to Error", "[run][safety]")
{
    auto hw_owned = std::make_unique<MockHardware>();
    MockHardware *hw = hw_owned.get();
    auto config = makeConfig();
    config->set("stuck_detect_timeout_ms", 1);
    config->set("stuck_detect_min_speed_ms", 0.03);
    config->set("stuck_recovery_max_attempts", 2);
    Robot robot(std::move(hw_owned), config, makeLogger());

    REQUIRE(robot.init());
    REQUIRE(robot.loadMap(writeSimpleMap("sunray_test_robot_stuck_exhaustion_map.json")));

    hw->odometry.mcuConnected = true;
    robot.startMowing();
    robot.run();
    robot.run();
    REQUIRE(robot.activeOpName() == "Mow");

    robot.run();
    RobotTelemetryAccess::advanceTimeMs(robot, 5);
    robot.run();
    RobotTelemetryAccess::advanceTimeMs(robot, 5);
    robot.run();
    REQUIRE(robot.activeOpName() == "EscapeReverse");

    RobotTelemetryAccess::advanceTimeMs(robot, 4000);
    robot.run();
    robot.run();
    REQUIRE(robot.activeOpName() == "Mow");

    robot.run();
    RobotTelemetryAccess::advanceTimeMs(robot, 5);
    robot.run();
    RobotTelemetryAccess::advanceTimeMs(robot, 5);
    robot.run();
    robot.run();

    REQUIRE(robot.activeOpName() == "Error");
    const auto telemetry = RobotTelemetryAccess::build(robot);
    REQUIRE(telemetry.error_code == "ERR_STUCK");
    REQUIRE(telemetry.event_reason == "stuck_recovery_exhausted");
}

TEST_CASE("Robot: dock watchdog escalates to error", "[run][safety]")
{
    auto hw_owned = std::make_unique<MockHardware>();
    MockHardware *hw = hw_owned.get();
    auto config = makeConfig();
    config->set("dock_max_duration_ms", 50);
    Robot robot(std::move(hw_owned), config, makeLogger());

    REQUIRE(robot.init());
    REQUIRE(robot.loadMap(writeSimpleMap("sunray_test_robot_watchdog_map.json")));

    robot.startDocking();
    robot.run();
    REQUIRE(robot.activeOpName() == "Dock");

    RobotTelemetryAccess::advanceTimeMs(robot, 100);
    hw->motorCalls.clear();
    robot.run();
    robot.run();

    REQUIRE(hw->hadMotorStop());
    REQUIRE(robot.activeOpName() == "Error");
}

TEST_CASE("Robot: battery voltage==0 (no MCU) does not trigger shutdown", "[battery]")
{
    auto [robot, hw] = makeRobot();
    robot->init();

    hw->battery.voltage = 0.0f; // no MCU — guard: voltage > 0.1f
    robot->run();

    REQUIRE(hw->keepPowerOnCalls == 0);
    REQUIRE(robot->activeOpName() == "Idle");
}

TEST_CASE("Robot: loadMap() with invalid JSON returns false without throwing", "[a3]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    const auto path = std::filesystem::temp_directory_path() / "sunray_test_invalid_map.json";
    std::ofstream f(path);
    f << R"({"perimeter":[[0,0]],"mow":[{"p":[1]}]})";
    f.close();

    bool ok = true;
    REQUIRE_NOTHROW(ok = robot->loadMap(path));
    REQUIRE_FALSE(ok);

    std::filesystem::remove(path);
}

TEST_CASE("Robot: loadSchedule() with invalid JSON returns false without throwing", "[a3]")
{
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    const auto path = std::filesystem::temp_directory_path() / "sunray_test_invalid_schedule.json";
    std::ofstream f(path);
    f << R"({"not":"an array"})";
    f.close();

    bool ok = true;
    REQUIRE_NOTHROW(ok = robot->loadSchedule(path));
    REQUIRE_FALSE(ok);

    std::filesystem::remove(path);
}

TEST_CASE("Robot: charger connected suppresses low battery dock", "[battery]")
{
    auto [robot, hw] = makeRobot();
    robot->init();

    hw->battery.voltage = 20.5f;
    hw->battery.chargerConnected = true;
    robot->run();
    robot->run();

    // Should NOT have docked
    REQUIRE(robot->activeOpName() == "Idle");
}

// ── [loop] ────────────────────────────────────────────────────────────────────

TEST_CASE("Robot: stop() causes loop() to exit", "[loop]")
{
    auto hw_owned = std::make_unique<MockHardware>();
    MockHardware *hw = hw_owned.get();
    Robot robot(std::move(hw_owned), makeConfig(), makeLogger());
    robot.init();

    std::thread loopThread([&robot]()
                           { robot.loop(); });

    // Give the loop a moment to start, then stop it
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    robot.stop();
    loopThread.join();

    REQUIRE(robot.controlLoops() > 0); // at least one iteration ran
    REQUIRE(robot.isRunning() == false);
    REQUIRE(hw->keepPowerOnFlag == false); // shutdown sequence was called
    REQUIRE(hw->hadMotorStop());           // motors were zeroed on exit
}

TEST_CASE("Robot: loop shutdown leaves system LED red during power-off grace", "[loop]")
{
    auto hw_owned = std::make_unique<MockHardware>();
    MockHardware *hw = hw_owned.get();
    Robot robot(std::move(hw_owned), makeConfig(), makeLogger());
    robot.init();

    std::thread loopThread([&robot]()
                           { robot.loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    robot.stop();
    loopThread.join();

    bool sawSystemRed = false;
    for (const auto &call : hw->ledCalls)
    {
        if (call.id == LedId::LED_2 && call.state == LedState::RED)
        {
            sawSystemRed = true;
            break;
        }
    }

    REQUIRE(sawSystemRed);
}
