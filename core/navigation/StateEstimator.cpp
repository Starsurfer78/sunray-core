// StateEstimator.cpp — Odometry dead-reckoning + GPS fusion stub.
//
// Ported from sunray/StateEstimator.cpp — computeRobotState() odometry section.
// IMU / April-tag / Reflector-tag / LiDAR / guidance-sheet paths removed (Phase 1).
// GPS fusion: updateGps() replaces position directly on fix (Phase 2 will add
// complementary filter matching the original fusionPI() logic).

#include "core/navigation/StateEstimator.h"

#include <algorithm>
#include <cmath>

namespace sunray {
namespace nav {

static constexpr float PI     = static_cast<float>(M_PI);
static constexpr float TWO_PI = 2.0f * PI;

// ── Construction ──────────────────────────────────────────────────────────────

StateEstimator::StateEstimator(std::shared_ptr<Config> config)
    : config_(std::move(config))
{}

// ── Helpers ───────────────────────────────────────────────────────────────────

float StateEstimator::scalePI(float v) {
    float r = std::fmod(v, TWO_PI);
    if (r >  PI) r -= TWO_PI;
    if (r < -PI) r += TWO_PI;
    return r;
}

// ── Pose update from odometry ─────────────────────────────────────────────────

void StateEstimator::update(const OdometryData& odo, unsigned long dt_ms) {
    if (!odo.mcuConnected) {
        firstUpdate_ = true;  // reset on reconnect — next valid frame is a fresh start
        return;
    }

    if (firstUpdate_) {
        firstUpdate_ = false;
        return;  // consume the first valid frame; no delta to integrate yet
    }

    const float ticksPerMeter = config_
        ? config_->get<float>("ticks_per_meter", 120.0f)
        : 120.0f;
    const float wheelBase = config_
        ? config_->get<float>("wheel_base_m", 0.285f)
        : 0.285f;

    float distLeft  = static_cast<float>(odo.leftTicks)  / ticksPerMeter;
    float distRight = static_cast<float>(odo.rightTicks) / ticksPerMeter;

    // Sanity guard: skip frame if tick delta implies > 0.5 m in one 20 ms step
    // (e.g. counter reset after MCU reconnect).
    if (std::fabs(distLeft) > 0.5f || std::fabs(distRight) > 0.5f) {
        distLeft  = 0.0f;
        distRight = 0.0f;
    }

    const float dist     = (distLeft + distRight) * 0.5f;
    const float dHeading = (distRight - distLeft) / wheelBase;

    x_       += dist * std::cos(heading_);
    y_       += dist * std::sin(heading_);
    heading_  = scalePI(heading_ + dHeading);

    // Safety clamp: if odometry drifts beyond ±10 km, something is very wrong.
    if (std::fabs(x_) > 10000.0f || std::fabs(y_) > 10000.0f) {
        x_ = 0.0f;
        y_ = 0.0f;
    }

    // Ground speed: low-pass from |dist| / dt.
    if (dt_ms > 0) {
        const float speed = std::fabs(dist) / (static_cast<float>(dt_ms) * 0.001f);
        groundSpeed_ = 0.9f * groundSpeed_ + 0.1f * speed;
    }
}

// ── GPS update (Phase 2 stub) ─────────────────────────────────────────────────

void StateEstimator::updateGps(float posE, float posN, bool isFix, bool isFloat) {
    gpsHasFix_   = isFix;
    gpsHasFloat_ = isFloat || isFix;

    if (isFix) {
        // Phase 2 will replace this with a complementary filter (fusionPI).
        x_ = posE;
        y_ = posN;
    }
}

// ── Direct pose override ──────────────────────────────────────────────────────

void StateEstimator::setPose(float x, float y, float heading) {
    x_       = x;
    y_       = y;
    heading_ = scalePI(heading);
}

void StateEstimator::reset() {
    x_           = 0.0f;
    y_           = 0.0f;
    heading_     = 0.0f;
    groundSpeed_ = 0.0f;
    gpsHasFix_   = false;
    gpsHasFloat_ = false;
    firstUpdate_ = true;
}

} // namespace nav
} // namespace sunray
