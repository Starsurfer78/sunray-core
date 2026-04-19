// LineTracker.cpp — Stanley path-tracking controller.
//
// Ported from sunray/LineTracker.cpp — trackLine() function.
// Removed: GPS-speed obstacle detection, localization-mode branches (April-tag,
//   Reflector-tag, Guidance-sheet), LiDAR/sonar slow-down checks.
// Kept: Stanley formula, rotation phase, kidnap detection, target progression.
//
// All motor output:  ctx.setLinearAngularSpeed(linear, angular)
// All Op callbacks:  ctx.opMgr.activeOp()->on*()

#include "core/navigation/LineTracker.h"
#include "core/navigation/Map.h"
#include "core/navigation/RuntimeState.h"
#include "core/navigation/StateEstimator.h"
#include "core/op/Op.h"

#include <algorithm>
#include <cmath>

namespace sunray
{
    namespace nav
    {

        static constexpr float TWO_PI = 2.0f * static_cast<float>(M_PI);

        // ── Construction ──────────────────────────────────────────────────────────────

        LineTracker::LineTracker(std::shared_ptr<Config> config)
            : config_(std::move(config))
        {
        }

        // ── Reset ─────────────────────────────────────────────────────────────────────

        void LineTracker::reset()
        {
            rotateLeft_ = false;
            rotateRight_ = false;
            angleToTargetFits_ = false;
            stateKidnapped_ = false;
            lateralError_ = 0.0f;
            targetDist_ = 0.0f;
            trackerDiffDelta_ = 0.0f;
            rotBlockArmed_ = false;
            rotBlockStartMs_ = 0;
            rotBlockImuYawStart_ = 0.0f;
        }

        // ── Geometry helpers ──────────────────────────────────────────────────────────

        float LineTracker::scalePI(float v)
        {
            float r = std::fmod(v, TWO_PI);
            if (r > PI)
                r -= TWO_PI;
            if (r < -PI)
                r += TWO_PI;
            return r;
        }

        float LineTracker::distancePI(float a, float b)
        {
            float d = scalePI(b - a);
            if (d < -PI)
                d += TWO_PI;
            if (d > PI)
                d -= TWO_PI;
            return d;
        }

        float LineTracker::scalePIangles(float setAngle, float currAngle)
        {
            return scalePI(setAngle) + std::round((currAngle - scalePI(setAngle)) / TWO_PI) * TWO_PI;
        }

        float LineTracker::distanceLineInfinite(float px, float py,
                                                float x1, float y1,
                                                float x2, float y2)
        {
            // Signed cross-track distance; sign: positive = left of line direction.
            const float dx = x2 - x1;
            const float dy = y2 - y1;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6f)
                return 0.0f;
            return (dy * (px - x1) - dx * (py - y1)) / len;
        }

        float LineTracker::distanceLine(float px, float py,
                                        float x1, float y1,
                                        float x2, float y2)
        {
            const float dx = x2 - x1;
            const float dy = y2 - y1;
            const float len2 = dx * dx + dy * dy;
            if (len2 < 1e-12f)
            {
                const float ex = px - x1, ey = py - y1;
                return std::sqrt(ex * ex + ey * ey);
            }
            const float t = std::clamp(((px - x1) * dx + (py - y1) * dy) / len2, 0.0f, 1.0f);
            const float cx = x1 + t * dx - px;
            const float cy = y1 + t * dy - py;
            return std::sqrt(cx * cx + cy * cy);
        }

        // ── Main tracking ─────────────────────────────────────────────────────────────

