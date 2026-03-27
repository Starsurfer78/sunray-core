// SimulationDriver.cpp — Software-only HardwareInterface implementation.
//
// Kinematic model (differential drive, unicycle approximation):
//
//   v_left  = pwmLeft  / 255.0 * maxSpeed_ms_
//   v_right = pwmRight / 255.0 * maxSpeed_ms_
//
//   v     = (v_left + v_right) / 2.0         — linear velocity (m/s)
//   omega = (v_right - v_left) / wheelBase_m_ — angular velocity (rad/s)
//
//   x'       = x       + v * cos(heading) * dt
//   y'       = y       + v * sin(heading) * dt
//   heading' = heading + omega * dt
//
// Encoder ticks are derived from wheel arc length:
//   ticks = (v * dt / (2π * wheelRadius_m_)) * ticksPerRev_
//
// Thread safety:
//   All mutable shared state is protected by mutex_.
//   Public fault-injection methods (setBumperLeft etc.) acquire the lock.
//   run() acquires the lock for the physics step and sensor snapshot update.

#include "hal/SimulationDriver/SimulationDriver.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sunray {

// ── Construction / init ────────────────────────────────────────────────────────

SimulationDriver::SimulationDriver(std::shared_ptr<Config> config)
    : config_(std::move(config))
{
    if (config_) {
        wheelBase_m_    = config_->get<double>("sim_wheel_base_m",    wheelBase_m_);
        wheelRadius_m_  = config_->get<double>("sim_wheel_radius_m",  wheelRadius_m_);
        maxSpeed_ms_    = config_->get<double>("sim_max_speed_ms",    maxSpeed_ms_);
        ticksPerRev_    = config_->get<double>("sim_ticks_per_rev",   ticksPerRev_);
        batteryVoltage_ = config_->get<double>("sim_battery_voltage", batteryVoltage_);
    }
}

bool SimulationDriver::init() {
    std::lock_guard<std::mutex> lk(mutex_);
    pose_            = SimPose{};
    gpsQuality_      = GpsQuality::FIX;
    motorLeft_       = 0;
    motorRight_      = 0;
    motorMow_        = 0;
    bumperLeftActive_  = false;
    bumperRightActive_ = false;
    liftActive_      = false;
    obstacleHit_     = false;
    obstacles_.clear();
    accumLeftTicks_  = 0.0;
    accumRightTicks_ = 0.0;
    accumMowTicks_   = 0.0;
    pendingLeftTicks_  = 0;
    pendingRightTicks_ = 0;
    pendingMowTicks_   = 0;
    buzzerOn_        = false;
    keepPowerOn_     = true;
    firstRun_        = true;
    return true;
}

// ── run() — physics tick ───────────────────────────────────────────────────────

void SimulationDriver::run() {
    std::lock_guard<std::mutex> lk(mutex_);

    auto now = std::chrono::steady_clock::now();
    double dt = 0.0;
    if (firstRun_) {
        firstRun_ = false;
    } else {
        dt = std::chrono::duration<double>(now - lastRunTime_).count();
        // Clamp dt to avoid large jumps after pauses (e.g. breakpoints).
        dt = std::min(dt, 0.1);
    }
    lastRunTime_ = now;

    if (dt > 0.0) {
        stepKinematics(dt);
    }

    checkObstacles();
}

// ── Motor control ──────────────────────────────────────────────────────────────

void SimulationDriver::setMotorPwm(int left, int right, int mow) {
    std::lock_guard<std::mutex> lk(mutex_);
    motorLeft_  = std::clamp(left,  -255, 255);
    motorRight_ = std::clamp(right, -255, 255);
    motorMow_   = std::clamp(mow,   -255, 255);
}

void SimulationDriver::resetMotorFault() {
    // No latched fault in simulation — no-op.
}

// ── Sensor reads ───────────────────────────────────────────────────────────────

OdometryData SimulationDriver::readOdometry() {
    std::lock_guard<std::mutex> lk(mutex_);
    OdometryData d;
    d.leftTicks   = pendingLeftTicks_;
    d.rightTicks  = pendingRightTicks_;
    d.mowTicks    = pendingMowTicks_;
    d.mcuConnected = true;
    pendingLeftTicks_  = 0;
    pendingRightTicks_ = 0;
    pendingMowTicks_   = 0;
    return d;
}

SensorData SimulationDriver::readSensors() {
    std::lock_guard<std::mutex> lk(mutex_);
    SensorData d;
    d.bumperLeft  = bumperLeftActive_  || obstacleHit_;
    d.bumperRight = bumperRightActive_ || obstacleHit_;
    d.lift        = liftActive_;
    d.rain        = false;
    d.stopButton  = false;
    d.motorFault  = false;
    d.nearObstacle = obstacleHit_;
    return d;
}

BatteryData SimulationDriver::readBattery() {
    BatteryData d;
    d.voltage          = static_cast<float>(batteryVoltage_);
    d.chargeVoltage    = 0.0f;
    d.chargeCurrent    = 0.0f;
    d.batteryTemp      = 25.0f;
    d.chargerConnected = false;
    return d;
}

