// test_op_machine.cpp — A4 state-machine safety invariants.
//
// Focus:
//   [a4_invariants] ensure safe behavior independent of mission logic details:
//   - Error state keeps all motors at zero.
//   - Emergency stop returns to Idle with mower off.
//   - Low battery leaves Mow and transitions toward Dock safely.
//   [a8_sim] deterministic scenario tests (fault/event handling).

#include <catch2/catch_test_macros.hpp>

#include "core/Robot.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "core/navigation/Map.h"
#include "core/op/Op.h"
#include "hal/GpsDriver/GpsDriver.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace sunray;

namespace sunray {
struct RobotTelemetryAccess {
    static WebSocketServer::TelemetryData build(const Robot& robot) {
        return robot.buildTelemetry();
    }
};
}

namespace {

struct MockHardware : public HardwareInterface {
    OdometryData odometry;
    SensorData sensors;
    BatteryData battery;
    ImuData imu;

    struct MotorCall { int left; int right; int mow; };
    std::vector<MotorCall> motorCalls;

    bool init() override { return true; }
    void run() override {}
    void setMotorPwm(int left, int right, int mow) override {
        motorCalls.push_back({left, right, mow});
    }
    void resetMotorFault() override {}
    OdometryData readOdometry() override { return odometry; }
    SensorData readSensors() override { return sensors; }
    BatteryData readBattery() override { return battery; }
    ImuData readImu() override { return imu; }
    void calibrateImu() override {}
    void setBuzzer(bool) override {}
    void setLed(LedId, LedState) override {}
    void keepPowerOn(bool) override {}
    float getCpuTemperature() override { return 40.0f; }
    std::string getRobotId() override { return "op_test"; }
    std::string getMcuFirmwareName() override { return "mock"; }
    std::string getMcuFirmwareVersion() override { return "1.0"; }
};

class MockGpsDriver : public GpsDriver {
public:
    bool init() override { return true; }
    GpsData getData() const override { return data; }
    GpsData data;
};

static std::shared_ptr<Config> makeConfig() {
    return std::make_shared<Config>("/tmp/sunray_test_op_config.json");
}

static std::shared_ptr<Logger> makeLogger() {
    return std::make_shared<NullLogger>();
}

struct RobotFixture {
    std::unique_ptr<Robot> robot;
    MockHardware*          hw = nullptr;
};

static RobotFixture makeRobot() {
    auto hwOwned = std::make_unique<MockHardware>();
    MockHardware* raw = hwOwned.get();
    return {std::make_unique<Robot>(std::move(hwOwned), makeConfig(), makeLogger()), raw};
}

static std::filesystem::path writeSimpleMap(const std::string& name) {
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

static std::filesystem::path writeDockPathMap(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(path);
    f << R"({
  "perimeter": [[-5,-5], [5,-5], [5,5], [-5,5]],
  "mow": [[0,0], [1,0], [1,1]],
  "dock": [[0,-1], [0,-2]],
  "exclusions": []
})";
    return path;
}

static bool hasMotorStop(const std::vector<MockHardware::MotorCall>& calls) {
    for (const auto& c : calls) {
        if (c.left == 0 && c.right == 0 && c.mow == 0) return true;
    }
    return false;
}

static bool hasMowOn(const std::vector<MockHardware::MotorCall>& calls) {
    for (const auto& c : calls) {
        if (c.mow > 0) return true;
    }
    return false;
}

static void enterMowFromStartCommand(Robot& robot) {
    robot.startMowing();
    robot.run(); // Idle -> NavToStart
    REQUIRE(robot.activeOpName() == "NavToStart");
    robot.run(); // NavToStart -> Mow
    REQUIRE(robot.activeOpName() == "Mow");
}

} // namespace

TEST_CASE("A4: Error state keeps motors at zero", "[a4_invariants]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    const auto mapPath = writeSimpleMap("sunray_test_a4_error_map.json");
    REQUIRE(robot->loadMap(mapPath));

    enterMowFromStartCommand(*robot);

    hw->motorCalls.clear();
    hw->sensors.lift = true; // edge event: Mow::onLiftTriggered -> Error pending
    robot->run();
    robot->run(); // flush pending transition to Error
    REQUIRE(robot->activeOpName() == "Error");
    REQUIRE(hasMotorStop(hw->motorCalls));

    hw->motorCalls.clear();
    robot->run(); // Error::run should keep motors stopped every cycle
    REQUIRE(robot->activeOpName() == "Error");
    REQUIRE_FALSE(hasMowOn(hw->motorCalls));
    REQUIRE(hasMotorStop(hw->motorCalls));
}

