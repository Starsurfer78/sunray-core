// test_navigation.cpp — Unit tests for StateEstimator, LineTracker, Map.
//
// No hardware required. LineTracker tests use MockHardware to observe
// motor PWM output (sign of angular correction is read from left vs. right PWM).
//
// Test groups:
//   [state_est]  — StateEstimator: odometry integration, heading, sanity guard, reset
//   [line_track] — LineTracker: Stanley controller angular output, onTargetReached
//   [map]        — Map: JSON load, mow-point sequence, isInsideAllowedArea

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/navigation/StateEstimator.h"
#include "core/navigation/GridMap.h"
#include "core/navigation/LineTracker.h"
#include "core/navigation/Map.h"
#include "core/control/OpenLoopDriveController.h"
#include "core/op/Op.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "hal/HardwareInterface.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace sunray;
using namespace sunray::nav;
using Catch::Approx;

// ── MockHardware ──────────────────────────────────────────────────────────────

struct MockHardware : public HardwareInterface {
    struct MotorCall { int left, right, mow; };
    std::vector<MotorCall> motorCalls;

    bool         init()           override { return true; }
    void         run()            override {}
    void         setMotorPwm(int l, int r, int m) override { motorCalls.push_back({l,r,m}); }
    void         resetMotorFault()  override {}
    OdometryData readOdometry()  override { return {}; }
    SensorData   readSensors()   override { return {}; }
    BatteryData  readBattery()   override { return {}; }
    ImuData      readImu()       override { return {}; }
    void         calibrateImu()  override {}
    void         setBuzzer(bool) override {}
    void         setLed(LedId, LedState) override {}
    void         keepPowerOn(bool) override {}
    float        getCpuTemperature()    override { return 40.0f; }
    std::string  getRobotId()           override { return "nav_test"; }
    std::string  getMcuFirmwareName()   override { return "test"; }
    std::string  getMcuFirmwareVersion()override { return "1.0"; }
};

// ── Helpers ───────────────────────────────────────────────────────────────────

