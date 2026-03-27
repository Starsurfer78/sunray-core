// DockOp.cpp — drive to docking station.
#include "core/op/Op.h"
#include "core/navigation/Map.h"
#include "core/navigation/LineTracker.h"

namespace sunray {

static constexpr int MAX_ROUTING_FAILURES = 5;

void DockOp::begin(OpContext& ctx) {
    ctx.logger.info("Dock", "OP_DOCK");
    lastMapRoutingFailed    = false;
    mapRoutingFailedCounter = 0;
    ctx.stopMotors();

    if (ctx.map && !ctx.map->startDocking(ctx.x, ctx.y)) {
        ctx.logger.error("Dock", "cannot build dock route => ERROR");
        changeOp(ctx, ctx.opMgr.error());
        return;
    }
    if (ctx.lineTracker) ctx.lineTracker->reset();
}

void DockOp::end(OpContext&) {}

void DockOp::run(OpContext& ctx) {
    // If charger connected, we've arrived.
    if (ctx.battery.chargerConnected) {
        onChargerConnected(ctx);
        return;
    }
    if (ctx.lineTracker && ctx.map && ctx.stateEst) {
        ctx.lineTracker->track(ctx, *ctx.map, *ctx.stateEst);
    }
}

void DockOp::onObstacle(OpContext& ctx) {
    ctx.logger.warn("Dock", "obstacle during dock => EscapeReverse");
    changeOp(ctx, ctx.opMgr.escape(), true);
}

void DockOp::onLiftTriggered(OpContext& ctx) {
    ctx.logger.error("Dock", "lift sensor => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void DockOp::onMotorError(OpContext& ctx) {
    ctx.logger.error("Dock", "motor error => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void DockOp::onTargetReached(OpContext& ctx) {
    ctx.logger.info("Dock", "dock waypoint reached");
    lastMapRoutingFailed = false;
}

void DockOp::onNoFurtherWaypoints(OpContext& ctx) {
    const int maxRetries = std::clamp(
        ctx.config.get<int>("dock_retry_max_attempts", MAX_ROUTING_FAILURES),
        1,
        MAX_ROUTING_FAILURES);

    if (mapRoutingFailedCounter >= maxRetries) {
        ctx.logger.error("Dock", "dock contact failed after retries => ERROR");
        changeOp(ctx, ctx.opMgr.error());
        return;
    }

    if (!ctx.map || !ctx.map->retryDocking(ctx.x, ctx.y)) {
        ctx.logger.error("Dock", "dock retry routing failed => ERROR");
        changeOp(ctx, ctx.opMgr.error());
        return;
    }

    ++mapRoutingFailedCounter;
    lastMapRoutingFailed = false;
    ctx.stopMotors();
    if (ctx.lineTracker) ctx.lineTracker->reset();
    ctx.logger.warn("Dock",
        "dock path exhausted without charger contact => retry "
        + std::to_string(mapRoutingFailedCounter) + "/"
        + std::to_string(maxRetries));
}

void DockOp::onGpsFixTimeout(OpContext& ctx) {
    ctx.logger.error("Dock", "GPS fix timeout during dock => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void DockOp::onGpsNoSignal(OpContext& ctx) {
    ctx.logger.warn("Dock", "GPS lost during dock => GpsWait");
    changeOp(ctx, ctx.opMgr.gpsWait(), true);
}

void DockOp::onChargerConnected(OpContext& ctx) {
    ctx.logger.info("Dock", "charger connected => CHARGE");
    ctx.stopMotors();
    changeOp(ctx, ctx.opMgr.charge());
}

void DockOp::onKidnapped(OpContext& ctx, bool state) {
    if (state) {
        ctx.stopMotors();
        ctx.logger.warn("Dock", "GPS kidnap during dock => GpsWait");
        changeOp(ctx, ctx.opMgr.gpsWait(), true);
    }
}

} // namespace sunray