TEST_CASE("A4: Emergency stop returns to Idle with mower off", "[a4_invariants]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    const auto mapPath = writeSimpleMap("sunray_test_a4_estop_map.json");
    REQUIRE(robot->loadMap(mapPath));

    enterMowFromStartCommand(*robot);

    hw->motorCalls.clear();
    robot->emergencyStop();
    robot->run();

    REQUIRE(robot->activeOpName() == "Idle");
    REQUIRE(hasMotorStop(hw->motorCalls));
    REQUIRE_FALSE(hasMowOn(hw->motorCalls));
}

TEST_CASE("A4: Low battery transitions Mow toward Dock safely", "[a4_invariants]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());

    const auto mapPath = writeSimpleMap("sunray_test_a4_battery_map.json");
    REQUIRE(robot->loadMap(mapPath));

    enterMowFromStartCommand(*robot);

    hw->motorCalls.clear();
    hw->battery.voltage = 20.5f; // below battery_low_v(21), above critical(20)
    hw->battery.chargerConnected = false;
    robot->run(); // low battery event, transition request
    robot->run(); // state settles to Dock

    REQUIRE(robot->activeOpName() == "Dock");
    REQUIRE(hasMotorStop(hw->motorCalls)); // Mow end / safety behavior
}

TEST_CASE("A5: startup without valid GPS transitions to GpsWait and recovers to Mow", "[a5_gps]") {
    auto cfg = makeConfig();
    cfg->set("gps_no_signal_ms", 1);
    cfg->set("gps_fix_timeout_ms", 1000);
    cfg->set("gps_recover_hysteresis_ms", 1);
    cfg->set("ekf_gps_failover_ms", 1);

    auto hwOwned = std::make_unique<MockHardware>();
    MockHardware* hw = hwOwned.get();
    Robot robot(std::move(hwOwned), cfg, makeLogger());
    REQUIRE(robot.init());

    const auto mapPath = writeSimpleMap("sunray_test_a5_gps_map.json");
    REQUIRE(robot.loadMap(mapPath));

    auto gps = std::make_unique<MockGpsDriver>();
    MockGpsDriver* gpsRaw = gps.get();
    gpsRaw->data.valid = false; // start without any GPS solution
    robot.setGpsDriver(std::move(gps));

    robot.startMowing();
    robot.run();
    REQUIRE(robot.activeOpName() == "NavToStart");

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    for (int i = 0; i < 5 && robot.activeOpName() != "GpsWait"; ++i) {
        robot.run();
    }
    REQUIRE(robot.activeOpName() == "GpsWait");

    // Recover with RTK Float: should continue previous op (Mow).
    gpsRaw->data.valid = true;
    gpsRaw->data.solution = GpsSolution::Float;
    gpsRaw->data.relPosE = 0.0f;
    gpsRaw->data.relPosN = 0.0f;

    for (int i = 0; i < 8 && robot.activeOpName() == "GpsWait"; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        robot.run();
    }
    REQUIRE(robot.activeOpName() == "NavToStart");

    (void)hw;
}

TEST_CASE("A5: prolonged GPS loss during Mow escalates to Dock", "[a5_gps]") {
    auto cfg = makeConfig();
    cfg->set("gps_no_signal_ms", 1);
    cfg->set("gps_fix_timeout_ms", 5);
    cfg->set("gps_recover_hysteresis_ms", 1);
    cfg->set("ekf_gps_failover_ms", 1);

    auto hwOwned = std::make_unique<MockHardware>();
    MockHardware* hw = hwOwned.get();
    Robot robot(std::move(hwOwned), cfg, makeLogger());
    REQUIRE(robot.init());

    const auto mapPath = writeSimpleMap("sunray_test_a5_timeout_map.json");
    REQUIRE(robot.loadMap(mapPath));

    auto gps = std::make_unique<MockGpsDriver>();
    MockGpsDriver* gpsRaw = gps.get();
    gpsRaw->data.valid = true;
    gpsRaw->data.solution = GpsSolution::Fixed;
    robot.setGpsDriver(std::move(gps));

    enterMowFromStartCommand(robot);

    gpsRaw->data.valid = false;

    for (int i = 0; i < 10 && robot.activeOpName() != "Dock"; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        robot.run();
    }
    REQUIRE(robot.activeOpName() == "Dock");

    (void)hw;
}

