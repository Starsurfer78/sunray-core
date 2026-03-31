#pragma once

namespace sunray {

enum class ButtonHoldAction {
    None,
    EmergencyStop,
    StartDocking,
    StartMowing,
    Shutdown,
};

struct ButtonHoldThresholds {
    unsigned emergencyStop_s = 1;
    unsigned startDocking_s  = 5;
    unsigned startMowing_s   = 6;
    unsigned shutdown_s      = 9;
};

ButtonHoldAction resolveButtonHoldAction(
    unsigned heldSeconds,
    const ButtonHoldThresholds& thresholds = {});

} // namespace sunray
