// OpenLoopDriveController.cpp — direct differential-drive command mapping.
//
// The controller applies the standard wheel-speed split for a differential
// drive robot:
//   left  = v - omega * wheelBase / 2
//   right = v + omega * wheelBase / 2
//
// These wheel speeds are then normalized against the configured nominal motor
// speed and converted to raw PWM values.

#include "core/control/OpenLoopDriveController.h"

#include <algorithm>

namespace sunray::control {

DrivePwmCommand OpenLoopDriveController::compute(const Config& config,
                                                 float linear_ms,
                                                 float angular_radps) {
    const float maxSpeed  = config.get<float>("motor_set_speed_ms", 0.5f);
    const float wheelBase = config.get<float>("wheel_base_m", 0.285f);

    const float vLeft  = linear_ms  - angular_radps * wheelBase * 0.5f;
    const float vRight = linear_ms  + angular_radps * wheelBase * 0.5f;

    DrivePwmCommand cmd;
    cmd.left = std::clamp(static_cast<int>(vLeft  / maxSpeed * 255.0f), -255, 255);
    cmd.right = std::clamp(static_cast<int>(vRight / maxSpeed * 255.0f), -255, 255);
    return cmd;
}

} // namespace sunray::control
