// EscapeReverseOp.cpp — reverse away from obstacle.
#include "core/op/Op.h"
#include "core/navigation/GridMap.h"

namespace sunray {

static constexpr unsigned long ESCAPE_REVERSE_MS = 3000;

void EscapeReverseOp::begin(OpContext& ctx) {
    ctx.logger.info("EscapeReverse", "begin");
    hitLeft  = ctx.sensors.bumperLeft;
    hitRight = ctx.sensors.bumperRight;
    driveReverseStopTime_ms = ctx.now_ms + ESCAPE_REVERSE_MS;
    // C.7: record obstacle at current robot position for future path avoidance
    if (ctx.map) ctx.map->addObstacle(ctx.x, ctx.y, ctx.now_ms);
}

void EscapeReverseOp::end(OpContext&) {}

void EscapeReverseOp::run(OpContext& ctx) {
    // Boundary guard: stop if reversing takes robot outside perimeter.
    if (!ctx.insidePerimeter) {
        ctx.logger.warn("EscapeReverse", "outside perimeter during escape — docking");
        ctx.stopMotors();
        changeOp(ctx, ctx.opMgr.dock());
        return;
    }

    // Steer away from the bumper side that was hit (TASK-BUMPER-03 logic).
    float escapeAngular = 0.0f;
    if      (hitLeft  && !hitRight) escapeAngular = -0.15f;  // hit left → steer right
    else if (hitRight && !hitLeft)  escapeAngular =  0.15f;  // hit right → steer left
    ctx.setLinearAngularSpeed(-0.1f, escapeAngular);

    if (ctx.now_ms >= driveReverseStopTime_ms) {
        ctx.stopMotors();
        driveReverseStopTime_ms = 0;

        if (ctx.sensors.lift) {
            ctx.logger.error("EscapeReverse", "lift sensor after escape => ERROR");
            changeOp(ctx, ctx.opMgr.error());
            return;
        }
        if (!ctx.insidePerimeter) {
            ctx.logger.warn("EscapeReverse", "still outside perimeter => DOCK");
            changeOp(ctx, ctx.opMgr.dock());
            return;
        }
        // Continue with previous operation.
        if (nextOp != nullptr) {
            ctx.logger.info("EscapeReverse", "escape done => continue " + nextOp->name());
            // E.2-c: Route around obstacle using GridMap A* before resuming mission.
            if (ctx.map && (ctx.map->wayMode == nav::WayType::MOW || ctx.map->wayMode == nav::WayType::DOCK)) {
                const nav::Point robotPos{ctx.x, ctx.y};
                const nav::Point target = ctx.map->targetPoint;

                // Build local occupancy grid and plan path
                nav::GridMap gridMap;
                gridMap.build(*ctx.map, ctx.x, ctx.y);
                auto path = gridMap.planPath(robotPos, target);

                if (!path.empty()) {
                    // Smooth and inject as FREE waypoints
                    path = gridMap.smoothPath(path);
                    ctx.map->injectFreePath(path);
                    ctx.logger.info("EscapeReverse",
                        "A* path found (" + std::to_string(path.size()) + " waypoints)");
                } else {
                    // Fallback: legacy iterative detour routing (C.7)
                    bool replanned = false;
                    if (ctx.map->wayMode == nav::WayType::DOCK) {
                        replanned = ctx.map->startDocking(ctx.x, ctx.y);
                    } else if (ctx.map->wayMode == nav::WayType::MOW) {
                        replanned = ctx.map->startMowing(ctx.x, ctx.y);
                    }

                    if (replanned) {
                        ctx.logger.info("EscapeReverse", "A* failed — fallback to legacy routing");
                    }
                }
            }
            changeOp(ctx, *nextOp, false);
        } else {
            ctx.logger.info("EscapeReverse", "escape done, no nextOp => IDLE");
            changeOp(ctx, ctx.opMgr.idle());
        }
    }
}

void EscapeReverseOp::onKidnapped(OpContext& ctx, bool state) {
    if (state) {
        ctx.stopMotors();
        changeOp(ctx, ctx.opMgr.gpsWait(), true);
    }
}

void EscapeReverseOp::onImuTilt(OpContext& ctx) {
    changeOp(ctx, ctx.opMgr.error());
}

void EscapeReverseOp::onImuError(OpContext& ctx) {
    changeOp(ctx, ctx.opMgr.error());
}

// ── EscapeForwardOp ───────────────────────────────────────────────────────────

static constexpr unsigned long ESCAPE_FORWARD_MS = 2000;

void EscapeForwardOp::begin(OpContext& ctx) {
    ctx.logger.info("EscapeForward", "begin");
    driveForwardStopTime_ms = ctx.now_ms + ESCAPE_FORWARD_MS;
}

void EscapeForwardOp::end(OpContext&) {}

void EscapeForwardOp::run(OpContext& ctx) {
    ctx.setLinearAngularSpeed(0.1f, 0.0f);

    if (ctx.now_ms >= driveForwardStopTime_ms) {
        ctx.stopMotors();
        if (nextOp != nullptr) {
            changeOp(ctx, *nextOp, false);
        } else {
            changeOp(ctx, ctx.opMgr.idle());
        }
    }
}

void EscapeForwardOp::onKidnapped(OpContext& ctx, bool state) {
    if (state) {
        ctx.stopMotors();
        changeOp(ctx, ctx.opMgr.gpsWait(), true);
    }
}

void EscapeForwardOp::onImuTilt(OpContext& ctx) {
    changeOp(ctx, ctx.opMgr.error());
}

void EscapeForwardOp::onImuError(OpContext& ctx) {
    changeOp(ctx, ctx.opMgr.error());
}

} // namespace sunray
