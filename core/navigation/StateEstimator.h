#pragma once
// StateEstimator.h — Robot pose estimation (x, y, heading).
//
// Extended Kalman Filter (EKF) fusing odometry, GPS, and IMU.
//
// State:    [x, y, θ]  (metres east, metres north, heading in radians)
// Predict:  differential-drive odometry (wheel encoder ticks)
// Update 1: GPS RTK position (posE, posN) when fix available
// Update 2: IMU heading (yaw) when valid
// Failover: GPS updates suspended if fix absent > ekf_gps_failover_ms
//
// Config keys (all optional, defaults shown):
//   ekf_q_xy              — process noise: position  (0.0001 m²/step)
//   ekf_q_theta           — process noise: heading   (0.0001 rad²/step)
//   ekf_r_gps             — GPS measurement noise    (0.05 m)
//   ekf_r_imu             — IMU heading noise        (0.02 rad)
//   ekf_gps_failover_ms   — GPS failover threshold   (5000 ms)
//
// Usage:
//   update(odo, dt_ms)               — call once per 20 ms control loop
//   updateGps(posE, posN, fix, flt)  — call when GPS solution available
//   updateImu(yaw, roll, pitch)      — call when IMU data valid
//   x(), y(), heading()              — current fused pose
//   fusionMode()                     — "EKF+GPS" | "EKF+IMU" | "Odo"
//   posUncertainty()                 — sqrt(P_xx + P_yy) in metres

#include "core/Config.h"
#include "hal/HardwareInterface.h"

#include <cmath>
#include <memory>
#include <string>

namespace sunray {
namespace nav {

class StateEstimator {
public:
    explicit StateEstimator(std::shared_ptr<Config> config = nullptr);

    // ── Pose update ───────────────────────────────────────────────────────────

    /// EKF predict step from odometry ticks. Call once per control loop.
    /// odo.leftTicks / rightTicks are incremental deltas since last readOdometry().
    /// dt_ms: elapsed time since last call (used for ground-speed estimate and GPS failover).
    void update(const OdometryData& odo, unsigned long dt_ms);

    /// EKF GPS measurement update. posE/posN: metres east/north from map origin.
    /// Only fuses on isFix (RTK-Fixed); float sets gpsHasFloat_ flag only.
    void updateGps(float posE, float posN, bool isFix, bool isFloat);

    /// EKF IMU heading update. yaw/roll/pitch in radians (integrated angles from driver).
    void updateImu(float yaw, float roll, float pitch);

    // ── Pose accessors ────────────────────────────────────────────────────────

    float x()           const { return x_; }
    float y()           const { return y_; }
    float heading()     const { return heading_; }    ///< rad, 0=east
    float groundSpeed() const { return groundSpeed_; } ///< m/s (low-pass)
    bool  gpsHasFix()   const { return gpsHasFix_; }
    bool  gpsHasFloat() const { return gpsHasFloat_; }

    /// Fusion mode string: "EKF+GPS", "EKF+IMU", or "Odo".
    std::string fusionMode() const;

    /// Position uncertainty: sqrt(P_xx + P_yy) in metres.
    float posUncertainty() const;

    /// Override pose directly (e.g. after manual placement or GPS cold-start).
    /// Resets covariance to initial value.
    void setPose(float x, float y, float heading);

    /// Reset all state to origin and reset EKF covariance to initial value.
    void reset();

private:
    // ── EKF noise parameters (loaded from Config) ─────────────────────────────

    struct EkfNoise {
        float         q_xy;         ///< process noise: position  (m²/step)
        float         q_theta;      ///< process noise: heading   (rad²/step)
        float         r_gps;        ///< GPS noise std-dev        (m)
        float         r_imu;        ///< IMU heading noise std-dev (rad)
        unsigned long failover_ms;  ///< ms without GPS fix before failover
    };
    EkfNoise loadNoise() const;

    // ── Private EKF step implementations ─────────────────────────────────────

    /// Predict step: update state + covariance from odometry deltas.
    void predictStep(float dDist, float dTheta, const EkfNoise& n);

    /// GPS measurement update (RTK-Fix only).
    void gpsUpdate(float measE, float measN, const EkfNoise& n);

    /// IMU heading measurement update.
    void imuUpdate(float yaw, const EkfNoise& n);

    static float scalePI(float v);

    // ── Pose state ────────────────────────────────────────────────────────────

    std::shared_ptr<Config> config_;

    float x_           = 0.0f;
    float y_           = 0.0f;
    float heading_     = 0.0f;  ///< rad, 0=east
    float groundSpeed_ = 0.0f;  ///< m/s, low-pass filtered

    bool  gpsHasFix_   = false;
    bool  gpsHasFloat_ = false;
    bool  imuActive_   = false;  ///< true after first valid updateImu() call

    bool  firstUpdate_ = true;

    // ── EKF covariance (3×3, row-major: P_[i*3+j]) ───────────────────────────

    float P_[9] = {};  ///< reset() initialises to diag(0.01, 0.01, 0.01)

    // ── GPS failover tracking ─────────────────────────────────────────────────

    unsigned long totalTime_ms_      = 0;  ///< accumulated from update() dt_ms
    unsigned long lastGpsFixTime_ms_ = 0;  ///< updated on each RTK fix received
};

} // namespace nav
} // namespace sunray