TEST_CASE("A5: GpsWait telemetry exposes resume target for mission recovery", "[a5_gps][telemetry]") {
    auto cfg = makeConfig();
    cfg->set("gps_no_signal_ms", 1);
    cfg->set("gps_fix_timeout_ms", 1000);
    cfg->set("gps_recover_hysteresis_ms", 1);
    cfg->set("ekf_gps_failover_ms", 1);

    auto hwOwned = std::make_unique<MockHardware>();
    Robot robot(std::move(hwOwned), cfg, makeLogger());
    REQUIRE(robot.init());

    const auto mapPath = writeSimpleMap("sunray_test_a5_resume_target_map.json");
    REQUIRE(robot.loadMap(mapPath));

    auto gps = std::make_unique<MockGpsDriver>();
    MockGpsDriver* gpsRaw = gps.get();
    gpsRaw->data.valid = true;
    gpsRaw->data.solution = GpsSolution::Fixed;
    robot.setGpsDriver(std::move(gps));

    enterMowFromStartCommand(robot);

    gpsRaw->data.valid = false;
    for (int i = 0; i < 10 && robot.activeOpName() != "GpsWait"; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        robot.run();
    }
    REQUIRE(robot.activeOpName() == "GpsWait");

    const auto td = RobotTelemetryAccess::build(robot);
    REQUIRE(td.resume_target == "Mow");
    REQUIRE(td.state_phase == "gps_recovery");
    REQUIRE(td.event_reason == "gps_signal_lost");
}

TEST_CASE("A8 Scenario 1: bumper hit in Mow transitions to EscapeReverse", "[a8_sim]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_a8_bumper_map.json")));

    enterMowFromStartCommand(*robot);

    hw->sensors.bumperLeft = true;
    robot->run(); // queue transition
    robot->run(); // apply transition
    REQUIRE(robot->activeOpName() == "EscapeReverse");
}

TEST_CASE("A8 Scenario 1b: EscapeReverse resumes back to Mow after timeout", "[a8_sim]") {
    auto cfg = makeConfig();
    MockHardware hw;
    NullLogger logger;
    OpManager opMgr;
    OpContext ctx{hw, *cfg, logger, opMgr};

    nav::Map map(cfg);
    REQUIRE(map.load(writeSimpleMap("sunray_test_a8_escape_resume_map.json")));
    ctx.map = &map;
    ctx.x = 0.0f;
    ctx.y = 0.0f;
    ctx.insidePerimeter = true;
    ctx.now_ms = 0;

    opMgr.idle().changeOp(ctx, opMgr.mow());
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Mow");

    ctx.sensors.bumperLeft = true;
    opMgr.mow().onObstacle(ctx);
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "EscapeReverse");

    ctx.sensors.bumperLeft = false;
    ctx.sensors.lift = false;
    ctx.insidePerimeter = true;
    ctx.now_ms = 3001;
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "EscapeReverse");

    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Mow");
}

TEST_CASE("A8 Scenario 2: lift event in Mow transitions to Error", "[a8_sim]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_a8_lift_map.json")));

    enterMowFromStartCommand(*robot);

    hw->sensors.lift = true;
    robot->run(); // queue transition
    robot->run(); // apply transition
    REQUIRE(robot->activeOpName() == "Error");
}

TEST_CASE("A8 Scenario 3: motor fault in Mow transitions to Error", "[a8_sim]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_a8_motorfault_map.json")));

    enterMowFromStartCommand(*robot);

    hw->sensors.motorFault = true;
    robot->run(); // queue transition
    robot->run(); // apply transition
    REQUIRE(robot->activeOpName() == "Error");
}

TEST_CASE("A8 Scenario 4: charger flapping from Dock remains stable in Charge", "[a8_sim]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_a8_charger_map.json")));

    robot->startDocking();
    robot->run(); // enter Dock
    REQUIRE(robot->activeOpName() == "Dock");

    // Charger appears: Dock -> Charge.
    hw->battery.chargerConnected = true;
    robot->run(); // queue transition
    robot->run(); // apply transition
    REQUIRE(robot->activeOpName() == "Charge");

    // Charger drops immediately (flap): Charge has 3s grace and should stay Charge.
    hw->battery.chargerConnected = false;
    robot->run();
    REQUIRE(robot->activeOpName() == "Charge");
}

TEST_CASE("C15: start from Charge transitions through Undock and NavToStart to Mow", "[c15_startpath]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_c15_startpath_map.json")));

    robot->startDocking();
    robot->run();
    REQUIRE(robot->activeOpName() == "Dock");

    hw->battery.chargerConnected = true;
    robot->run();
    robot->run();
    REQUIRE(robot->activeOpName() == "Charge");

    hw->battery.voltage = 27.0f;
    robot->startMowing();
    robot->run();
    REQUIRE(robot->activeOpName() == "Undock");

    hw->battery.chargerConnected = false;
    hw->odometry.mcuConnected = true;
    for (int i = 0; i < 12 && robot->activeOpName() == "Undock"; ++i) {
        hw->odometry.leftTicks = -40;
        hw->odometry.rightTicks = -40;
        robot->run();
    }
    if (robot->activeOpName() == "Undock") {
        robot->run(); // flush pending Undock -> NavToStart transition
    }
    REQUIRE(robot->activeOpName() == "NavToStart");

    for (int i = 0; i < 5 && robot->activeOpName() != "Mow"; ++i) {
        hw->odometry.leftTicks = 40;
        hw->odometry.rightTicks = 40;
        robot->run();
    }
    REQUIRE(robot->activeOpName() == "Mow");
}