        void LineTracker::track(OpContext &ctx, Map &map, RuntimeState &runtime, const StateEstimator &estimator)
        {
            Op *activeOp = ctx.opMgr.activeOp();

            const float stateX = estimator.x();
            const float stateY = estimator.y();
            const float stateDelta = estimator.heading();
            const float speed = estimator.groundSpeed();

            if (runtime.refreshTracking(map, ctx.now_ms, stateX, stateY))
            {
                return;
            }

            const RouteSegment segment = runtime.currentTrackingSegment(map, stateX, stateY, stateDelta);
            const Point &target = segment.target;
            const Point &lastTarget = segment.lastTarget;

            // ── Heading error to target ───────────────────────────────────────────────

            float targetDelta = segment.hasTargetHeading
                                    ? segment.targetHeadingRad
                                    : std::atan2(target.y - stateY, target.x - stateX);
            if (segment.reverse)
                targetDelta = scalePI(targetDelta + PI);
            targetDelta = scalePIangles(targetDelta, stateDelta);
            trackerDiffDelta_ = distancePI(stateDelta, targetDelta);

            // ── Cross-track (lateral) error ───────────────────────────────────────────

            lateralError_ = distanceLineInfinite(stateX, stateY,
                                                 lastTarget.x, lastTarget.y,
                                                 target.x, target.y);

            const float distToPath = distanceLine(stateX, stateY,
                                                  lastTarget.x, lastTarget.y,
                                                  target.x, target.y);

            // ── Target-distance ───────────────────────────────────────────────────────

            targetDist_ = segment.distanceToTarget_m;
            const float lastTargetDist = segment.distanceToLastTarget_m;

            const bool targetReached = segment.targetReached;

            // ── Angle check: rotate or track? ─────────────────────────────────────────
            //   Allow rotation only near waypoints or when far off-path (as in original).

            if ((targetDist_ < 0.5f) || (lastTargetDist < 0.5f) || (distToPath > 0.5f) || rotateLeft_ || rotateRight_)
            {
                // Hysteresis stop-threshold: only exit rotation when well-aligned (< 20°).
                // Previously 120° — that let Stanley take over at 90–120° error, which
                // immediately cleared the rotation flags, causing the rotate↔Stanley
                // oscillation that made the robot thrash wildly.
                angleToTargetFits_ = (std::fabs(trackerDiffDelta_) / PI * 180.0f < 20.0f);
            }
            else
            {
                angleToTargetFits_ = (std::fabs(trackerDiffDelta_) / PI * 180.0f < 45.0f);
            }

            // ── Config snapshot (read once per tick to avoid repeated map lookups) ───
            //   All config values used below are read here so the if/else branches
            //   below stay readable and perform only a single lookup per key per tick.

            const float setSpeed = config_ ? config_->get<float>("motor_set_speed_ms", 0.3f) : 0.3f;

            const bool rotRampEnabled = config_ ? config_->get<bool>("rotation_ramp_enabled", true) : true;
            const float rampMaxDegS = config_ ? config_->get<float>("rotation_ramp_max_deg_s", 75.0f) : 75.0f;
            const float rampMinDegS = config_ ? config_->get<float>("rotation_ramp_min_deg_s", 18.0f) : 18.0f;

            const bool rotBlockEnabled = config_ ? config_->get<bool>("obstacle_detect_rotation_enabled", false) : false;
            const unsigned long rotBlockTimeoutMs = static_cast<unsigned long>(
                config_ ? config_->get<int>("obstacle_detect_rotation_timeout_ms", 3000) : 3000);
            const float rotBlockMinYawDeg = config_ ? config_->get<float>("obstacle_detect_rotation_min_yaw_deg", 10.0f) : 10.0f;

            const bool adaptEnabled = config_ ? config_->get<bool>("adaptive_speed_enabled", false) : false;
            const float adaptScale = config_ ? config_->get<float>("adaptive_speed_overload_scale", 0.5f) : 0.5f;

            const float floatSpeedScale = config_ ? config_->get<float>("gps_float_speed_scale", 0.6f) : 0.6f;
            const unsigned long staleAgeMs = static_cast<unsigned long>(
                config_ ? config_->get<int>("gps_stale_age_ms", 2000) : 2000);
            const float staleSpeedScale = config_ ? config_->get<float>("gps_stale_speed_scale", 0.4f) : 0.4f;

            const float stanleyKSlow = config_ ? config_->get<float>("stanley_k_slow", 0.2f) : 0.2f;
            const float stanleyPSlow = config_ ? config_->get<float>("stanley_p_slow", 0.5f) : 0.5f;
            const float dockLinearSpeed = config_ ? config_->get<float>("dock_linear_speed_ms", 0.1f) : 0.1f;
            const float stanleyKNormal = config_ ? config_->get<float>("stanley_k_normal", 1.0f) : 1.0f;
            const float stanleyPNormal = config_ ? config_->get<float>("stanley_p_normal", 2.0f) : 2.0f;

            // ── Speed and angular control ─────────────────────────────────────────────

            float linear = 0.0f;
            float angular = 0.0f;

            if (!angleToTargetFits_)
            {
                // ── Rotate phase: no forward motion ─────────────────────────────────
                linear = 0.0f;

                // ROTATION_RAMP: scale angular speed linearly with remaining angle error.
                // Large error → ramp_max_deg_s, small error → ramp_min_deg_s.
                // Disabled by setting rotation_ramp_enabled=false → constant ROTATE_SPEED_RADPS.
                if (rotRampEnabled)
                {
                    const float angleDeg = std::fabs(trackerDiffDelta_) / PI * 180.0f;
                    // Linear ramp: full speed at 120°, min speed at 0°.
                    const float t = std::clamp(angleDeg / 120.0f, 0.0f, 1.0f);
                    angular = (rampMinDegS + t * (rampMaxDegS - rampMinDegS)) / 180.0f * PI;
                }
                else
                {
                    angular = ROTATE_SPEED_RADPS;
                }

                if (!rotateLeft_ && !rotateRight_)
                {
                    if (trackerDiffDelta_ < 0.0f)
                        rotateLeft_ = true;
                    else
                        rotateRight_ = true;
                }
                if (rotateLeft_)
                    angular *= -1.0f;

                // Once within 90° reset rotation bias (allow Stanley to take over).
                if (std::fabs(trackerDiffDelta_) / PI * 180.0f < 90.0f)
                {
                    rotateLeft_ = false;
                    rotateRight_ = false;
                }

                // ── OBSTACLE_DETECTION_ROTATION ──────────────────────────────────────
                // If enabled and IMU is valid: arm on first rotation tick, then monitor
                // whether yaw actually changes. If yaw moves less than threshold after
                // rot_block_timeout_ms → robot is blocked → fire onObstacleRotation().
                if (rotBlockEnabled && ctx.imuValid)
                {
                    if (!rotBlockArmed_)
                    {
                        rotBlockArmed_ = true;
                        rotBlockStartMs_ = ctx.now_ms;
                        rotBlockImuYawStart_ = ctx.imuYaw;
                    }
                    else
                    {
                        if (ctx.now_ms - rotBlockStartMs_ >= rotBlockTimeoutMs)
                        {
                            // Fix: use distancePI() to correctly handle ±π yaw wraparound.
                            const float yawChangeDeg =
                                std::fabs(distancePI(ctx.imuYaw, rotBlockImuYawStart_)) * 180.0f / PI;
                            rotBlockArmed_ = false;
                            rotBlockStartMs_ = 0;
                            if (yawChangeDeg < rotBlockMinYawDeg)
                            {
                                if (activeOp)
                                    activeOp->onObstacleRotation(ctx);
                            }
                        }
                    }
                }
                else
                {
                    // rotBlockEnabled=false OR imuValid=false — disarm so a future
                    // valid rotation phase starts fresh.
                    rotBlockArmed_ = false;
                }
            }
            else
            {
                // ── Stanley tracking phase ───────────────────────────────────────────
                rotateLeft_ = false;
                rotateRight_ = false;
                rotBlockArmed_ = false;

                float k, p;
                if (segment.slow)
                {
                    k = stanleyKSlow;
                    p = stanleyPSlow;
                    linear = dockLinearSpeed;
                }
                else
                {
                    k = stanleyKNormal;
                    p = stanleyPNormal;
                    linear = setSpeed;
                    // Reduce speed when approaching a non-straight (turning) waypoint.
                    if ((setSpeed > 0.2f) && (targetDist_ < 0.5f) && !segment.nextSegmentStraight)
                    {
                        linear = 0.1f;
                    }
                }

                // Stanley formula: p*headingError + atan2(k*crossTrack, 0.001+|speed|)
                angular = p * trackerDiffDelta_ + std::atan2(k * lateralError_, 0.001f + std::fabs(speed));

                // When reversing, flip linear AND angular steering polarity
                if (segment.reverse) {
                    linear *= -1.0f;
                }

                // ── ADAPTIVE_SPEED: reduce forward speed when mow motor is overloaded ─
                // Uses the STM32 overload flag (AT+S/AT+M summary field).
                // Disabled by default — enable via config: "adaptive_speed_enabled": true
                if (!segment.slow && !segment.reverse)
                {
                    if (adaptEnabled && ctx.sensors.mowOverload)
                    {
                        linear *= std::clamp(adaptScale, 0.1f, 1.0f);
                    }
                }
            }

            // ── GPS degradation mode ─────────────────────────────────────────────────
            // Keep navigating, but reduce commanded speed when GPS quality is degraded.
            // Only applies when we actually have forward/reverse motion to scale.
            if (linear != 0.0f && !ctx.gpsHasFix)
            {
                if (ctx.gpsHasFloat)
                {
                    linear *= std::clamp(floatSpeedScale, 0.1f, 1.0f);
                }
                if (ctx.gpsFixAge_ms >= staleAgeMs)
                {
                    linear *= std::clamp(staleSpeedScale, 0.1f, 1.0f);
                }
            }

            // ── Kidnap detection ──────────────────────────────────────────────────────
            //   If the robot is more than KIDNAP_TOLERANCE off the planned line,
            //   fire onKidnapped(true). Clear when back on path.

            if (std::fabs(distToPath) > segment.kidnapTolerance_m)
            {
                if (!stateKidnapped_)
                {
                    stateKidnapped_ = true;
                    if (activeOp)
                        activeOp->onKidnapped(ctx, true);
                }
            }
            else
            {
                if (stateKidnapped_)
                {
                    stateKidnapped_ = false;
                    if (activeOp)
                        activeOp->onKidnapped(ctx, false);
                }
            }

            // ── Apply motor command ───────────────────────────────────────────────────
            ctx.setLinearAngularSpeed(linear, angular);

            // ── Waypoint progression ──────────────────────────────────────────────────
            if (targetReached)
            {
                rotateLeft_ = false;
                rotateRight_ = false;
                if (activeOp)
                    activeOp->onTargetReached(ctx);
                // onTargetReached() may have triggered an Op transition (e.g. to Dock).
                // Only advance the map and fire onNoFurtherWaypoints() if the active Op
                // is still the same — otherwise the new Op manages its own tracking.
                Op *opAfterReached = ctx.opMgr.activeOp();
                if (opAfterReached == activeOp)
                {
                    if (!runtime.advanceTracking(map, stateX, stateY) && activeOp)
                    {
                        activeOp->onNoFurtherWaypoints(ctx);
                    }
                }
            }
        }

    } // namespace nav
} // namespace sunray
