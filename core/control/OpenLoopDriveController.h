#pragma once

#include "core/Config.h"

namespace sunray::control {

struct DrivePwmCommand {
    int left = 0;
    int right = 0;
};

class OpenLoopDriveController {
public:
    /// Compute differential-drive PWM directly from linear/angular velocity.
    /// This preserves the current open-loop behavior and exists to decouple
    /// the mapping from OpContext before a later PID controller is introduced.
    static DrivePwmCommand compute(const Config& config, float linear_ms, float angular_radps);
};

} // namespace sunray::control
