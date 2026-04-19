#pragma once

namespace sunray::control {

/// PidController — matches the original Sunray PID implementation.
class PidController {
public:
    void reset();

    /// Compute one PID output sample for the given control error.
    /// @param w The desired setpoint (e.g. target RPM)
    /// @param x The measured value (e.g. current RPM)
    /// @param dt_s The time delta in seconds
    /// @param kp Proportional gain
    /// @param ki Integral gain
    /// @param kd Derivative gain
    /// @param max_output Absolute maximum limit for the integral sum and final output
    /// @return The computed control variable
    float compute(float w, float x, float dt_s, float kp, float ki, float kd, float max_output);

private:
    float esum_ = 0.0f;
    float eold_ = 0.0f;
};

} // namespace sunray::control
