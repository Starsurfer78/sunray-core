// DifferentialDriveController.cpp — wheel-speed feedback controller.
//
// Control flow:
//   1. build an open-loop base PWM from the desired robot motion
//   2. if feedback is unavailable, return that base command unchanged
//   3. estimate measured left/right wheel speeds from odometry ticks
//   4. low-pass filter the measured speeds to reduce tick noise
//   5. apply one PID correction per wheel and clamp the final PWM

#include "core/control/DifferentialDriveController.h"
#include "core/control/OpenLoopDriveController.h"
#include <algorithm>
#include <cmath>

namespace sunray::control {

void DifferentialDriveController::reset() {
    leftPid_.reset();
    rightPid_.reset();
    leftPwmCurr_ = 0.0f;
    rightPwmCurr_ = 0.0f;
    leftFilteredRpm_ = 0.0f;
    rightFilteredRpm_ = 0.0f;
    lastCommandedLinear_ms_ = 0.0f;
    filterInitialized_ = false;
}

DrivePwmCommand DifferentialDriveController::compute(const Config& config,
                                                     float linear_ms,
                                                     float angular_radps,
                                                     const OdometryData& odometry,
                                                     unsigned long dt_ms) {
    if (std::fabs(linear_ms) < 1e-4f && std::fabs(angular_radps) < 1e-4f) {
        reset();
        return {};
    }

    if (!odometry.mcuConnected || dt_ms == 0) {
        // Fallback if no MCU connected (just OpenLoop)
        lastCommandedLinear_ms_ = linear_ms;
        return OpenLoopDriveController::compute(config, linear_ms, angular_radps);
    }

    lastCommandedLinear_ms_ = linear_ms;

    const float ticksPerRev = static_cast<float>(
        config.get<int>("ticks_per_revolution", config.get<int>("ticks_per_rev", 320)));
    const float wheelDiameter = config.get<float>("wheel_diameter_m", 0.205f);
    const float wheelBase = config.get<float>("wheel_base_m", 0.390f);
    const float dt_s = static_cast<float>(dt_ms) / 1000.0f;

    // 1. Calculate desired RPM
    const float desiredLeft_ms = linear_ms - angular_radps * wheelBase * 0.5f;
    const float desiredRight_ms = linear_ms + angular_radps * wheelBase * 0.5f;
    
    const float rpm_per_ms = 60.0f / (static_cast<float>(M_PI) * wheelDiameter);
    const float desiredLeftRpm = desiredLeft_ms * rpm_per_ms;
    const float desiredRightRpm = desiredRight_ms * rpm_per_ms;

    // 2. Calculate measured RPM
    float leftRpm = 60.0f * (static_cast<float>(odometry.leftTicks) / ticksPerRev) / dt_s;
    float rightRpm = 60.0f * (static_cast<float>(odometry.rightTicks) / ticksPerRev) / dt_s;

    // 3. Low-Pass Filter
    const float lp = std::clamp(config.get<float>("motor_pid_lp", 0.0f), 0.0f, 1.0f);
    if (!filterInitialized_ || lp <= 0.0f) {
        leftFilteredRpm_ = leftRpm;
        rightFilteredRpm_ = rightRpm;
        filterInitialized_ = true;
    } else {
        leftFilteredRpm_ = leftFilteredRpm_ * (1.0f - lp) + leftRpm * lp;
        rightFilteredRpm_ = rightFilteredRpm_ * (1.0f - lp) + rightRpm * lp;
    }

    // 4. PID Compute (Accumulating Delta-PWM)
    const float kp = config.get<float>("motor_pid_kp", 0.5f);
    const float ki = config.get<float>("motor_pid_ki", 0.01f);
    const float kd = config.get<float>("motor_pid_kd", 0.01f);
    const float max_pwm = 255.0f;

    leftPwmCurr_ += leftPid_.compute(desiredLeftRpm, leftFilteredRpm_, dt_s, kp, ki, kd, max_pwm);
    rightPwmCurr_ += rightPid_.compute(desiredRightRpm, rightFilteredRpm_, dt_s, kp, ki, kd, max_pwm);

    // 5. Clamp to allowed direction (0..255 or -255..0)
    if (desiredLeftRpm >= 0) {
        leftPwmCurr_ = std::clamp(leftPwmCurr_, 0.0f, max_pwm);
    } else {
        leftPwmCurr_ = std::clamp(leftPwmCurr_, -max_pwm, 0.0f);
    }

    if (desiredRightRpm >= 0) {
        rightPwmCurr_ = std::clamp(rightPwmCurr_, 0.0f, max_pwm);
    } else {
        rightPwmCurr_ = std::clamp(rightPwmCurr_, -max_pwm, 0.0f);
    }

    // 6. Stall / Zero override (Stiction bypass)
    if (std::abs(desiredLeftRpm) < 0.01f && std::abs(leftPwmCurr_) < 30.0f) leftPwmCurr_ = 0.0f;
    if (std::abs(desiredRightRpm) < 0.01f && std::abs(rightPwmCurr_) < 30.0f) rightPwmCurr_ = 0.0f;

    DrivePwmCommand cmd;
    cmd.left = static_cast<int>(std::lround(leftPwmCurr_));
    cmd.right = static_cast<int>(std::lround(rightPwmCurr_));
    return cmd;
}

} // namespace sunray::control
