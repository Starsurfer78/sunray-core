#pragma once
// RobotConstants.h — Named compile-time constants for the Sunray Core.
//
// These are architectural constants, NOT runtime tuning parameters.
// Tuning parameters belong in Config / config.json.
//
// Naming: UPPER_SNAKE_CASE, unit in name suffix where unambiguous.

namespace sunray {

// ── Motion ────────────────────────────────────────────────────────────────────

/// Maximum time the robot may remain in a motion-requested state before the
/// motion is considered lost and a fault event is raised (ms).
inline constexpr unsigned long OVERALL_MOTION_TIMEOUT_MS = 10'000;

/// Time constant for the ground-speed low-pass filter (ms).
/// Determines how quickly the smoothed speed estimate reacts to changes.
inline constexpr unsigned long MOTION_LP_DECAY_MS = 2'000;

/// GPS displacement below this value is treated as "robot not moving" (metres).
/// Used by stuck-detection and GPS no-motion checks.
/// Runtime-tuneable via config key "gps_no_motion_threshold_m".
inline constexpr float GPS_NO_MOTION_THRESHOLD_M = 0.05f;

// ── Obstacle-avoidance rotation ───────────────────────────────────────────────

/// Maximum time the robot may rotate toward a new heading after an obstacle (ms).
/// If not aligned within this window the rotation phase is aborted.
inline constexpr unsigned long OBSTACLE_ROTATION_TIMEOUT_MS = 15'000;

/// Angular speed used during obstacle-avoidance rotation (degrees per second).
/// Note: an earlier comment in the original firmware incorrectly stated 8 °/s;
/// the correct value derived from the STM32 PWM calibration is 10 °/s.
inline constexpr float OBSTACLE_ROTATION_SPEED_DEG_S = 10.0f;

} // namespace sunray