/// Write content to a temp file and return the path.
static std::filesystem::path writeTmpMap(const std::string& json) {
    auto p = std::filesystem::temp_directory_path() / "sunray_nav_test_map.json";
    std::ofstream f(p);
    f << json;
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// StateEstimator Tests
//
// IMPORTANT: StateEstimator consumes the FIRST valid odometry frame (firstUpdate_
// flag). Every test therefore calls update() twice:
//   - First call: frame is consumed (primes the estimator), no integration
//   - Second call: delta is integrated into pose
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("StateEstimator: zero odometry keeps position (0,0) and heading 0", "[state_est]") {
    StateEstimator est;   // nullptr config → uses built-in defaults

    OdometryData odo;
    odo.mcuConnected = true;
    odo.leftTicks    = 0;
    odo.rightTicks   = 0;

    est.update(odo, 20);   // prime (consumed)
    est.update(odo, 20);   // integrate zeros

    REQUIRE(est.x()       == Approx(0.0f).margin(1e-5f));
    REQUIRE(est.y()       == Approx(0.0f).margin(1e-5f));
    REQUIRE(est.heading() == Approx(0.0f).margin(1e-5f));
}

TEST_CASE("StateEstimator: forward drive increases x by ~0.4 m", "[state_est]") {
    // Keep the step below the 0.5 m sanity guard. 78 ticks are about 0.4 m
    // with the no-config defaults used in this test.
    StateEstimator est;

    OdometryData prime;
    prime.mcuConnected = true;
    est.update(prime, 20);   // prime

    OdometryData fwd;
    fwd.mcuConnected = true;
    fwd.leftTicks    = 78;
    fwd.rightTicks   = 78;
    est.update(fwd, 20);

    REQUIRE(est.x()       == Approx(0.4f).margin(0.05f));
    REQUIRE(est.y()       == Approx(0.0f).margin(0.01f));
    REQUIRE(est.heading() == Approx(0.0f).margin(0.01f));
}

TEST_CASE("StateEstimator: in-place rotation ~90 degrees turns heading to pi/2", "[state_est]") {
    // For 90° CCW: rightTicks - leftTicks = (pi/2) * wheelBase * ticksPerMeter
    //   = (pi/2) * 0.285 * 193.94 ≈ 86.8 → 87 ticks difference (leftTicks = 0)
    StateEstimator est;

    OdometryData prime;
    prime.mcuConnected = true;
    est.update(prime, 20);   // prime

    OdometryData rot;
    rot.mcuConnected = true;
    rot.leftTicks    = 0;
    rot.rightTicks   = 87;
    est.update(rot, 20);

    REQUIRE(est.heading() == Approx(static_cast<float>(M_PI) * 0.5f).margin(0.03f));
}

TEST_CASE("StateEstimator: sanity guard discards frame with >0.5 m delta", "[state_est]") {
    // 5000 ticks ≈ 25.8 m >> 0.5 m limit → position must stay at (0,0).
    StateEstimator est;

    OdometryData prime;
    prime.mcuConnected = true;
    est.update(prime, 20);   // prime

    OdometryData huge;
    huge.mcuConnected = true;
    huge.leftTicks    = 5000;
    huge.rightTicks   = 5000;
    est.update(huge, 20);

    REQUIRE(est.x() == Approx(0.0f).margin(1e-5f));
    REQUIRE(est.y() == Approx(0.0f).margin(1e-5f));
}

TEST_CASE("StateEstimator: reset() returns pose to (0,0,0)", "[state_est]") {
    StateEstimator est;
    est.setPose(3.5f, -2.1f, 1.2f);

    REQUIRE(est.x()       == Approx(3.5f));
    REQUIRE(est.y()       == Approx(-2.1f));

    est.reset();

    REQUIRE(est.x()       == Approx(0.0f).margin(1e-5f));
    REQUIRE(est.y()       == Approx(0.0f).margin(1e-5f));
    REQUIRE(est.heading() == Approx(0.0f).margin(1e-5f));
    REQUIRE_FALSE(est.gpsHasFix());
}

// ─────────────────────────────────────────────────────────────────────────────
// LineTracker Tests
//
// Setup: east-bound line from (0,0) to (10,0).
// Lateral error sign convention (Stanley):
//   Robot NORTH (left) of east-bound line  → lateralError < 0 → angular < 0
//     → right wheel faster → robot turns right (CW) → correct ✓
//   Robot SOUTH (right) of east-bound line → lateralError > 0 → angular > 0
//     → left wheel faster → robot turns left (CCW) → correct ✓
//
// We observe angular sign via MockHardware.motorCalls: pwmLeft vs. pwmRight.
//   angular = 0 → pwmLeft == pwmRight
//   angular < 0 → pwmLeft > pwmRight   (turns right)
//   angular > 0 → pwmLeft < pwmRight   (turns left)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LineTracker: reset clears internal state", "[line_track]") {
    LineTracker tracker;
    tracker.reset();

    REQUIRE(tracker.lateralError() == Approx(0.0f).margin(1e-6f));
    REQUIRE(tracker.targetDist() == Approx(0.0f).margin(1e-6f));
    REQUIRE_FALSE(tracker.angleToTargetFits());
}

TEST_CASE("LineTracker: default construction is stable", "[line_track]") {
    LineTracker tracker;
    REQUIRE_NOTHROW(tracker.reset());
}

TEST_CASE("LineTracker: repeated reset remains stable", "[line_track]") {
    LineTracker tracker;
    tracker.reset();
    tracker.reset();

    REQUIRE(tracker.lateralError() == Approx(0.0f).margin(1e-6f));
    REQUIRE(tracker.targetDist() == Approx(0.0f).margin(1e-6f));
}

