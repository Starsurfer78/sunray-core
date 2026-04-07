// ChargeOp.cpp — charging station management.
#include "core/op/Op.h"
#include "core/navigation/Map.h"

namespace sunray
{

    static constexpr float CHARGE_COMPLETE_V = 28.5f; // V — fully charged

    void ChargeOp::begin(OpContext &ctx)
    {
        ctx.logger.info("Charge", "OP_CHARGE");
        ctx.stopMotors(); // stops drive + mow motors via setMotorPwm(0,0,0)
        retryTouchDock = false;
        retryTime_ms = 0;
        contactRetryCount = 0;
        chargingComplete_ = false;
        nextConsoleDetails_ms = ctx.now_ms + 30000;
        startTime_ms = ctx.now_ms;
        // C.7: clear auto-detected obstacles — robot has returned to dock safely
        if (ctx.map)
            ctx.map->clearAutoObstacles();
    }

    void ChargeOp::end(OpContext &) {}

    void ChargeOp::run(OpContext &ctx)
    {
        const unsigned long contactTimeoutMs = static_cast<unsigned long>(
            ctx.config.get<int>("dock_retry_contact_timeout_ms", 3000));
        if (retryTouchDock)
        {
            if (ctx.battery.chargerConnected)
            {
                onChargerConnected(ctx);
                return;
            }
            if (ctx.now_ms < retryTime_ms)
            {
                const float creepSpeed = std::min(
                    ctx.config.get<float>("dock_linear_speed_ms", 0.1f),
                    0.05f);
                ctx.setLinearAngularSpeed(creepSpeed, 0.0f);
                return;
            }

            retryTouchDock = false;
            ctx.stopMotors();
            if (ctx.battery.chargerConnected)
            {
                onChargerConnected(ctx);
                return;
            }
            ctx.logger.warn("Charge", "touch retry did not restore contact => DOCK");
            onChargerDisconnected(ctx);
            return;
        }

        // If charger unexpectedly disconnected, first try to recover a weak contact.
        if (!ctx.battery.chargerConnected)
        {
            if (ctx.now_ms - startTime_ms <= contactTimeoutMs)
            {
                onBadChargingContact(ctx);
                return;
            }
            onChargerDisconnected(ctx);
            return;
        }

        // Log details periodically.
        if (ctx.now_ms >= nextConsoleDetails_ms)
        {
            ctx.logger.info("Charge",
                            "V=" + std::to_string(ctx.battery.voltage) +
                                " Ic=" + std::to_string(ctx.battery.chargeCurrent) + "A");
            nextConsoleDetails_ms = ctx.now_ms + 30000;
        }

        // Charging complete: high voltage + near-zero current.
        if (!chargingComplete_ &&
            ctx.battery.voltage >= CHARGE_COMPLETE_V &&
            ctx.battery.chargeCurrent < 0.1f)
        {
            if (ctx.now_ms - startTime_ms > 60000)
            { // at least 60s in charge
                chargingComplete_ = true;
                onChargingCompleted(ctx);
            }
        }
    }

    void ChargeOp::onChargerDisconnected(OpContext &ctx)
    {
        ctx.logger.warn("Charge", "charger disconnected unexpectedly => DOCK");
        ctx.stopMotors();
        changeOp(ctx, ctx.opMgr.dock());
    }

    void ChargeOp::onBadChargingContact(OpContext &ctx)
    {
        if (retryTouchDock)
            return;

        const int maxRetries = std::max(1, ctx.config.get<int>("dock_retry_max_attempts", 3));
        if (contactRetryCount >= maxRetries)
        {
            ctx.logger.error("Charge", "charging contact failed after retries => ERROR");
            changeOp(ctx, ctx.opMgr.error());
            return;
        }

        const unsigned long touchApproachMs = static_cast<unsigned long>(
            ctx.config.get<int>("dock_retry_approach_ms", 2000));

        ++contactRetryCount;
        ctx.logger.warn("Charge",
                        "bad charging contact — retry touch " + std::to_string(contactRetryCount) + "/" + std::to_string(maxRetries));
        retryTouchDock = true;
        retryTime_ms = ctx.now_ms + touchApproachMs;
        ctx.setLinearAngularSpeed(0.02f, 0.0f); // very slow creep
    }

    void ChargeOp::onBatteryUndervoltage(OpContext &ctx)
    {
        ctx.logger.error("Charge", "battery undervoltage while charging!");
        changeOp(ctx, ctx.opMgr.error());
    }

    void ChargeOp::onRainTriggered(OpContext &ctx)
    {
        ctx.logger.info("Charge", "rain — stay in CHARGE");
        // Stay docked during rain regardless of timetable.
        (void)ctx;
    }

    void ChargeOp::onChargerConnected(OpContext &ctx)
    {
        ctx.logger.info("Charge", "charger reconnected");
        retryTouchDock = false;
        retryTime_ms = 0;
        startTime_ms = ctx.now_ms;
        ctx.stopMotors();
    }

    void ChargeOp::onTimetableStartMowing(OpContext &ctx)
    {
        // Timetable says start — only leave dock if battery is sufficient.
        const float minV = ctx.config.get<float>("battery_low_v", 21.0f);
        if (ctx.battery.voltage < minV)
        {
            ctx.logger.warn("Charge", "timetable start — battery too low, staying");
            return;
        }
        if (ctx.map && !ctx.map->startMowing(ctx.x, ctx.y))
        {
            ctx.logger.error("Charge", "timetable start — no active mowing route available");
            return;
        }
        ctx.logger.info("Charge", "timetable start => UNDOCK");
        changeOp(ctx, ctx.opMgr.undock());
    }

    void ChargeOp::onTimetableStopMowing(OpContext &ctx)
    {
        ctx.logger.info("Charge", "timetable stop (already charging)");
        (void)ctx;
    }

    void ChargeOp::onChargingCompleted(OpContext &ctx)
    {
        ctx.logger.info("Charge", "charging complete");
        (void)ctx;
        // Stay in CHARGE until operator or timetable commands mowing.
    }

} // namespace sunray
