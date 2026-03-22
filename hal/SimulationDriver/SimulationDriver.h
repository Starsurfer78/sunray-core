#pragma once
// SimulationDriver.h — Software-only HardwareInterface implementation.
//
// Purpose:
//   Enables full Robot + Op state machine testing without any real hardware
//   (no serial port, no I2C, no Raspberry Pi required).
//   Runs on any Linux/macOS/Windows development machine.
//
// Kinematic model:
//   Differential-drive unicycle model.
//   Each run() call advances the simulated pose by dt seconds.
//   Motor PWM (-255…+255) is linearly mapped to wheel velocity (m/s).
//   Wheel velocities → linear + angular velocity → dead-reckoning pose update.
//
// Controllable fault injection (for test scenarios):
//   setBumperLeft(true)  / setBumperRight(true)  — trigger bumper
//   setLift(true)                                — trigger lift sensor
//   setGpsQuality(GpsQuality::FLOAT)             — degrade GPS fix
//   addObstacle(polygon)                         — polygon area triggers bumper
//                                                  when robot enters it
//
// GPS simulation:
//   Robot pose (x, y, heading) is the ground truth.
//   GPS readout reflects the pose with optional noise (configurable).
//   Quality modes: FIX (RTK fixed), FLOAT (RTK float), NO_FIX.
//
// Usage in main.cpp:
//   if (sim_mode) hw = std::make_unique<SimulationDriver>();
//   else          hw = std::make_unique<SerialRobotDriver>(config);

#include "hal/HardwareInterface.h"
#include "core/Config.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sunray {

// ── Geometry helpers ──────────────────────────────────────────────────────────

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

/// Closed polygon — vertices in order, last edge implicitly connects back to [0].
using Polygon = std::vector<Vec2>;

// ── GPS quality modes ─────────────────────────────────────────────────────────

enum class GpsQuality {
    FIX,     ///< RTK fixed  — full accuracy
    FLOAT,   ///< RTK float  — degraded (adds noise, slightly larger position error)
    NO_FIX,  ///< No GPS signal — position frozen at last known
};

// ── Simulated pose ─────────────────────────────────────────────────────────────

struct SimPose {
    double x       = 0.0;  ///< metres, east
    double y       = 0.0;  ///< metres, north
    double heading = 0.0;  ///< radians, 0=north, clockwise positive
};

// ── SimulationDriver ──────────────────────────────────────────────────────────

class SimulationDriver : public HardwareInterface {
public:
    /// Construct with optional config for parameter overrides.
    /// If config is nullptr, uses hard-coded simulation defaults.
    explicit SimulationDriver(std::shared_ptr<Config> config = nullptr);

    ~SimulationDriver() override = default;

    // Non-copyable (holds mutex + atomic state).
    SimulationDriver(const SimulationDriver&)            = delete;
    SimulationDriver& operator=(const SimulationDriver&) = delete;

    // ── HardwareInterface ──────────────────────────────────────────────────────

    bool init() override;
    void run()  override;

    void setMotorPwm(int left, int right, int mow) override;
    void resetMotorFault() override;

    OdometryData readOdometry() override;
    SensorData   readSensors()  override;
    BatteryData  readBattery()  override;

    void setBuzzer(bool on)               override;
    void setLed(LedId id, LedState state) override;
    void keepPowerOn(bool flag)           override;

    float       getCpuTemperature()     override;
    std::string getRobotId()            override;
    std::string getMcuFirmwareName()    override;
    std::string getMcuFirmwareVersion() override;

    // ── Fault injection (thread-safe) ─────────────────────────────────────────

    /// Manually assert or release the left bumper contact.
    void setBumperLeft(bool active);

    /// Manually assert or release the right bumper contact.
    void setBumperRight(bool active);

    /// Manually assert or release the lift sensor.
    void setLift(bool active);

    /// Switch GPS quality. NO_FIX freezes the reported position.
    void setGpsQuality(GpsQuality q);

    /// Add a polygon obstacle. The robot's centre entering this polygon will
    /// assert bumperLeft and bumperRight simultaneously on the next run() call.
    void addObstacle(Polygon poly);

    /// Remove all polygon obstacles.
    void clearObstacles();

    // ── State inspection (for tests / WebUI) ─────────────────────────────────

    SimPose     getPose()       const;
    GpsQuality  getGpsQuality() const;
    bool        isBuzzerOn()    const { return buzzerOn_; }

    /// Override the robot's pose directly (e.g. to set a known start position).
    void setPose(SimPose pose);

private:
    // ── Physics step ──────────────────────────────────────────────────────────

    /// Advance pose by dt seconds given current motor PWM values.
    void stepKinematics(double dt);

    /// Check all polygon obstacles against current pose.
    /// Sets obstacleHit_ if any polygon contains the robot centre.
    void checkObstacles();

    // ── Geometry ──────────────────────────────────────────────────────────────

    /// Ray-casting point-in-polygon test.
    static bool pointInPolygon(const Vec2& p, const Polygon& poly);

    // ── Parameters (from Config or defaults) ─────────────────────────────────

    double wheelBase_m_   = 0.285;  ///< distance between wheel centres (m)
    double wheelRadius_m_ = 0.095;  ///< wheel radius (m) — for tick ↔ vel conversion
    double maxSpeed_ms_   = 0.5;    ///< full PWM 255 → this wheel speed (m/s)
    double ticksPerRev_   = 60.0;   ///< encoder ticks per wheel revolution
    double batteryVoltage_= 28.0f;  ///< simulated battery voltage (V)

    // ── Shared state (guarded by mutex_) ─────────────────────────────────────

    mutable std::mutex mutex_;

    SimPose     pose_;
    GpsQuality  gpsQuality_  = GpsQuality::FIX;

    int motorLeft_  = 0;  ///< last setMotorPwm() left  value
    int motorRight_ = 0;  ///< last setMotorPwm() right value
    int motorMow_   = 0;  ///< last setMotorPwm() mow   value

    // Injected fault state
    bool bumperLeftActive_  = false;
    bool bumperRightActive_ = false;
    bool liftActive_        = false;

    // Obstacle-triggered bumper (set by checkObstacles, cleared when robot exits)
    bool obstacleHit_ = false;

    std::vector<Polygon> obstacles_;

    // ── Odometry accumulation ─────────────────────────────────────────────────

    double accumLeftTicks_  = 0.0;
    double accumRightTicks_ = 0.0;
    double accumMowTicks_   = 0.0;

    // Odometry delta — consumed by readOdometry()
    int pendingLeftTicks_  = 0;
    int pendingRightTicks_ = 0;
    int pendingMowTicks_   = 0;

    // ── Timing ────────────────────────────────────────────────────────────────

    std::chrono::steady_clock::time_point lastRunTime_;
    bool firstRun_ = true;

    // ── Actuator state ────────────────────────────────────────────────────────

    bool buzzerOn_ = false;
    bool keepPowerOn_ = true;

    // ── Config ────────────────────────────────────────────────────────────────

    std::shared_ptr<Config> config_;
};

} // namespace sunray