TEST_CASE("LineTracker: map progression data can be prepared safely", "[line_track]") {
    auto tmpPath = writeTmpMap(
        R"({"perimeter":[[0,0],[10,0],[10,10],[0,10]],"mow":[[1,1],[2,1],[3,1]]})");

    Map          map;

    REQUIRE(map.load(tmpPath));
    std::filesystem::remove(tmpPath);

    REQUIRE(map.startMowing(0.0f, 0.0f));   // nearest to origin → index 0, target=(1,1)
    const int idxBefore = map.mowPointsIdx;  // 0
    REQUIRE(map.nextPoint(true, map.targetPoint.x, map.targetPoint.y));
    REQUIRE(map.mowPointsIdx == idxBefore + 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Map Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("OpenLoopDriveController: zero command maps to zero PWM", "[drive]") {
    Config cfg("/tmp/sunray_drive_controller_test_config.json");
    cfg.set("motor_set_speed_ms", 0.5f);
    cfg.set("wheel_base_m", 0.4f);

    const auto pwm = control::OpenLoopDriveController::compute(cfg, 0.0f, 0.0f);
    REQUIRE(pwm.left == 0);
    REQUIRE(pwm.right == 0);
}

TEST_CASE("OpenLoopDriveController: forward command maps symmetrically", "[drive]") {
    Config cfg("/tmp/sunray_drive_controller_test_config.json");
    cfg.set("motor_set_speed_ms", 0.5f);
    cfg.set("wheel_base_m", 0.4f);

    const auto pwm = control::OpenLoopDriveController::compute(cfg, 0.25f, 0.0f);
    REQUIRE(pwm.left == 127);
    REQUIRE(pwm.right == 127);
}

TEST_CASE("OpenLoopDriveController: positive angular command slows left and speeds right", "[drive]") {
    Config cfg("/tmp/sunray_drive_controller_test_config.json");
    cfg.set("motor_set_speed_ms", 0.5f);
    cfg.set("wheel_base_m", 0.4f);

    const auto pwm = control::OpenLoopDriveController::compute(cfg, 0.2f, 1.0f);
    REQUIRE(pwm.left == 0);
    REQUIRE(pwm.right == 204);
}

TEST_CASE("OpenLoopDriveController: output is clamped to PWM range", "[drive]") {
    Config cfg("/tmp/sunray_drive_controller_test_config.json");
    cfg.set("motor_set_speed_ms", 0.3f);
    cfg.set("wheel_base_m", 0.39f);

    const auto pwm = control::OpenLoopDriveController::compute(cfg, 1.0f, 5.0f);
    REQUIRE(pwm.left >= -255);
    REQUIRE(pwm.left <= 255);
    REQUIRE(pwm.right >= -255);
    REQUIRE(pwm.right <= 255);
}

TEST_CASE("Map: load() with non-existent path returns false", "[map]") {
    Map map;
    REQUIRE_FALSE(map.load("/tmp/sunray_no_such_map_99999.json"));
}

TEST_CASE("Map: load() with valid JSON stores mow points", "[map]") {
    auto tmpPath = writeTmpMap(
        R"({"perimeter":[[0,0],[10,0],[10,10],[0,10]],"mow":[[1,1],[2,1],[3,1]]})");

    Map map;
    REQUIRE(map.load(tmpPath));
    REQUIRE(map.mowPoints().size() == 3);
    REQUIRE(map.mowPoints()[0].p.x == Approx(1.0f));
    REQUIRE(map.mowPoints()[0].p.y == Approx(1.0f));
    REQUIRE(map.mowPoints()[2].p.x == Approx(3.0f));

    std::filesystem::remove(tmpPath);
}

TEST_CASE("Map: load() with inconsistent JSON returns false and clears state", "[map]") {
    auto tmpPath = writeTmpMap(
        R"({"perimeter":[[0,0],[10,0],[10,10],[0,10]],"mow":[{"p":[1]}]})");

    Map map;
    REQUIRE_FALSE(map.load(tmpPath));
    REQUIRE_FALSE(map.isLoaded());
    REQUIRE(map.mowPoints().empty());
    REQUIRE(map.zones().empty());

    std::filesystem::remove(tmpPath);
}

TEST_CASE("Map: startMowing() returns false when no mow route is available", "[map]") {
    auto tmpPath = writeTmpMap(
        R"({"perimeter":[[0,0],[10,0],[10,10],[0,10]],"dock":[[0,0]]})");

    Map map;
    REQUIRE(map.load(tmpPath));
    REQUIRE_FALSE(map.startMowing(0.0f, 0.0f));

    std::filesystem::remove(tmpPath);
}

TEST_CASE("Map: nextPoint() advances through mow points in order", "[map]") {
    auto tmpPath = writeTmpMap(
        R"({"perimeter":[[0,0],[10,0],[10,10],[0,10]],"mow":[[1,1],[2,1],[3,1]]})");

    Map map;
    REQUIRE(map.load(tmpPath));
    REQUIRE(map.startMowing(0.0f, 0.0f));   // nearest to origin = (1,1), index 0
    std::filesystem::remove(tmpPath);

    REQUIRE(map.mowPointsIdx   == 0);
    REQUIRE(map.targetPoint.x  == Approx(1.0f));
    REQUIRE(map.targetPoint.y  == Approx(1.0f));

    // Advance to second point
    REQUIRE(map.nextPoint(true, 1.0f, 1.0f));
    REQUIRE(map.mowPointsIdx   == 1);
    REQUIRE(map.targetPoint.x  == Approx(2.0f));

    // Advance to third point
    REQUIRE(map.nextPoint(true, 2.0f, 1.0f));
    REQUIRE(map.mowPointsIdx   == 2);
    REQUIRE(map.targetPoint.x  == Approx(3.0f));

    // At last point: nextPoint returns false (no further points)
    REQUIRE_FALSE(map.nextPoint(true, 3.0f, 1.0f));
}

TEST_CASE("Map: startMowingZones() filters mow points to requested zone", "[map]") {
    auto tmpPath = writeTmpMap(
        R"({
            "perimeter":[[0,0],[10,0],[10,10],[0,10]],
            "mow":[[1,1],[2,1],[8,8]],
            "zones":[
                {"id":"zone-a","order":1,"polygon":[[0,0],[4,0],[4,4],[0,4]]},
                {"id":"zone-b","order":2,"polygon":[[6,6],[10,6],[10,10],[6,10]]}
            ]
        })");

    Map map;
    REQUIRE(map.load(tmpPath));
    REQUIRE(map.startMowingZones(0.0f, 0.0f, {"zone-b"}));
    REQUIRE(map.mowPoints().size() == 1);
    REQUIRE(map.mowPoints()[0].p.x == Approx(8.0f));
    REQUIRE(map.mowPoints()[0].p.y == Approx(8.0f));
    REQUIRE(map.targetPoint.x == Approx(8.0f));
    REQUIRE(map.targetPoint.y == Approx(8.0f));

    std::filesystem::remove(tmpPath);
}

TEST_CASE("Map: startMowingZones() falls back to all mow points for unknown zone", "[map]") {
    auto tmpPath = writeTmpMap(
        R"({
            "perimeter":[[0,0],[10,0],[10,10],[0,10]],
            "mow":[[1,1],[2,1],[8,8]],
            "zones":[
                {"id":"zone-a","order":1,"polygon":[[0,0],[4,0],[4,4],[0,4]]},
                {"id":"zone-b","order":2,"polygon":[[6,6],[10,6],[10,10],[6,10]]}
            ]
        })");

    Map map;
    REQUIRE(map.load(tmpPath));
    REQUIRE(map.startMowingZones(0.0f, 0.0f, {"does-not-exist"}));
    REQUIRE(map.mowPoints().size() == 3);
    REQUIRE(map.targetPoint.x == Approx(1.0f));
    REQUIRE(map.targetPoint.y == Approx(1.0f));

    std::filesystem::remove(tmpPath);
}

