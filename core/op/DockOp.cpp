// DockOp.cpp — drive to docking station.
#include "core/op/Op.h"
#include "core/navigation/Map.h"
#include "core/navigation/LineTracker.h"
#include "core/navigation/TransitionRouter.h"
#include "core/navigation/RuntimeState.h"

#include <cmath>

namespace sunray
{

    static constexpr int MAX_ROUTING_FAILURES = 5;
    static constexpr int MAX_NON_PROGRESSIVE_RETRIES = 2;
    static constexpr float MIN_RETRY_PROGRESS_M = 0.15f;

    void DockOp::begin(OpContext &ctx)
    {
        ctx.logger.info("Dock", "OP_DOCK");
        mapRoutingFailedCounter = 0;
        nonProgressiveRetryCounter = 0;
        retryStartX = ctx.x;
        retryStartY = ctx.y;
        ctx.stopMotors();

        if (ctx.map && ctx.runtimeState && !ctx.runtimeState->startDocking(*ctx.map, ctx.x, ctx.y))
        {
            ctx.logger.error("Dock", "cannot build dock route => ERROR");
            changeOp(ctx, ctx.opMgr.error());
            return;
        }

        // N2.5: If TransitionRouter finds a better obstacle-aware approach path,
        // override the approach segment (dock state already set by startDocking).
        if (ctx.map && ctx.runtimeState)
        {
            const auto &dockRoute = ctx.map->dockRoutePlan();
            if (!dockRoute.points.empty())
            {
                nav::TransitionRouterOptions opts;
                opts.allowOutsidePerimeter = true;
                const auto path = nav::TransitionRouter::plan(
                    nav::Point{ctx.x, ctx.y},
                    dockRoute.points.front().p,
                    *ctx.map, opts);
                if (!path.empty())
                    ctx.runtimeState->injectFreePath(*ctx.map, path);
            }
        }

        if (ctx.lineTracker)
            ctx.lineTracker->reset();
    }

    void DockOp::end(OpContext &) {}

    void DockOp::run(OpContext &ctx)
    {
        // If charger connected, we've arrived.
        if (ctx.battery.chargerConnected)
        {
            onChargerConnected(ctx);
            return;
        }
        if (ctx.lineTracker && ctx.map && ctx.runtimeState && ctx.stateEst)
        {
            ctx.lineTracker->track(ctx, *ctx.map, *ctx.runtimeState, *ctx.stateEst);
        }
    }

    void DockOp::onObstacle(OpContext &ctx)
    {
        ctx.logger.warn("Dock", "obstacle during dock => EscapeReverse");
        changeOp(ctx, ctx.opMgr.escape(), true);
    }

    void DockOp::onStuck(OpContext &ctx)
    {
        ctx.logger.warn("Dock", "stuck during dock => EscapeReverse");
        changeOp(ctx, ctx.opMgr.escape(), true);
    }

    void DockOp::onLiftTriggered(OpContext &ctx)
    {
        ctx.logger.error("Dock", "lift sensor => ERROR");
        changeOp(ctx, ctx.opMgr.error());
    }

    void DockOp::onMotorError(OpContext &ctx)
    {
        ctx.logger.error("Dock", "motor error => ERROR");
        changeOp(ctx, ctx.opMgr.error());
    }

    void DockOp::onTargetReached(OpContext &ctx)
    {
        ctx.logger.info("Dock", "dock waypoint reached");
    }

