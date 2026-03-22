#pragma once
// StateEstimator.h — Robot pose estimation (x, y, heading).
//
// Phase 1: odometry dead-reckoning from wheel encoder ticks (LOC_IMU_ODO_ONLY).
// GPS position update method provided for Phase 2 integration.
//
// Ported from sunray/StateEstimator.cpp — computeRobotState() odometry section.
// All Arduino, IMU, April-tag, Reflector-tag, and LiDAR paths removed.
//
// Coordinate system: local metres (east = +x, north = +y).
// Heading: radians, 0 = east (atan2 convention), +π/2 = north.
//
// Usage:
//   update(odo, dt_ms)               — call once per control loop
//   updateGps(posE, posN, fix, flt)  — call when GPS solution available (Phase 2)
//   x(), y(), heading()              — current pose

#include "core/Config.h"
#include "hal/HardwareInterface.h"

#include <cmath>
#include <memory>

namespace sunray {
namespace nav {

class StateEstimator {
public:
    explicit StateEstimator(std::shared_ptr<Config> config = nullptr);

    // ── Pose update ───────────────────────────────────────────────────────────

    /// Update pose from odometry ticks. Call once per control loop.
    /// odo.leftTicks / rightTicks are incremental deltas since last readOdometry().
    /// dt_ms: elapsed time since last call (used for ground-speed estimate).
    void update(const OdometryData& odo, unsigned long dt_ms);

    /// GPS position update stub.
    /// posE/posN: metres east/north from map origin. isFix: RTK fixed.
    /// Phase 1: not called. Phase 2: call from Robot::run() after GPS read.
    void updateGps(float posE, float posN, bool isFix, bool isFloat);

    // ── Pose accessors ────────────────────────────────────────────────────────

    float x()           const { return x_; }
    float y()           const { return y_; }
    float heading()     const { return heading_; }    ///< rad, 0=east
    float groundSpeed() const { return groundSpeed_; } ///< m/s (low-pass)
    bool  gpsHasFix()   const { return gpsHasFix_; }
    bool  gpsHasFloat() const { return gpsHasFloat_; }

    /// Override pose directly (e.g. after manual placement or GPS cold-start).
    void setPose(float x, float y, float heading);

    /// Reset all state to origin.
    void reset();

private:
    static float scalePI(float v);

    std::shared_ptr<Config> config_;

    float x_           = 0.0f;
    float y_           = 0.0f;
    float heading_     = 0.0f;  ///< rad, 0=east
    float groundSpeed_ = 0.0f;  ///< m/s, low-pass filtered

    bool  gpsHasFix_   = false;
    bool  gpsHasFloat_ = false;

    bool  firstUpdate_ = true;  ///< skip first valid frame (no prior delta)
};

} // namespace nav
} // namespace sunray