TEST_CASE("Map: load/save preserves capture metadata", "[map]") {
    auto tmpPath = writeTmpMap(
        R"({
            "perimeter":[[0,0],[10,0],[10,10],[0,10]],
            "captureMeta":{
                "perimeter":[
                    {"fix_duration_ms":3000,"sample_variance":0.0012,"mean_accuracy":0.018}
                ]
            }
        })");

    auto savedPath = std::filesystem::temp_directory_path() / "sunray_nav_capture_meta_save.json";

    Map map;
    REQUIRE(map.load(tmpPath));
    REQUIRE(map.captureMeta().is_object());
    REQUIRE(map.captureMeta().contains("perimeter"));
    REQUIRE(map.captureMeta()["perimeter"].size() == 1);
    REQUIRE(map.captureMeta()["perimeter"][0]["fix_duration_ms"] == 3000);

    REQUIRE(map.save(savedPath));

    nlohmann::json saved;
    std::ifstream in(savedPath);
    in >> saved;
    REQUIRE(saved.contains("captureMeta"));
    REQUIRE(saved["captureMeta"]["perimeter"].size() == 1);
    REQUIRE(saved["captureMeta"]["perimeter"][0]["sample_variance"] == Approx(0.0012));
    REQUIRE(saved["captureMeta"]["perimeter"][0]["mean_accuracy"] == Approx(0.018));

    std::filesystem::remove(tmpPath);
    std::filesystem::remove(savedPath);
}

TEST_CASE("Map: isInsideAllowedArea returns true for point inside perimeter", "[map]") {
    auto tmpPath = writeTmpMap(
        R"({"perimeter":[[0,0],[10,0],[10,10],[0,10]]})");

    Map map;
    REQUIRE(map.load(tmpPath));
    REQUIRE(map.isInsideAllowedArea(5.0f, 5.0f));

    std::filesystem::remove(tmpPath);
}

TEST_CASE("Map: isInsideAllowedArea returns false for point outside perimeter", "[map]") {
    auto tmpPath = writeTmpMap(
        R"({"perimeter":[[0,0],[10,0],[10,10],[0,10]]})");

    Map map;
    REQUIRE(map.load(tmpPath));
    REQUIRE_FALSE(map.isInsideAllowedArea(15.0f, 5.0f));
    REQUIRE_FALSE(map.isInsideAllowedArea(-1.0f, 5.0f));

    std::filesystem::remove(tmpPath);
}