    void DockOp::onNoFurtherWaypoints(OpContext &ctx)
    {
        const int maxRetries = std::clamp(
            ctx.config.get<int>("dock_retry_max_attempts", MAX_ROUTING_FAILURES),
            1,
            MAX_ROUTING_FAILURES);
        const int nextRetryNumber = mapRoutingFailedCounter + 1;
        const float dx = ctx.x - retryStartX;
        const float dy = ctx.y - retryStartY;
        const float progressM = std::sqrt(dx * dx + dy * dy);

        if (progressM < MIN_RETRY_PROGRESS_M)
        {
            ++nonProgressiveRetryCounter;
            if (nonProgressiveRetryCounter >= MAX_NON_PROGRESSIVE_RETRIES)
            {
                ctx.logger.error("Dock", "dock retry made no progress => ERROR");
                changeOp(ctx, ctx.opMgr.error());
                return;
            }
        }
        else
        {
            nonProgressiveRetryCounter = 0;
        }

        if (mapRoutingFailedCounter >= maxRetries)
        {
            ctx.logger.error("Dock", "dock contact failed after retries => ERROR");
            changeOp(ctx, ctx.opMgr.error());
            return;
        }

        float lateralOffsetM = 0.0f;
        const float configuredOffset = std::fabs(
            ctx.config.get<float>("dock_retry_lateral_offset_m", 0.10f));
        if (nextRetryNumber == 3)
            lateralOffsetM = configuredOffset;
        if (nextRetryNumber == 4)
            lateralOffsetM = -configuredOffset;

        if (!ctx.map || !ctx.runtimeState || !ctx.runtimeState->retryDocking(*ctx.map, ctx.x, ctx.y, lateralOffsetM))
        {
            ctx.logger.error("Dock", "dock retry routing failed => ERROR");
            changeOp(ctx, ctx.opMgr.error());
            return;
        }

        ++mapRoutingFailedCounter;
        retryStartX = ctx.x;
        retryStartY = ctx.y;
        ctx.stopMotors();
        if (ctx.lineTracker)
            ctx.lineTracker->reset();
        ctx.logger.warn("Dock",
                        "dock path exhausted without charger contact => retry " + std::to_string(mapRoutingFailedCounter) + "/" + std::to_string(maxRetries) + (std::fabs(lateralOffsetM) > 1e-4f ? " with lateral offset " + std::to_string(lateralOffsetM) + "m" : ""));
    }

    void DockOp::onGpsFixTimeout(OpContext &ctx)
    {
        ctx.logger.error("Dock", "GPS fix timeout during dock => ERROR");
        changeOp(ctx, ctx.opMgr.error());
    }

    void DockOp::onGpsNoSignal(OpContext &ctx)
    {
        ctx.logger.warn("Dock", "GPS lost during dock => GpsWait");
        changeOp(ctx, ctx.opMgr.gpsWait(), true);
    }

    void DockOp::onChargerConnected(OpContext &ctx)
    {
        ctx.logger.info("Dock", "charger connected => CHARGE");
        ctx.stopMotors();
        changeOp(ctx, ctx.opMgr.charge());
    }

    void DockOp::onKidnapped(OpContext &ctx, bool state)
    {
        if (state)
        {
            ctx.stopMotors();
            ctx.logger.warn("Dock", "GPS kidnap during dock => GpsWait");
            changeOp(ctx, ctx.opMgr.gpsWait(), true);
        }
    }

    void DockOp::onPerimeterViolated(OpContext &ctx)
    {
        ctx.stopMotors();
        ctx.logger.error("Dock", "perimeter violated during docking => ERROR");
        changeOp(ctx, ctx.opMgr.error());
    }

    void DockOp::onMapChanged(OpContext &ctx)
    {
        ctx.stopMotors();
        ctx.logger.warn("Dock", "map changed during docking => IDLE");
        changeOp(ctx, ctx.opMgr.idle());
    }

    void DockOp::onBatteryUndervoltage(OpContext &ctx)
    {
        ctx.stopMotors();
        ctx.logger.error("Dock", "battery undervoltage during docking => ERROR");
        changeOp(ctx, ctx.opMgr.error());
    }

    void DockOp::onWatchdogTimeout(OpContext &ctx)
    {
        ctx.stopMotors();
        ctx.logger.error("Dock", "dock watchdog timeout => ERROR");
        changeOp(ctx, ctx.opMgr.error());
    }

    unsigned long DockOp::watchdogTimeoutMs(const OpContext &ctx) const
    {
        return static_cast<unsigned long>(ctx.config.get<int>("dock_max_duration_ms", 180000));
    }

} // namespace sunray
