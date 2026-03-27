#pragma once
// LineTracker.h — Stanley path-tracking controller.
//
// Ported from sunray/LineTracker.cpp — trackLine() function.
// Global variables replaced by class members.
// All Arduino/CONSOLE/motor globals replaced by OpContext + Map + StateEstimator DI.
//
// Stanley controller reference:
//   https://medium.com/@dingyan7361/three-methods-of-vehicle-lateral-control-pure-pursuit-stanley-and-mpc-db8cc1d32081
//   angular = p * headingError + atan2(k * lateralError, (0.001 + |groundSpeed|))
//
// Op events fired directly on ctx.opMgr.activeOp():
//   onTargetReached()      — target within TARGET_REACHED_TOLERANCE
//   onNoFurtherWaypoints() — map.nextPoint() returned false
//   onKidnapped(true/false) — robot too far off expected path
//
// Usage:
//   reset()              — call in MowOp::begin() / DockOp::begin()
//   track(ctx, map, est) — call every control loop iteration while active

#include "core/Config.h"

#include <cmath>
#include <memory>

// Forward declarations — avoid including Op.h here to keep navigation/
// independent of op/ in the include graph. Op.h includes us via Robot.h,
// not the other way around; we get OpContext via the .cpp include.
namespace sunray {
    struct OpContext;
    namespace nav {
        class Map;
        class StateEstimator;
    }
}

namespace sunray {
namespace nav {

class LineTracker {
public:
    explicit LineTracker(std::shared_ptr<Config> config = nullptr);

    /// Reset internal rotation state (call at begin() of each Op using tracking).
    void reset();

    /// Execute one tracking iteration.
    /// Reads pose from estimator, computes Stanley steering, fires Op events.
    void track(OpContext& ctx, Map& map, const StateEstimator& estimator);

    // ── State accessors (for telemetry) ────────────────────────────────────────

    float lateralError()      const { return lateralError_; }
    float targetDist()        const { return targetDist_; }
    bool  angleToTargetFits() const { return angleToTargetFits_; }
    bool  kidnapped()         const { return stateKidnapped_; }

private:
    // ── Geometry helpers ──────────────────────────────────────────────────────

    /// Signed perpendicular distance from (px,py) to infinite line (x1,y1)→(x2,y2).
    static float distanceLineInfinite(float px, float py,
                                       float x1, float y1,
                                       float x2, float y2);

    /// Distance from (px,py) to finite segment (x1,y1)→(x2,y2).
    static float distanceLine(float px, float py,
                               float x1, float y1,
                               float x2, float y2);

    static float scalePI(float v);
    static float distancePI(float a, float b);
    static float scalePIangles(float setAngle, float currAngle);

    // ── Config ────────────────────────────────────────────────────────────────

    std::shared_ptr<Config> config_;

    // ── Tracking state (reset by reset()) ────────────────────────────────────

    bool  rotateLeft_        = false;
    bool  rotateRight_       = false;
    bool  angleToTargetFits_ = false;
    bool  stateKidnapped_    = false;
    float lateralError_      = 0.0f;
    float targetDist_        = 0.0f;
    float trackerDiffDelta_  = 0.0f;

    // ── Constants ─────────────────────────────────────────────────────────────

    static constexpr float PI = static_cast<float>(M_PI);

    /// Robot is "at target" when distance drops below this (metres).
    static constexpr float TARGET_REACHED_TOLERANCE = 0.2f;

    /// Kidnap threshold: robot this far off the planned line → fire onKidnapped.
    static constexpr float KIDNAP_TOLERANCE = 3.0f;

    /// Angular velocity used when rotating toward waypoint (≈ 0.5 rad/s).
    static constexpr float ROTATE_SPEED_RADPS =
        29.0f / 180.0f * static_cast<float>(M_PI);
};

} // namespace nav
} // namespace sunray