// ─────────────────────────────────────────────────────────────────────────────
// On-The-Fly Obstacle Tests (C.7)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Map: addObstacle stores obstacle with metadata", "[obstacle]") {
    Map map;
    REQUIRE(map.addObstacle(3.0f, 4.0f, 1000u));
    const auto& obs = map.obstacles();
    REQUIRE(obs.size() == 1);
    REQUIRE(obs[0].center.x == Approx(3.0f));
    REQUIRE(obs[0].center.y == Approx(4.0f));
    REQUIRE(obs[0].detectedAt_ms == 1000u);
    REQUIRE(obs[0].hitCount == 1);
    REQUIRE_FALSE(obs[0].persistent);
}

TEST_CASE("Map: addObstacle merges nearby hits into same obstacle", "[obstacle]") {
    Map map;
    map.addObstacle(3.0f, 4.0f, 1000u);
    map.addObstacle(3.2f, 4.1f, 2000u);  // within 0.5 m merge radius
    REQUIRE(map.obstacles().size() == 1);
    REQUIRE(map.obstacles()[0].hitCount == 2);
    REQUIRE(map.obstacles()[0].detectedAt_ms == 2000u);  // refreshed
}

TEST_CASE("Map: addObstacle adds separate obstacle when far enough", "[obstacle]") {
    Map map;
    map.addObstacle(3.0f, 4.0f, 1000u);
    map.addObstacle(8.0f, 8.0f, 2000u);  // >0.5 m away
    REQUIRE(map.obstacles().size() == 2);
}

TEST_CASE("Map: clearAutoObstacles removes non-persistent, keeps persistent", "[obstacle]") {
    Map map;
    map.addObstacle(1.0f, 1.0f, 1000u, false);   // auto
    map.addObstacle(2.0f, 2.0f, 1000u, true);    // persistent
    REQUIRE(map.obstacles().size() == 2);
    map.clearAutoObstacles();
    REQUIRE(map.obstacles().size() == 1);
    REQUIRE(map.obstacles()[0].persistent);
}

TEST_CASE("Map: cleanupExpiredObstacles removes old auto obstacles", "[obstacle]") {
    Map map;
    map.addObstacle(1.0f, 1.0f, 0u,       false);   // auto, age = 4000 s
    map.addObstacle(2.0f, 2.0f, 3000000u, false);   // auto, age = 1000 s
    map.addObstacle(3.0f, 3.0f, 0u,       true);    // persistent (never removed)
    // expire after 3600 s (3 600 000 ms)
    map.cleanupExpiredObstacles(4000000u, 3600000u);
    // obstacle at t=0 is 4000 s old → removed; t=3000000 is 1000 s → kept; persistent → kept
    REQUIRE(map.obstacles().size() == 2);
}

TEST_CASE("Map: clearObstacles removes everything", "[obstacle]") {
    Map map;
    map.addObstacle(1.0f, 1.0f, 1000u, false);
    map.addObstacle(2.0f, 2.0f, 1000u, true);
    map.clearObstacles();
    REQUIRE(map.obstacles().empty());
}

TEST_CASE("GridMap: planPath routes around occupied obstacle and keeps exact destination", "[gridmap]") {
    auto tmpPath = writeTmpMap(
        R"({"perimeter":[[-5,-5],[5,-5],[5,5],[-5,5]],"mow":[[-2,0],[2,0]]})");

    Map map;
    REQUIRE(map.load(tmpPath));
    std::filesystem::remove(tmpPath);

    REQUIRE(map.addObstacle(0.0f, 0.0f, 1000u, true));

    GridMap grid;
    grid.build(map, 0.0f, 0.0f);

    const Point src{-2.0f, 0.0f};
    const Point dst{ 2.0f, 0.0f};
    const auto path = grid.planPath(src, dst);

    REQUIRE_FALSE(path.empty());
    REQUIRE(path.back().x == Approx(dst.x).margin(1e-5f));
    REQUIRE(path.back().y == Approx(dst.y).margin(1e-5f));

    bool tookDetour = false;
    for (const auto& p : path) {
        REQUIRE(map.isInsideAllowedArea(p.x, p.y));
        if (std::fabs(p.y) > 0.2f) tookDetour = true;
    }
    REQUIRE(tookDetour);
}

