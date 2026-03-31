#pragma once

#include "hal/HardwareInterface.h"

#include <cstddef>
#include <cstdint>

namespace sunray {

enum class BuzzerPattern {
    ButtonTick,
    StartRejected,
    StartMowingAccepted,
    StartDockingAccepted,
    ShutdownRequested,
    EmergencyStop,
};

struct BuzzerStep {
    bool     on = false;
    uint32_t durationMs = 0;
};

class BuzzerSequencer {
public:
    void play(HardwareInterface& hw, uint64_t nowMs, BuzzerPattern pattern);
    void stop(HardwareInterface& hw);
    void tick(HardwareInterface& hw, uint64_t nowMs);

private:
    void start(HardwareInterface& hw, uint64_t nowMs,
               const BuzzerStep* steps, std::size_t count);
    void applyCurrentStep(HardwareInterface& hw, uint64_t nowMs);

    const BuzzerStep* steps_       = nullptr;
    std::size_t       stepCount_   = 0;
    std::size_t       stepIndex_   = 0;
    uint64_t          nextStepAtMs_ = 0;
    bool              active_      = false;
};

} // namespace sunray
