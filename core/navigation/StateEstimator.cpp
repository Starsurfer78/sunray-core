// StateEstimator.cpp — EKF-based robot pose estimation.
//
// State vector: [x, y, θ]
// Covariance P:  3×3 symmetric, stored row-major as P_[0..8].
//
// Predict  — differential-drive model, Jacobian F is nearly-identity.
// Update 1 — GPS RTK position (H = [[1,0,0],[0,1,0]]).
// Update 2 — IMU heading      (H = [[0,0,1]]).
//
// All matrix operations are written out analytically (no matrix library).
// The EKF stays numerically stable for the expected operating range
// (position ≤ 10 km, heading ∈ [−π, π]).

#include "core/navigation/StateEstimator.h"

#include <algorithm>
#include <cmath>

namespace sunray {
namespace nav {

static constexpr float PI     = static_cast<float>(M_PI);
static constexpr float TWO_PI = 2.0f * PI;

// ── Construction ───────────────────────────────────────────────────────────────

StateEstimator::StateEstimator(std::shared_ptr<Config> config)
    : config_(std::move(config))
{
    reset();  // initialises P_ to default covariance
}

// ── Helpers ────────────────────────────────────────────────────────────────────

float StateEstimator::scalePI(float v) {
    float r = std::fmod(v, TWO_PI);
    if (r >  PI) r -= TWO_PI;
    if (r < -PI) r += TWO_PI;
    return r;
}

StateEstimator::EkfNoise StateEstimator::loadNoise() const {
    EkfNoise n;
    n.q_xy       = config_ ? config_->get<float>("ekf_q_xy",    0.0001f) : 0.0001f;
    n.q_theta    = config_ ? config_->get<float>("ekf_q_theta", 0.0001f) : 0.0001f;
    n.r_gps      = config_ ? config_->get<float>("ekf_r_gps",   0.05f)   : 0.05f;
    n.r_imu      = config_ ? config_->get<float>("ekf_r_imu",   0.02f)   : 0.02f;
    n.failover_ms = config_
        ? static_cast<unsigned long>(config_->get<int>("ekf_gps_failover_ms", 5000))
        : 5000UL;
    return n;
}

// ── EKF Predict step ──────────────────────────────────────────────────────────
//
// P_new = F * P * F^T + Q
//
// F = [[1, 0, fa],   fa = −dDist·sin(midHeading)
//      [0, 1, fb],   fb = +dDist·cos(midHeading)
//      [0, 0,  1]]
//
// Analytical result (exploiting that P is symmetric and F is almost-identity):
//   fp02 = P_[2] + fa·P_[8]          (third element of FP row 0)
//   fp12 = P_[5] + fb·P_[8]          (third element of FP row 1)
//
//   P_new[0,0] = (P_[0]+fa·P_[6]) + fa·fp02 + q_xy
//   P_new[0,1] = (P_[1]+fa·P_[7]) + fb·fp02
//   P_new[0,2] = fp02
//   P_new[1,1] = (P_[4]+fb·P_[7]) + fb·fp12 + q_xy
//   P_new[1,2] = fp12
//   P_new[2,2] = P_[8] + q_theta
//   Off-diagonal elements mirror via symmetry.

void StateEstimator::predictStep(float dDist, float dTheta, const EkfNoise& n) {
    const float midH = heading_ + dTheta * 0.5f;
    const float cosM = std::cos(midH);
    const float sinM = std::sin(midH);

    // Jacobian partial derivatives
    const float fa = -dDist * sinM;
    const float fb =  dDist * cosM;

    const float p00 = P_[0], p01 = P_[1], p02 = P_[2];
    const float            p11 = P_[4], p12 = P_[5];
    const float                               p22 = P_[8];
    // Note: P_[6]=p02, P_[7]=p12 via symmetry (used below as p20=p02, p21=p12)

    const float fp02 = p02 + fa * p22;
    const float fp12 = p12 + fb * p22;

    P_[0] = (p00 + fa * p02) + fa * fp02 + n.q_xy;  // FPFt[0,0] + Q
    P_[1] = (p01 + fa * p12) + fb * fp02;            // FPFt[0,1]
    P_[2] = fp02;                                     // FPFt[0,2]
    P_[3] = P_[1];                                   // symmetric
    P_[4] = (p11 + fb * p12) + fb * fp12 + n.q_xy;  // FPFt[1,1] + Q
    P_[5] = fp12;                                     // FPFt[1,2]
    P_[6] = P_[2];                                   // symmetric
    P_[7] = P_[5];                                   // symmetric
    P_[8] = p22 + n.q_theta;                         // FPFt[2,2] + Q
}

// ── GPS measurement update ────────────────────────────────────────────────────
//
// H = [[1,0,0],[0,1,0]],  z = [measE, measN]
//
// S = H·P·H^T + R  (2×2 diagonal-R)
// K = P·H^T · S^{−1}  (3×2, computed analytically)
// x += K · (z − H·x)
// P  = (I − K·H) · P

void StateEstimator::gpsUpdate(float measE, float measN, const EkfNoise& n) {
    const float y0 = measE - x_;
    const float y1 = measN - y_;

    const float r_sq = n.r_gps * n.r_gps;
    const float s00  = P_[0] + r_sq;
    const float s01  = P_[1];
    const float s10  = P_[3];
    const float s11  = P_[4] + r_sq;

    const float det = s00 * s11 - s01 * s10;
    if (std::fabs(det) < 1e-9f) return;  // degenerate — skip
    const float id = 1.0f / det;

    // K[i][0] = ( P[i*3+0]*s11 − P[i*3+1]*s10 ) / det
    // K[i][1] = (−P[i*3+0]*s01 + P[i*3+1]*s00 ) / det
    float k[3][2];
    for (int i = 0; i < 3; ++i) {
        k[i][0] = ( P_[i*3+0]*s11 - P_[i*3+1]*s10) * id;
        k[i][1] = (-P_[i*3+0]*s01 + P_[i*3+1]*s00) * id;
    }

    // State update
    x_       += k[0][0]*y0 + k[0][1]*y1;
    y_       += k[1][0]*y0 + k[1][1]*y1;
    heading_  = scalePI(heading_ + k[2][0]*y0 + k[2][1]*y1);

    // Covariance update: P = (I − K·H) · P
    // (I−KH) row 0: [1−k[0][0], −k[0][1], 0]
    // (I−KH) row 1: [−k[1][0],  1−k[1][1], 0]
    // (I−KH) row 2: [−k[2][0], −k[2][1],  1]
    float p[9];
    for (int i = 0; i < 9; ++i) p[i] = P_[i];

    for (int j = 0; j < 3; ++j) {
        P_[0*3+j] = (1.0f - k[0][0])*p[0*3+j] - k[0][1]*p[1*3+j];
        P_[1*3+j] = -k[1][0]*p[0*3+j] + (1.0f - k[1][1])*p[1*3+j];
        P_[2*3+j] = -k[2][0]*p[0*3+j] - k[2][1]*p[1*3+j] + p[2*3+j];
    }
}

// ── IMU heading measurement update ────────────────────────────────────────────
//
// H = [0,0,1],  z = yaw
//
// S = P_[8] + r_imu²   (scalar)
// K = P[:,2] / S        (3×1)
// x += K · (yaw − θ)
// P  = (I − K·H) · P

void StateEstimator::imuUpdate(float yaw, const EkfNoise& n) {
    const float innov = scalePI(yaw - heading_);
    const float s     = P_[8] + n.r_imu * n.r_imu;
    if (std::fabs(s) < 1e-9f) return;

    const float k0 = P_[2] / s;
    const float k1 = P_[5] / s;
    const float k2 = P_[8] / s;

    x_       += k0 * innov;
    y_       += k1 * innov;
    heading_  = scalePI(heading_ + k2 * innov);

    // P = (I − K·H) · P
    // (I−KH) = [[1,0,−k0],[0,1,−k1],[0,0,1−k2]]
    float p[9];
    for (int i = 0; i < 9; ++i) p[i] = P_[i];

    for (int j = 0; j < 3; ++j) {
        P_[0*3+j] = p[0*3+j] - k0 * p[2*3+j];
        P_[1*3+j] = p[1*3+j] - k1 * p[2*3+j];
        P_[2*3+j] = (1.0f - k2) * p[2*3+j];
    }
}

// ── Public update() — predict + GPS failover ──────────────────────────────────

void StateEstimator::update(const OdometryData& odo, unsigned long dt_ms) {
    totalTime_ms_ += dt_ms;

    // GPS failover: clear fix flag if no RTK fix received for too long
    if (gpsHasFix_) {
        const EkfNoise n = loadNoise();
        if (totalTime_ms_ - lastGpsFixTime_ms_ > n.failover_ms) {
            gpsHasFix_ = false;
        }
    }

    if (!odo.mcuConnected) {
        firstUpdate_ = true;
        return;
    }
    if (firstUpdate_) {
        firstUpdate_ = false;
        return;
    }

    const float ticksPerRev  = config_ ? config_->get<float>("ticks_per_revolution", 120.0f) : 120.0f;
    const float wheelDiam    = config_ ? config_->get<float>("wheel_diameter_m",     0.197f)  : 0.197f;
    const float wheelBase    = config_ ? config_->get<float>("wheel_base_m",         0.285f)  : 0.285f;

    const float ticksPerMeter = ticksPerRev / (PI * wheelDiam);

    float distLeft  = static_cast<float>(odo.leftTicks)  / ticksPerMeter;
    float distRight = static_cast<float>(odo.rightTicks) / ticksPerMeter;

    // Sanity guard: skip frame if implied speed > 0.5 m/step
    if (std::fabs(distLeft) > 0.5f || std::fabs(distRight) > 0.5f) {
        distLeft = distRight = 0.0f;
    }

    const float dDist  = (distLeft + distRight) * 0.5f;
    const float dTheta = (distRight - distLeft) / wheelBase;

    // State integration (heading updated before covariance for midpoint accuracy)
    const float midH = heading_ + dTheta * 0.5f;
    x_       += dDist * std::cos(midH);
    y_       += dDist * std::sin(midH);
    heading_  = scalePI(heading_ + dTheta);

    // Safety clamp
    if (std::fabs(x_) > 10000.0f || std::fabs(y_) > 10000.0f) {
        x_ = y_ = 0.0f;
    }

    // Ground speed (low-pass)
    if (dt_ms > 0) {
        const float speed = std::fabs(dDist) / (static_cast<float>(dt_ms) * 0.001f);
        groundSpeed_ = 0.9f * groundSpeed_ + 0.1f * speed;
    }

    // EKF covariance predict
    predictStep(dDist, dTheta, loadNoise());
}

// ── Public updateGps() ────────────────────────────────────────────────────────

void StateEstimator::updateGps(float posE, float posN, bool isFix, bool isFloat) {
    gpsHasFloat_ = isFloat || isFix;

    if (isFix) {
        gpsHasFix_         = true;
        lastGpsFixTime_ms_ = totalTime_ms_;

        // GPS antenna offset correction (robot reference point = mid-axle)
        const float offX = config_ ? config_->get<float>("gps_offset_x_m", 0.0f) : 0.0f;
        const float offY = config_ ? config_->get<float>("gps_offset_y_m", 0.0f) : 0.0f;
        const float cosH = std::cos(heading_);
        const float sinH = std::sin(heading_);
        const float measE = posE - (offX * cosH - offY * sinH);
        const float measN = posN - (offX * sinH + offY * cosH);

        gpsUpdate(measE, measN, loadNoise());
    }
}

// ── Public updateImu() ────────────────────────────────────────────────────────

void StateEstimator::updateImu(float yaw, float roll, float pitch) {
    (void)roll; (void)pitch;
    imuActive_ = true;
    imuUpdate(yaw, loadNoise());
}

// ── Fusion mode / uncertainty ─────────────────────────────────────────────────

std::string StateEstimator::fusionMode() const {
    if (gpsHasFix_)  return "EKF+GPS";
    if (imuActive_)  return "EKF+IMU";
    return "Odo";
}

float StateEstimator::posUncertainty() const {
    return std::sqrt(P_[0] + P_[4]);
}

// ── Direct pose override ──────────────────────────────────────────────────────

void StateEstimator::setPose(float x, float y, float heading) {
    x_       = x;
    y_       = y;
    heading_ = scalePI(heading);
    // Reset covariance — position is now known
    for (int i = 0; i < 9; ++i) P_[i] = 0.0f;
    P_[0] = 0.01f;
    P_[4] = 0.01f;
    P_[8] = 0.01f;
}

void StateEstimator::reset() {
    x_           = 0.0f;
    y_           = 0.0f;
    heading_     = 0.0f;
    groundSpeed_ = 0.0f;
    gpsHasFix_   = false;
    gpsHasFloat_ = false;
    imuActive_   = false;
    firstUpdate_ = true;
    totalTime_ms_      = 0;
    lastGpsFixTime_ms_ = 0;

    // Initial covariance: 10 cm position std-dev, ~5.7° heading std-dev
    for (int i = 0; i < 9; ++i) P_[i] = 0.0f;
    P_[0] = 0.01f;
    P_[4] = 0.01f;
    P_[8] = 0.01f;
}

} // namespace nav
} // namespace sunray