TEST_CASE("GridMap: smoothPath removes visible intermediate waypoints", "[gridmap]") {
    auto tmpPath = writeTmpMap(
        R"({"perimeter":[[-5,-5],[5,-5],[5,5],[-5,5]],"mow":[[-2,-2],[2,2]]})");

    Map map;
    REQUIRE(map.load(tmpPath));
    std::filesystem::remove(tmpPath);

    GridMap grid;
    grid.build(map, 0.0f, 0.0f);

    const std::vector<Point> rawPath{
        {-2.0f, -2.0f},
        {-1.0f, -1.0f},
        { 0.0f,  0.0f},
        { 1.0f,  1.0f},
        { 2.0f,  2.0f},
    };

    const auto smoothed = grid.smoothPath(rawPath);

    REQUIRE(smoothed.size() == 2);
    REQUIRE(smoothed.front().x == Approx(rawPath.front().x).margin(1e-5f));
    REQUIRE(smoothed.front().y == Approx(rawPath.front().y).margin(1e-5f));
    REQUIRE(smoothed.back().x == Approx(rawPath.back().x).margin(1e-5f));
    REQUIRE(smoothed.back().y == Approx(rawPath.back().y).margin(1e-5f));
}

// ─────────────────────────────────────────────────────────────────────────────
// EKF Tests (E.1)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("EKF: predict step accumulates position uncertainty over time", "[ekf]") {
    // After odometry-only operation, position uncertainty must grow because
    // the process noise Q adds to the covariance every step.
    StateEstimator est;   // nullptr config → built-in defaults

    const float unc0 = est.posUncertainty();  // initial ~0.141 m (sqrt(0.01+0.01))

    OdometryData prime;
    prime.mcuConnected = true;
    est.update(prime, 20);  // prime — consumes first frame

    OdometryData fwd;
    fwd.mcuConnected = true;
    fwd.leftTicks    = 39;   // ~0.2 m forward per step
    fwd.rightTicks   = 39;

    for (int i = 0; i < 20; ++i) est.update(fwd, 20);

    // Position must have grown (20 × ~0.2 m)
    REQUIRE(est.x() == Approx(4.0f).margin(0.5f));
    // Uncertainty must be larger than initial (Q accumulates each step)
    REQUIRE(est.posUncertainty() > unc0);
    // Fusion mode: no GPS or IMU → "Odo"
    REQUIRE(est.fusionMode() == "Odo");
}

TEST_CASE("EKF: GPS update pulls position toward measurement", "[ekf]") {
    // Start at origin, drive ~0.2 m east, then inject GPS saying (2, 0).
    // The EKF must move x toward 2 but not jump there immediately.
    StateEstimator est;

    OdometryData prime;
    prime.mcuConnected = true;
    est.update(prime, 20);  // prime

    OdometryData fwd;
    fwd.mcuConnected = true;
    fwd.leftTicks    = 39;
    fwd.rightTicks   = 39;
    est.update(fwd, 20);  // odometry says x ≈ 0.2

    REQUIRE(est.x() == Approx(0.2f).margin(0.05f));

    const float x_before = est.x();
    est.updateGps(2.0f, 0.0f, /*isFix=*/true, /*isFloat=*/false);

    // State must move toward GPS measurement
    REQUIRE(est.x() > x_before);
    REQUIRE(est.x() < 2.0f + 1e-3f);  // Kalman gain < 1 → never overshoots
    REQUIRE(est.gpsHasFix());
    REQUIRE(est.fusionMode() == "EKF+GPS");
}

TEST_CASE("EKF: GPS failover clears fix after timeout", "[ekf]") {
    // After a GPS fix is established, stop delivering fixes and run the
    // predict loop for > 5000 ms (default ekf_gps_failover_ms).
    // gpsHasFix() must become false and fusionMode() must leave EKF+GPS.
    StateEstimator est;

    OdometryData odo;
    odo.mcuConnected = true;
    est.update(odo, 20);  // prime

    // Establish a GPS fix
    est.updateGps(0.0f, 0.0f, true, false);
    REQUIRE(est.gpsHasFix());

    // Run 6000 ms of odometry with no further GPS updates (>5000 ms default)
    for (int i = 0; i < 300; ++i) est.update(odo, 20);

    // Fix must be cleared by failover logic in update()
    REQUIRE_FALSE(est.gpsHasFix());
    REQUIRE(est.fusionMode() != "EKF+GPS");
}
