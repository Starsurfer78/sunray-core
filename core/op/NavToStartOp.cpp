// NavToStartOp.cpp — drive to the first mowing waypoint with blade off.
#include "core/op/Op.h"
#include "core/navigation/LineTracker.h"
#include "core/navigation/Map.h"

namespace sunray {

void NavToStartOp::begin(OpContext& ctx) {
    ctx.logger.info("NavToStart", "OP_NAV_TO_START");
    ctx.stopMotors();

    const bool alreadyTrackingMow = ctx.map && ctx.map->wayMode == nav::WayType::MOW;
    if (ctx.map && !alreadyTrackingMow && !ctx.map->startMowing(ctx.x, ctx.y)) {
        ctx.logger.error("NavToStart", "cannot build mowing route => IDLE");
        changeOp(ctx, ctx.opMgr.idle());
        return;
    }
    if (ctx.lineTracker) ctx.lineTracker->reset();
}

void NavToStartOp::end(OpContext& ctx) {
    ctx.stopMotors();
}

void NavToStartOp::run(OpContext& ctx) {
    if (ctx.lineTracker && ctx.map && ctx.stateEst) {
        ctx.lineTracker->track(ctx, *ctx.map, *ctx.stateEst);
    }
}

void NavToStartOp::onTargetReached(OpContext& ctx) {
    ctx.logger.info("NavToStart", "start waypoint reached => MOW");
    if (initiatedByOperator) ctx.opMgr.mow().initiatedByOperator = true;
    changeOp(ctx, ctx.opMgr.mow());
}

void NavToStartOp::onNoFurtherWaypoints(OpContext& ctx) {
    ctx.logger.warn("NavToStart", "no further start waypoints => MOW");
    if (initiatedByOperator) ctx.opMgr.mow().initiatedByOperator = true;
    changeOp(ctx, ctx.opMgr.mow());
}

void NavToStartOp::onGpsNoSignal(OpContext& ctx) {
    ctx.logger.warn("NavToStart", "GPS signal lost => GpsWait");
    changeOp(ctx, ctx.opMgr.gpsWait(), true);
}

void NavToStartOp::onGpsFixTimeout(OpContext& ctx) {
    ctx.logger.error("NavToStart", "GPS fix timeout => DOCK");
    changeOp(ctx, ctx.opMgr.dock());
}

void NavToStartOp::onObstacle(OpContext& ctx) {
    ctx.logger.info("NavToStart", "obstacle => EscapeReverse");
    changeOp(ctx, ctx.opMgr.escape(), true);
}

void NavToStartOp::onLiftTriggered(OpContext& ctx) {
    ctx.logger.error("NavToStart", "lift sensor => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void NavToStartOp::onMotorError(OpContext& ctx) {
    ctx.logger.error("NavToStart", "motor error => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void NavToStartOp::onKidnapped(OpContext& ctx, bool state) {
    if (state) {
        ctx.stopMotors();
        ctx.logger.warn("NavToStart", "GPS kidnap => GpsWait");
        changeOp(ctx, ctx.opMgr.gpsWait(), true);
    }
}

void NavToStartOp::onImuTilt(OpContext& ctx) {
    ctx.logger.error("NavToStart", "IMU tilt => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void NavToStartOp::onImuError(OpContext& ctx) {
    ctx.logger.error("NavToStart", "IMU error => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

} // namespace sunray
