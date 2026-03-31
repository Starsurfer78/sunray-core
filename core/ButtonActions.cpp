#include "core/ButtonActions.h"

namespace sunray {

ButtonHoldAction resolveButtonHoldAction(
    unsigned heldSeconds,
    const ButtonHoldThresholds& thresholds) {
    if (heldSeconds >= thresholds.shutdown_s) {
        return ButtonHoldAction::Shutdown;
    }
    if (heldSeconds >= thresholds.startMowing_s) {
        return ButtonHoldAction::StartMowing;
    }
    if (heldSeconds >= thresholds.startDocking_s) {
        return ButtonHoldAction::StartDocking;
    }
    if (heldSeconds >= thresholds.emergencyStop_s) {
        return ButtonHoldAction::EmergencyStop;
    }
    return ButtonHoldAction::None;
}

} // namespace sunray
