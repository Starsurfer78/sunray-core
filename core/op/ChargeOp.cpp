// ChargeOp.cpp — charging station management.
#include "core/op/Op.h"

namespace sunray {

static constexpr unsigned long CHARGE_RETRY_INTERVAL_MS = 10000;
static constexpr unsigned long CHARGE_COMPLETE_V        = 28.5f; // V — fully charged

void ChargeOp::begin(OpContext& ctx) {
    ctx.logger.info("Charge", "OP_CHARGE");
    ctx.stopMotors();  // stops drive + mow motors via setMotorPwm(0,0,0)
    retryTouchDock        = false;
    retryTime_ms          = 0;
    nextConsoleDetails_ms = ctx.now_ms + 30000;
    startTime_ms          = ctx.now_ms;
    // C.7: clear auto-detected obstacles — robot has returned to dock safely
    if (ctx.map) ctx.map->clearAutoObstacles();
}

void ChargeOp::end(OpContext&) {}

void ChargeOp::run(OpContext& ctx) {
    // If charger unexpectedly disconnected, handle via event.
    if (!ctx.battery.chargerConnected && ctx.now_ms - startTime_ms > 3000) {
        onChargerDisconnected(ctx);
        return;
    }

    // Log details periodically.
    if (ctx.now_ms >= nextConsoleDetails_ms) {
        ctx.logger.info("Charge",
            "V=" + std::to_string(ctx.battery.voltage) +
            " Ic=" + std::to_string(ctx.battery.chargeCurrent) + "A");
        nextConsoleDetails_ms = ctx.now_ms + 30000;
    }

    // Charging complete: high voltage + near-zero current.
    if (ctx.battery.voltage >= CHARGE_COMPLETE_V && ctx.battery.chargeCurrent < 0.1f) {
        if (ctx.now_ms - startTime_ms > 60000) {  // at least 60s in charge
            onChargingCompleted(ctx);
        }
    }
}

void ChargeOp::onChargerDisconnected(OpContext& ctx) {
    ctx.logger.info("Charge", "charger disconnected => IDLE");
    changeOp(ctx, ctx.opMgr.idle());
}

void ChargeOp::onBadChargingContact(OpContext& ctx) {
    // Brief retry: creep forward slightly, then stop.
    ctx.logger.warn("Charge", "bad charging contact — retry touch");
    retryTouchDock = true;
    retryTime_ms   = ctx.now_ms + 500;
    ctx.setLinearAngularSpeed(0.02f, 0.0f);  // very slow creep
}

void ChargeOp::onBatteryUndervoltage(OpContext& ctx) {
    ctx.logger.error("Charge", "battery undervoltage while charging!");
    changeOp(ctx, ctx.opMgr.error());
}

void ChargeOp::onRainTriggered(OpContext& ctx) {
    ctx.logger.info("Charge", "rain — stay in CHARGE");
    // Stay docked during rain regardless of timetable.
    (void)ctx;
}

void ChargeOp::onChargerConnected(OpContext& ctx) {
    ctx.logger.info("Charge", "charger reconnected");
    (void)ctx;
}

void ChargeOp::onTimetableStartMowing(OpContext& ctx) {
    // Timetable says start — only leave dock if battery is sufficient.
    const float minV = ctx.config.get<float>("battery_low_v", 21.0f);
    if (ctx.battery.voltage < minV) {
        ctx.logger.warn("Charge", "timetable start — battery too low, staying");
        return;
    }
    ctx.logger.info("Charge", "timetable start => MOW");
    changeOp(ctx, ctx.opMgr.mow());
}

void ChargeOp::onTimetableStopMowing(OpContext& ctx) {
    ctx.logger.info("Charge", "timetable stop (already charging)");
    (void)ctx;
}

void ChargeOp::onChargingCompleted(OpContext& ctx) {
    ctx.logger.info("Charge", "charging complete");
    (void)ctx;
    // Stay in CHARGE until operator or timetable commands mowing.
}

} // namespace sunray
