// MowOp.cpp — mowing operation.
#include "core/op/Op.h"
#include "core/navigation/Map.h"
#include "core/navigation/LineTracker.h"

namespace sunray {

void MowOp::begin(OpContext& ctx) {
    ctx.logger.info("Mow", "OP_MOW");
    ctx.hw.setMotorPwm(0, 0, 200);  // start mow blade

    if (ctx.map && !ctx.map->startMowing(ctx.x, ctx.y)) {
        ctx.logger.error("Mow", "cannot start mowing route => IDLE");
        changeOp(ctx, ctx.opMgr.idle());
        return;
    }
    if (ctx.lineTracker) ctx.lineTracker->reset();
}

void MowOp::end(OpContext& ctx) {
    ctx.stopMotors();
    ctx.hw.setMotorPwm(0, 0, 0);  // stop mow blade
}

void MowOp::run(OpContext& ctx) {
    if (ctx.lineTracker && ctx.map && ctx.stateEst) {
        ctx.lineTracker->track(ctx, *ctx.map, *ctx.stateEst);
    }
}

void MowOp::onGpsNoSignal(OpContext& ctx) {
    ctx.logger.warn("Mow", "GPS signal lost => GpsWait");
    changeOp(ctx, ctx.opMgr.gpsWait(), true);  // return to Mow after fix
}

void MowOp::onGpsFixTimeout(OpContext& ctx) {
    ctx.logger.error("Mow", "GPS fix timeout => DOCK");
    changeOp(ctx, ctx.opMgr.dock());
}

void MowOp::onObstacle(OpContext& ctx) {
    ctx.logger.info("Mow", "obstacle => EscapeReverse");
    changeOp(ctx, ctx.opMgr.escape(), true);  // return to Mow after escape
}

void MowOp::onLiftTriggered(OpContext& ctx) {
    ctx.logger.error("Mow", "lift sensor => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void MowOp::onMotorError(OpContext& ctx) {
    ctx.logger.error("Mow", "motor error => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void MowOp::onRainTriggered(OpContext& ctx) {
    const int rainDelay = ctx.config.get<int>("rain_delay_min", 60);
    ctx.logger.info("Mow", "rain => DOCK (delay " + std::to_string(rainDelay) + "min)");
    changeOp(ctx, ctx.opMgr.dock());
}

void MowOp::onBatteryLowShouldDock(OpContext& ctx) {
    ctx.logger.info("Mow", "battery low => DOCK");
    changeOp(ctx, ctx.opMgr.dock());
}

void MowOp::onNoFurtherWaypoints(OpContext& ctx) {
    ctx.logger.info("Mow", "no further waypoints => DOCK");
    changeOp(ctx, ctx.opMgr.dock());
}

void MowOp::onTimetableStopMowing(OpContext& ctx) {
    ctx.logger.info("Mow", "timetable stop => DOCK");
    changeOp(ctx, ctx.opMgr.dock());
}

void MowOp::onKidnapped(OpContext& ctx, bool state) {
    if (state) {
        ctx.stopMotors();
        ctx.logger.warn("Mow", "GPS kidnap => GpsWait");
        changeOp(ctx, ctx.opMgr.gpsWait(), true);
    }
}

void MowOp::onImuTilt(OpContext& ctx) {
    ctx.logger.error("Mow", "IMU tilt => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void MowOp::onImuError(OpContext& ctx) {
    ctx.logger.error("Mow", "IMU error => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

} // namespace sunray
