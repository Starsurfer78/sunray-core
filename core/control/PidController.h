#pragma once

/// PidController — minimal reusable PID helper for wheel-speed control.
///
/// This class stores only the controller state (integral + previous error).
/// Gains are passed in on every update so callers can source tuning values
/// from Config without rebuilding the controller instance.
///
/// Current behaviour:
///   - integral term is clamped to a small fixed range to limit wind-up
///   - derivative term is disabled on the very first sample
///   - invalid/zero dt falls back to proportional control only

namespace sunray::control {

class PidController {
public:
    /// Clear accumulated integral and derivative history.
    void reset();

    /// Compute one PID output sample for the given control error.
    float update(float error, float dt_s, float kp, float ki, float kd);

private:
    float integral_ = 0.0f;
    float previousError_ = 0.0f;
    bool initialized_ = false;
};

} // namespace sunray::control
