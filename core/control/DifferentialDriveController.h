#pragma once

/// DifferentialDriveController — closed-loop wheel command refinement.
///
/// This controller starts from the open-loop PWM command and, when odometry is
/// available, applies a per-wheel PID correction based on measured wheel speed.
/// The goal is not full drivetrain modelling, but a lightweight improvement
/// over pure open loop while keeping the command path robust when feedback is
/// missing or stale.

#include "core/Config.h"
#include "core/control/OpenLoopDriveController.h"
#include "core/control/PidController.h"
#include "hal/HardwareInterface.h"

namespace sunray::control {

class DifferentialDriveController {
public:
    /// Reset PID state and filtered measurements.
    void reset();
    float lastCommandedLinear() const { return lastCommandedLinear_ms_; }

    /// Compute final left/right PWM from desired motion and wheel odometry.
    ///
    /// Falls back to open-loop behaviour if the MCU is disconnected or `dt_ms`
    /// is zero, so callers do not need a separate degraded-mode path.
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
    float lastCommandedLinear_ms_ = 0.0f;
    bool filterInitialized_ = false;
};

} // namespace sunray::control
