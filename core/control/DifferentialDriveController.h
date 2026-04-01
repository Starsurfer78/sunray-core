#pragma once

#include "core/Config.h"
#include "core/control/OpenLoopDriveController.h"
#include "core/control/PidController.h"
#include "hal/HardwareInterface.h"

namespace sunray::control {

class DifferentialDriveController {
public:
    void reset();

    DrivePwmCommand compute(const Config& config,
                            float linear_ms,
                            float angular_radps,
                            const OdometryData& odometry,
                            unsigned long dt_ms);

private:
    PidController leftPid_;
    PidController rightPid_;
    float leftFiltered_ms_ = 0.0f;
    float rightFiltered_ms_ = 0.0f;
    bool filterInitialized_ = false;
};

} // namespace sunray::control
