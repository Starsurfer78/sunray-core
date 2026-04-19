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
    esum_ = 0.0f;
    eold_ = 0.0f;
}

float PidController::compute(float w, float x, float dt_s, float kp, float ki, float kd, float max_output) {
    if (dt_s <= 0.0f) return 0.0f;

    float e = w - x;
    esum_ += e;

    // anti wind-up
    esum_ = std::clamp(esum_, -max_output, max_output);

    float y = kp * e + ki * dt_s * esum_ + kd / dt_s * (e - eold_);
    eold_ = e;

    return std::clamp(y, -max_output, max_output);
}

} // namespace sunray::control
