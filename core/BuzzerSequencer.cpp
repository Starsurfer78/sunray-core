#include "core/BuzzerSequencer.h"

namespace sunray {

namespace {

constexpr BuzzerStep kButtonTickPattern[] = {
    {true, 100},
    {false, 0},
};

constexpr BuzzerStep kStartRejectedPattern[] = {
    {true, 120},
    {false, 120},
    {true, 120},
    {false, 0},
};

constexpr BuzzerStep kStartMowingAcceptedPattern[] = {
    {true, 90},
    {false, 60},
    {true, 160},
    {false, 0},
};

constexpr BuzzerStep kStartDockingAcceptedPattern[] = {
    {true, 180},
    {false, 0},
};

constexpr BuzzerStep kChargerConnectedPattern[] = {
    {true, 90},
    {false, 90},
    {true, 90},
    {false, 0},
};

constexpr BuzzerStep kShutdownRequestedPattern[] = {
    {true, 220},
    {false, 80},
    {true, 220},
    {false, 0},
};

constexpr BuzzerStep kEmergencyStopPattern[] = {
    {true, 250},
    {false, 0},
};

} // namespace

void BuzzerSequencer::play(HardwareInterface& hw, uint64_t nowMs, BuzzerPattern pattern) {
    switch (pattern) {
        case BuzzerPattern::ButtonTick:
            start(hw, nowMs, kButtonTickPattern,
                  sizeof(kButtonTickPattern) / sizeof(kButtonTickPattern[0]));
            break;
        case BuzzerPattern::StartRejected:
            start(hw, nowMs, kStartRejectedPattern,
                  sizeof(kStartRejectedPattern) / sizeof(kStartRejectedPattern[0]));
            break;
        case BuzzerPattern::StartMowingAccepted:
            start(hw, nowMs, kStartMowingAcceptedPattern,
                  sizeof(kStartMowingAcceptedPattern) / sizeof(kStartMowingAcceptedPattern[0]));
            break;
        case BuzzerPattern::StartDockingAccepted:
            start(hw, nowMs, kStartDockingAcceptedPattern,
                  sizeof(kStartDockingAcceptedPattern) / sizeof(kStartDockingAcceptedPattern[0]));
            break;
        case BuzzerPattern::ChargerConnected:
            start(hw, nowMs, kChargerConnectedPattern,
                  sizeof(kChargerConnectedPattern) / sizeof(kChargerConnectedPattern[0]));
            break;
        case BuzzerPattern::ShutdownRequested:
            start(hw, nowMs, kShutdownRequestedPattern,
                  sizeof(kShutdownRequestedPattern) / sizeof(kShutdownRequestedPattern[0]));
            break;
        case BuzzerPattern::EmergencyStop:
            start(hw, nowMs, kEmergencyStopPattern,
                  sizeof(kEmergencyStopPattern) / sizeof(kEmergencyStopPattern[0]));
            break;
    }
}

void BuzzerSequencer::stop(HardwareInterface& hw) {
    active_ = false;
    steps_ = nullptr;
    stepCount_ = 0;
    stepIndex_ = 0;
    nextStepAtMs_ = 0;
    hw.setBuzzer(false);
}

void BuzzerSequencer::tick(HardwareInterface& hw, uint64_t nowMs) {
    if (!active_ || steps_ == nullptr) return;
    if (nowMs < nextStepAtMs_) return;

    ++stepIndex_;
    if (stepIndex_ >= stepCount_) {
        stop(hw);
        return;
    }
    applyCurrentStep(hw, nowMs);
}

void BuzzerSequencer::start(HardwareInterface& hw, uint64_t nowMs,
                            const BuzzerStep* steps, std::size_t count) {
    steps_ = steps;
    stepCount_ = count;
    stepIndex_ = 0;
    active_ = (steps_ != nullptr && stepCount_ > 0);
    applyCurrentStep(hw, nowMs);
}

void BuzzerSequencer::applyCurrentStep(HardwareInterface& hw, uint64_t nowMs) {
    const auto& step = steps_[stepIndex_];
    hw.setBuzzer(step.on);
    nextStepAtMs_ = nowMs + step.durationMs;
    if (step.durationMs == 0) {
        ++stepIndex_;
        if (stepIndex_ >= stepCount_) {
            stop(hw);
        }
    }
}

} // namespace sunray