ImuData SimulationDriver::readImu() {
    std::lock_guard<std::mutex> lk(mutex_);
    ImuData d;
    d.valid = true;
    // Simplified simulation: imu yaw = pose heading
    d.yaw = static_cast<float>(pose_.heading);
    return d;
}

void SimulationDriver::calibrateImu() {
    // simulation: no-op
}

// ── Actuators ──────────────────────────────────────────────────────────────────

void SimulationDriver::setBuzzer(bool on) {
    buzzerOn_ = on;
}

void SimulationDriver::setLed(LedId, LedState) {
    // Silently accepted — LED state not simulated.
}

void SimulationDriver::keepPowerOn(bool flag) {
    keepPowerOn_ = flag;
}

// ── System info ───────────────────────────────────────────────────────────────

float       SimulationDriver::getCpuTemperature()      { return 45.0f; }
std::string SimulationDriver::getRobotId()             { return "SIM:00:00:00:00:00"; }
std::string SimulationDriver::getMcuFirmwareName()     { return "simulation"; }
std::string SimulationDriver::getMcuFirmwareVersion()  { return "0.0.1"; }

// ── Fault injection ───────────────────────────────────────────────────────────

void SimulationDriver::setBumperLeft(bool active) {
    std::lock_guard<std::mutex> lk(mutex_);
    bumperLeftActive_ = active;
}

void SimulationDriver::setBumperRight(bool active) {
    std::lock_guard<std::mutex> lk(mutex_);
    bumperRightActive_ = active;
}

void SimulationDriver::setLift(bool active) {
    std::lock_guard<std::mutex> lk(mutex_);
    liftActive_ = active;
}

void SimulationDriver::setGpsQuality(GpsQuality q) {
    std::lock_guard<std::mutex> lk(mutex_);
    gpsQuality_ = q;
}

void SimulationDriver::addObstacle(Polygon poly) {
    std::lock_guard<std::mutex> lk(mutex_);
    obstacles_.push_back(std::move(poly));
}

void SimulationDriver::clearObstacles() {
    std::lock_guard<std::mutex> lk(mutex_);
    obstacles_.clear();
    obstacleHit_ = false;
}

// ── State inspection ──────────────────────────────────────────────────────────

SimPose SimulationDriver::getPose() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return pose_;
}

GpsQuality SimulationDriver::getGpsQuality() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return gpsQuality_;
}

void SimulationDriver::setPose(SimPose pose) {
    std::lock_guard<std::mutex> lk(mutex_);
    pose_ = pose;
}

// ── Private: kinematic step ───────────────────────────────────────────────────

void SimulationDriver::stepKinematics(double dt) {
    // Map PWM (-255…+255) to wheel speed (m/s)
    const double vLeft  = (motorLeft_  / 255.0) * maxSpeed_ms_;
    const double vRight = (motorRight_ / 255.0) * maxSpeed_ms_;
    const double vMow   = std::abs(motorMow_  / 255.0) * maxSpeed_ms_;

    // Unicycle model
    const double v     = (vLeft + vRight) * 0.5;
    const double omega = (vRight - vLeft) / wheelBase_m_;

    // Pose integration
    pose_.x       += v * std::cos(pose_.heading) * dt;
    pose_.y       += v * std::sin(pose_.heading) * dt;
    pose_.heading += omega * dt;

    // Normalise heading to (-π, π]
    while (pose_.heading >  M_PI) pose_.heading -= 2.0 * M_PI;
    while (pose_.heading <= -M_PI) pose_.heading += 2.0 * M_PI;

    // Accumulate encoder ticks
    const double arcPerTick = (2.0 * M_PI * wheelRadius_m_) / ticksPerRev_;
    accumLeftTicks_  += (vLeft  * dt) / arcPerTick;
    accumRightTicks_ += (vRight * dt) / arcPerTick;
    accumMowTicks_   += (vMow   * dt) / arcPerTick;

    // Flush integer ticks into pending counters
    const int dLeft  = static_cast<int>(accumLeftTicks_);
    const int dRight = static_cast<int>(accumRightTicks_);
    const int dMow   = static_cast<int>(accumMowTicks_);

    pendingLeftTicks_  += dLeft;
    pendingRightTicks_ += dRight;
    pendingMowTicks_   += dMow;

    accumLeftTicks_  -= dLeft;
    accumRightTicks_ -= dRight;
    accumMowTicks_   -= dMow;
}

// ── Private: obstacle check ───────────────────────────────────────────────────

void SimulationDriver::checkObstacles() {
    const Vec2 pos{pose_.x, pose_.y};
    for (const auto& poly : obstacles_) {
        if (pointInPolygon(pos, poly)) {
            obstacleHit_ = true;
            return;
        }
    }
    obstacleHit_ = false;
}

// ── Private: point-in-polygon (ray casting) ───────────────────────────────────

bool SimulationDriver::pointInPolygon(const Vec2& p, const Polygon& poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    const std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = poly[i].x, yi = poly[i].y;
        const double xj = poly[j].x, yj = poly[j].y;
        const bool intersect =
            ((yi > p.y) != (yj > p.y)) &&
            (p.x < (xj - xi) * (p.y - yi) / (yj - yi) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

} // namespace sunray