TEST_CASE("A8 Scenario 5: low battery in Mow transitions to Dock", "[a8_sim]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_a8_lowbat_map.json")));

    enterMowFromStartCommand(*robot);

    hw->battery.voltage = 20.5f; // low, but not critical
    hw->battery.chargerConnected = false;
    robot->run(); // queue transition
    robot->run(); // apply transition
    REQUIRE(robot->activeOpName() == "Dock");
}

TEST_CASE("A8 Scenario 6: critical battery forces Error and loop stop", "[a8_sim]") {
    auto [robot, hw] = makeRobot();
    REQUIRE(robot->init());
    REQUIRE(robot->loadMap(writeSimpleMap("sunray_test_a8_critbat_map.json")));

    enterMowFromStartCommand(*robot);

    hw->battery.voltage = 18.5f; // below critical
    hw->battery.chargerConnected = false;
    robot->run();
    robot->run();

    REQUIRE(robot->activeOpName() == "Idle");
    REQUIRE_FALSE(robot->isRunning());
}

TEST_CASE("E2x: Dock retries docking route when final contact is missing", "[e2x_dock]") {
    auto cfg = makeConfig();
    cfg->set("dock_retry_max_attempts", 2);

    MockHardware hw;
    NullLogger logger;
    OpManager opMgr;
    OpContext ctx{hw, *cfg, logger, opMgr};

    nav::Map map(cfg);
    REQUIRE(map.load(writeDockPathMap("sunray_test_e2x_dock_retry_map.json")));
    ctx.map = &map;
    ctx.x = 1.0f;
    ctx.y = 0.0f;

    opMgr.changeOperationTypeByOperator(ctx, "Dock");
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Dock");

    opMgr.dock().onNoFurtherWaypoints(ctx);

    REQUIRE(opMgr.dock().mapRoutingFailedCounter == 1);
    REQUIRE(map.isDocking());
    REQUIRE_FALSE(opMgr.dock().shouldStop);
}

TEST_CASE("E2x: Dock enters Error after configured retry budget is exhausted", "[e2x_dock]") {
    auto cfg = makeConfig();
    cfg->set("dock_retry_max_attempts", 1);

    MockHardware hw;
    NullLogger logger;
    OpManager opMgr;
    OpContext ctx{hw, *cfg, logger, opMgr};

    nav::Map map(cfg);
    REQUIRE(map.load(writeDockPathMap("sunray_test_e2x_dock_error_map.json")));
    ctx.map = &map;
    ctx.x = 1.0f;
    ctx.y = 0.0f;

    opMgr.changeOperationTypeByOperator(ctx, "Dock");
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Dock");

    opMgr.dock().onNoFurtherWaypoints(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Dock");

    opMgr.dock().onNoFurtherWaypoints(ctx);
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Error");
}

TEST_CASE("E2x: Charge retries weak contact before redocking", "[e2x_charge]") {
    auto cfg = makeConfig();
    cfg->set("dock_retry_approach_ms", 50);
    cfg->set("dock_retry_contact_timeout_ms", 3000);

    MockHardware hw;
    NullLogger logger;
    OpManager opMgr;
    OpContext ctx{hw, *cfg, logger, opMgr};

    nav::Map map(cfg);
    REQUIRE(map.load(writeDockPathMap("sunray_test_e2x_charge_retry_map.json")));
    ctx.map = &map;
    ctx.battery.chargerConnected = true;

    opMgr.changeOperationTypeByOperator(ctx, "Charge");
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Charge");

    ctx.battery.chargerConnected = false;
    ctx.now_ms = 1000;
    opMgr.tick(ctx);
    REQUIRE(opMgr.charge().retryTouchDock);

    ctx.now_ms = opMgr.charge().retryTime_ms + 1;
    opMgr.tick(ctx);
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Dock");
}

TEST_CASE("E2x: Charge escalates to Error after repeated contact failures", "[e2x_charge]") {
    auto cfg = makeConfig();
    cfg->set("dock_retry_max_attempts", 1);

    MockHardware hw;
    NullLogger logger;
    OpManager opMgr;
    OpContext ctx{hw, *cfg, logger, opMgr};

    opMgr.changeOperationTypeByOperator(ctx, "Charge");
    ctx.battery.chargerConnected = true;
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Charge");

    opMgr.charge().contactRetryCount = 1;
    opMgr.charge().retryTouchDock = false;
    ctx.battery.chargerConnected = false;
    opMgr.charge().onBadChargingContact(ctx);
    opMgr.tick(ctx);
    REQUIRE(opMgr.activeOp()->name() == "Error");
}
