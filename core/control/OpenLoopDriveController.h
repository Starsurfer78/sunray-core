#pragma once

/// OpenLoopDriveController — kinematic velocity-to-PWM mapping.
///
/// This module converts desired robot motion `(linear, angular)` into left/right
/// PWM commands without using wheel feedback. It is the simplest possible drive
/// path and also serves as the base command that the closed-loop controller can
/// later refine with PID corrections.

#include "core/Config.h"

namespace sunray::control {

struct DrivePwmCommand {
    int left = 0;
    int right = 0;
};

class OpenLoopDriveController {
public:
    /// Compute differential-drive PWM directly from linear/angular velocity.
    ///
    /// Inputs are interpreted in robot space:
    ///   - `linear_ms`      forward speed in m/s
    ///   - `angular_radps`  yaw rate in rad/s
    ///
    /// Config is used for nominal max speed and wheel base. The result is
    /// clamped to the motor PWM range `[-255, 255]`.
    static DrivePwmCommand compute(const Config& config, float linear_ms, float angular_radps);
};

} // namespace sunray::control
