// UndockOp.cpp — leave charger safely before navigating to the mow start.
#include "core/op/Op.h"

#include <cmath>

namespace sunray {

void UndockOp::begin(OpContext& ctx) {
    ctx.logger.info("Undock", "OP_UNDOCK");
    ctx.stopMotors();
    startX = ctx.x;
    startY = ctx.y;
    chargerDropped = !ctx.battery.chargerConnected;
}

void UndockOp::end(OpContext& ctx) {
    ctx.stopMotors();
}

void UndockOp::run(OpContext& ctx) {
    const float speed = ctx.config.get<float>("undock_speed_ms", 0.08f);
    const float distanceM = ctx.config.get<float>("undock_distance_m", 0.32f);
    const unsigned long chargerTimeoutMs = static_cast<unsigned long>(
        ctx.config.get<int>("undock_charger_timeout_ms", 5000));
    const unsigned long positionTimeoutMs = static_cast<unsigned long>(
        ctx.config.get<int>("undock_position_timeout_ms", 10000));

    const float dx = ctx.x - startX;
    const float dy = ctx.y - startY;
    const float travelled = std::sqrt(dx * dx + dy * dy);

    if (!ctx.battery.chargerConnected) chargerDropped = true;

    if (chargerDropped && travelled >= distanceM) {
        ctx.logger.info("Undock", "undock complete => NavToStart");
        ctx.stopMotors();
        if (initiatedByOperator) ctx.opMgr.navToStart().initiatedByOperator = true;
        changeOp(ctx, ctx.opMgr.navToStart());
        return;
    }

    if (!chargerDropped && ctx.now_ms - startTime_ms >= chargerTimeoutMs) {
        ctx.logger.error("Undock", "charger still connected => ERROR");
        changeOp(ctx, ctx.opMgr.error());
        return;
    }

    if (ctx.now_ms - startTime_ms >= positionTimeoutMs) {
        ctx.logger.error("Undock", "undock position timeout => ERROR");
        changeOp(ctx, ctx.opMgr.error());
        return;
    }

    ctx.setLinearAngularSpeed(-speed, 0.0f);
}

void UndockOp::onObstacle(OpContext& ctx) {
    ctx.logger.error("Undock", "obstacle during undock => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void UndockOp::onStuck(OpContext& ctx) {
    ctx.logger.error("Undock", "stuck during undock => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void UndockOp::onLiftTriggered(OpContext& ctx) {
    ctx.logger.error("Undock", "lift sensor => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

void UndockOp::onMotorError(OpContext& ctx) {
    ctx.logger.error("Undock", "motor error => ERROR");
    changeOp(ctx, ctx.opMgr.error());
}

} // namespace sunray
