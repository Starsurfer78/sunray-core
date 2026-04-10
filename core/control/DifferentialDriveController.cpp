// DifferentialDriveController.cpp — wheel-speed feedback controller.
//
// Control flow:
//   1. build an open-loop base PWM from the desired robot motion
//   2. if feedback is unavailable, return that base command unchanged
//   3. estimate measured left/right wheel speeds from odometry ticks
//   4. low-pass filter the measured speeds to reduce tick noise
//   5. apply one PID correction per wheel and clamp the final PWM

#include "core/control/DifferentialDriveController.h"

#include <algorithm>
#include <cmath>

namespace sunray::control {

void DifferentialDriveController::reset() {
    leftPid_.reset();
    rightPid_.reset();
    leftFiltered_ms_ = 0.0f;
    rightFiltered_ms_ = 0.0f;
    lastCommandedLinear_ms_ = 0.0f;
    filterInitialized_ = false;
}

DrivePwmCommand DifferentialDriveController::compute(const Config& config,
                                                     float linear_ms,
                                                     float angular_radps,
                                                     const OdometryData& odometry,
                                                     unsigned long dt_ms) {
    // Always compute the open-loop baseline first; it doubles as the fallback
    // path whenever wheel feedback is unavailable.
    const auto base = OpenLoopDriveController::compute(config, linear_ms, angular_radps);
    lastCommandedLinear_ms_ = linear_ms;

    if (std::fabs(linear_ms) < 1e-4f && std::fabs(angular_radps) < 1e-4f) {
        reset();
        return {};
    }

    if (!odometry.mcuConnected || dt_ms == 0) {
        return base;
    }

    const float ticksPerRev = static_cast<float>(
        config.get<int>("ticks_per_revolution", config.get<int>("ticks_per_rev", 320)));
    const float wheelDiameter = config.get<float>("wheel_diameter_m", 0.205f);
    const float wheelBase = config.get<float>("wheel_base_m", 0.390f);
    const float dt_s = static_cast<float>(dt_ms) / 1000.0f;
    const float metresPerTick = (static_cast<float>(M_PI) * wheelDiameter) / std::max(1.0f, ticksPerRev);

    const float leftMeasured = static_cast<float>(odometry.leftTicks) * metresPerTick / dt_s;
    const float rightMeasured = static_cast<float>(odometry.rightTicks) * metresPerTick / dt_s;

    const float lp = std::clamp(config.get<float>("motor_pid_lp", 0.0f), 0.0f, 1.0f);
    // Optional first-order low-pass filter to make the PID react less strongly
    // to single noisy odometry intervals.
    if (!filterInitialized_ || lp <= 0.0f) {
        leftFiltered_ms_ = leftMeasured;
        rightFiltered_ms_ = rightMeasured;
        filterInitialized_ = true;
    } else {
        leftFiltered_ms_ = leftFiltered_ms_ * (1.0f - lp) + leftMeasured * lp;
        rightFiltered_ms_ = rightFiltered_ms_ * (1.0f - lp) + rightMeasured * lp;
    }

    const float desiredLeft = linear_ms - angular_radps * wheelBase * 0.5f;
    const float desiredRight = linear_ms + angular_radps * wheelBase * 0.5f;
    const float kp = config.get<float>("motor_pid_kp", 0.5f);
    const float ki = config.get<float>("motor_pid_ki", 0.01f);
    const float kd = config.get<float>("motor_pid_kd", 0.01f);

    const float leftCorrection = std::clamp(
        leftPid_.update(desiredLeft - leftFiltered_ms_, dt_s, kp, ki, kd), -1.0f, 1.0f);
    const float rightCorrection = std::clamp(
        rightPid_.update(desiredRight - rightFiltered_ms_, dt_s, kp, ki, kd), -1.0f, 1.0f);

    DrivePwmCommand cmd;
    cmd.left = std::clamp(base.left + static_cast<int>(std::lround(leftCorrection * 255.0f)), -255, 255);
    cmd.right = std::clamp(base.right + static_cast<int>(std::lround(rightCorrection * 255.0f)), -255, 255);
    return cmd;
}

} // namespace sunray::control
