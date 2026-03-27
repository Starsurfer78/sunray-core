// WaitRainOp.cpp — explicit rain handling phase before/while docking.
#include "core/op/Op.h"

namespace sunray {

void WaitRainOp::begin(OpContext& ctx) {
    ctx.logger.info("WaitRain", "OP_WAIT_RAIN");
    ctx.stopMotors();
    dockingRequested = false;
}

void WaitRainOp::run(OpContext& ctx) {
    ctx.stopMotors();

    if (!ctx.sensors.rain) {
        ctx.logger.info("WaitRain", "rain cleared => IDLE");
        changeOp(ctx, ctx.opMgr.idle());
        return;
    }

    if (!ctx.battery.chargerConnected && !dockingRequested) {
        dockingRequested = true;
        ctx.logger.info("WaitRain", "rain active away from dock => DOCK");
        changeOp(ctx, ctx.opMgr.dock());
    }
}

void WaitRainOp::end(OpContext& ctx) {
    ctx.stopMotors();
}

void WaitRainOp::onRainTriggered(OpContext& ctx) {
    ctx.logger.info("WaitRain", "rain still active");
    (void)ctx;
}

void WaitRainOp::onBatteryLowShouldDock(OpContext& ctx) {
    if (!ctx.battery.chargerConnected) {
        ctx.logger.warn("WaitRain", "battery low during rain wait => DOCK");
        changeOp(ctx, ctx.opMgr.dock());
    }
}

} // namespace sunray
