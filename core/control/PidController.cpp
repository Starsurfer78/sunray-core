// PidController.cpp — compact PID implementation used by drive control.
//
// The controller intentionally stays small and predictable:
//   - no internal gain storage
//   - no output clamping
//   - only mild anti-windup via integral clamping
// Higher-level drive code remains responsible for choosing gains and for
// clamping the final actuator command to the allowed PWM range.

#include "core/control/PidController.h"

#include <algorithm>

namespace sunray::control {

void PidController::reset() {
    integral_ = 0.0f;
    previousError_ = 0.0f;
    initialized_ = false;
}

float PidController::update(float error, float dt_s, float kp, float ki, float kd) {
    if (dt_s <= 0.0f) return kp * error;

    // Keep the integrator bounded so long saturation periods do not dominate
    // the response once the wheel speed catches up again.
    integral_ += error * dt_s;
    integral_ = std::clamp(integral_, -1.0f, 1.0f);

    float derivative = 0.0f;
    if (initialized_) {
        derivative = (error - previousError_) / dt_s;
    }

    previousError_ = error;
    initialized_ = true;
    return kp * error + ki * integral_ + kd * derivative;
}

} // namespace sunray::control
