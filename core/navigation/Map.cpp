// Map.cpp — Waypoint management + polygon operations.

#include "core/navigation/Map.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace sunray {
namespace nav {

// ── Construction ───────────────────────────────────────────────────────────────

Map::Map(std::shared_ptr<Config> config)
    : config_(std::move(config))
{}

// ── Persistence ───────────────────────────────────────────────────────────────

static PolygonPoints parsePoints(const nlohmann::json& arr) {
    PolygonPoints pts;
    pts.reserve(arr.size());
    for (auto& p : arr) {
        pts.push_back({p.at(0).get<float>(), p.at(1).get<float>()});
    }
    return pts;
}

static nlohmann::json encodePoints(const PolygonPoints& pts) {
    nlohmann::json arr = nlohmann::json::array();
    for (auto& p : pts) arr.push_back({p.x, p.y});
    return arr;
}

bool Map::load(const std::filesystem::path& path) {
    clear();
    try {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        nlohmann::json j;
        f >> j;

        if (j.contains("perimeter"))  perimeterPoints_ = parsePoints(j["perimeter"]);
        if (j.contains("mow"))        mowPoints_        = parsePoints(j["mow"]);
        if (j.contains("dock"))       dockPoints_       = parsePoints(j["dock"]);
        if (j.contains("exclusions")) {
            for (auto& ex : j["exclusions"])
                exclusions_.push_back(parsePoints(ex));
        }
        mapCRC_ = calcCRC();
        return true;
    } catch (...) {
        clear();
        return false;
    }
}

bool Map::save(const std::filesystem::path& path) const {
    try {
        nlohmann::json j;
        j["perimeter"] = encodePoints(perimeterPoints_);
        j["mow"]       = encodePoints(mowPoints_);
        j["dock"]      = encodePoints(dockPoints_);
        j["exclusions"] = nlohmann::json::array();
        for (auto& ex : exclusions_) j["exclusions"].push_back(encodePoints(ex));

        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

void Map::clear() {
    perimeterPoints_.clear();
    mowPoints_.clear();
    dockPoints_.clear();
    freePoints_.clear();
    exclusions_.clear();
    obstacles_.clear();
    mowPointsIdx  = 0;
    dockPointsIdx = 0;
    freePointsIdx = 0;
    shouldDock_ = false;
    shouldMow_  = false;
    isDocked_   = false;
    mapCRC_     = 0;
}

long Map::calcCRC() const {
    long crc = 0;
    auto accum = [&](const PolygonPoints& pts) {
        for (auto& p : pts) {
            crc ^= static_cast<long>(p.x * 1000) ^ static_cast<long>(p.y * 1000);
        }
    };
    accum(perimeterPoints_);
    accum(mowPoints_);
    accum(dockPoints_);
    for (auto& ex : exclusions_) accum(ex);
    return crc;
}

// ── Mission control ────────────────────────────────────────────────────────────

bool Map::startMowing(float robotX, float robotY) {
    if (mowPoints_.empty()) return false;

    // Find nearest mow point to current position.
    float bestDist = 1e9f;
    int   bestIdx  = 0;
    for (int i = 0; i < static_cast<int>(mowPoints_.size()); ++i) {
        float d = Point(robotX, robotY).distanceTo(mowPoints_[i]);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }

    mowPointsIdx = bestIdx;
    shouldMow_   = true;
    shouldDock_  = false;
    wayMode      = WayType::MOW;

    setLastTargetPoint(robotX, robotY);
    targetPoint = mowPoints_[mowPointsIdx];
    return true;
}

bool Map::startDocking(float robotX, float robotY) {
    if (dockPoints_.empty()) return false;

    // Free-path routing: direct line to first dock point (A* skipped Phase 1).
    freePoints_.clear();
    freePoints_.push_back(dockPoints_.front());

    freePointsIdx = 0;
    dockPointsIdx = 0;
    shouldDock_   = true;
    shouldMow_    = false;
    wayMode       = WayType::FREE;

    setLastTargetPoint(robotX, robotY);
    targetPoint = freePoints_.front();
    return true;
}

bool Map::retryDocking(float robotX, float robotY) {
    return startDocking(robotX, robotY);
}

bool Map::isDocking() const {
    return shouldDock_ && (wayMode == WayType::DOCK || wayMode == WayType::FREE);
}

bool Map::isUndocking() const {
    return !shouldDock_ && (wayMode == WayType::DOCK);
}

// ── Waypoint navigation ────────────────────────────────────────────────────────

bool Map::nextPoint(bool sim, float robotX, float robotY) {
    switch (wayMode) {
        case WayType::MOW:  return nextMowPoint(sim);
        case WayType::DOCK: return nextDockPoint(sim);
        case WayType::FREE: return nextFreePoint(sim);
        default:            return false;
    }
}

bool Map::nextMowPoint(bool sim) {
    if (mowPoints_.empty()) return false;
    ++mowPointsIdx;
    if (mowPointsIdx >= static_cast<int>(mowPoints_.size())) {
        mowPointsIdx = static_cast<int>(mowPoints_.size()) - 1;
        percentCompleted = 100;
        return false;
    }
    percentCompleted = static_cast<int>(100.0f * mowPointsIdx / mowPoints_.size());
    lastTargetPoint = targetPoint;
    targetPoint     = mowPoints_[mowPointsIdx];
    trackReverse    = false;
    trackSlow       = false;
    return true;
}

bool Map::nextDockPoint(bool sim) {
    if (dockPoints_.empty()) return false;
    ++dockPointsIdx;
    if (dockPointsIdx >= static_cast<int>(dockPoints_.size())) return false;
    lastTargetPoint = targetPoint;
    targetPoint     = dockPoints_[dockPointsIdx];
    // Slow down near last dock point.
    trackSlow = (dockPointsIdx >= static_cast<int>(dockPoints_.size()) - 2);
    return true;
}

bool Map::nextFreePoint(bool sim) {
    if (freePoints_.empty()) return false;
    ++freePointsIdx;
    if (freePointsIdx >= static_cast<int>(freePoints_.size())) {
        // Transition from free-path to dock points.
        if (!dockPoints_.empty()) {
            wayMode       = WayType::DOCK;
            dockPointsIdx = 0;
            lastTargetPoint = targetPoint;
            targetPoint     = dockPoints_.front();
            return true;
        }
        return false;
    }
    lastTargetPoint = targetPoint;
    targetPoint     = freePoints_[freePointsIdx];
    return true;
}

bool Map::nextPointIsStraight() const {
    // Check angle change between current and next segment.
    int nextIdx = -1;
    const PolygonPoints* pts = nullptr;
    int curIdx = -1;

    if (wayMode == WayType::MOW && mowPointsIdx + 1 < static_cast<int>(mowPoints_.size())) {
        pts    = &mowPoints_;
        curIdx = mowPointsIdx;
        nextIdx = mowPointsIdx + 1;
    } else if (wayMode == WayType::DOCK && dockPointsIdx + 1 < static_cast<int>(dockPoints_.size())) {
        pts    = &dockPoints_;
        curIdx = dockPointsIdx;
        nextIdx = dockPointsIdx + 1;
    }

    if (pts == nullptr || nextIdx < 0) return true;  // no next → assume straight

    const float a1 = pointsAngle(lastTargetPoint.x, lastTargetPoint.y,
                                  targetPoint.x, targetPoint.y);
    const float a2 = pointsAngle(targetPoint.x, targetPoint.y,
                                  (*pts)[nextIdx].x, (*pts)[nextIdx].y);
    return std::fabs(distancePI(a1, a2)) < (20.0f / 180.0f * static_cast<float>(M_PI));
}

float Map::distanceToTargetPoint(float rx, float ry) const {
    return Point(rx, ry).distanceTo(targetPoint);
}

float Map::distanceToLastTargetPoint(float rx, float ry) const {
    return Point(rx, ry).distanceTo(lastTargetPoint);
}

bool Map::getDockingPos(float& x, float& y, float& delta, int idx) const {
    if (dockPoints_.empty()) return false;
    int i = (idx < 0) ? static_cast<int>(dockPoints_.size()) - 1 : idx;
    i = std::clamp(i, 0, static_cast<int>(dockPoints_.size()) - 1);

    x = dockPoints_[i].x;
    y = dockPoints_[i].y;

    // Heading: angle from previous dock point to this one.
    if (i > 0) {
        delta = pointsAngle(dockPoints_[i-1].x, dockPoints_[i-1].y, x, y);
    } else {
        delta = 0.0f;
    }
    return true;
}

bool Map::mowingCompleted() const {
    return !mowPoints_.empty()
        && mowPointsIdx >= static_cast<int>(mowPoints_.size()) - 1;
}

void Map::setMowingPointPercent(float pct) {
    if (mowPoints_.empty()) return;
    pct = std::clamp(pct, 0.0f, 100.0f);
    mowPointsIdx = static_cast<int>(pct / 100.0f * (mowPoints_.size() - 1));
}

void Map::skipNextMowingPoint() {
    if (mowPointsIdx + 1 < static_cast<int>(mowPoints_.size())) {
        ++mowPointsIdx;
        targetPoint = mowPoints_[mowPointsIdx];
    }
}

void Map::setLastTargetPoint(float x, float y) {
    lastTargetPoint = Point(x, y);
}

// ── Obstacle management ────────────────────────────────────────────────────────

bool Map::addObstacle(float x, float y) {
    obstacles_.push_back({x, y});
    return true;
}

void Map::clearObstacles() {
    obstacles_.clear();
}

// ── Boundary queries ───────────────────────────────────────────────────────────

bool Map::isInsideAllowedArea(float x, float y) const {
    if (perimeterPoints_.empty()) return true;  // no perimeter loaded → allow all
    if (!pointInPolygon(perimeterPoints_, x, y)) return false;
    for (auto& ex : exclusions_) {
        if (pointInPolygon(ex, x, y)) return false;
    }
    return true;
}

// ── Geometry helpers ───────────────────────────────────────────────────────────

float Map::scalePI(float v) {
    float res = std::fmod(v, 2.0f * static_cast<float>(M_PI));
    if (res > static_cast<float>(M_PI))  res -= 2.0f * static_cast<float>(M_PI);
    if (res < -static_cast<float>(M_PI)) res += 2.0f * static_cast<float>(M_PI);
    return res;
}

float Map::distancePI(float x, float w) {
    float d = scalePI(x - w);
    if (d < -static_cast<float>(M_PI)) d += 2.0f * static_cast<float>(M_PI);
    if (d >  static_cast<float>(M_PI)) d -= 2.0f * static_cast<float>(M_PI);
    return d;
}

float Map::scalePIangles(float setAngle, float currAngle) {
    return scalePI(setAngle) + std::round((currAngle - scalePI(setAngle)) / (2.0f * static_cast<float>(M_PI))) * 2.0f * static_cast<float>(M_PI);
}

float Map::pointsAngle(float x1, float y1, float x2, float y2) {
    return std::atan2(y2 - y1, x2 - x1);
}

bool Map::pointInPolygon(const PolygonPoints& poly, float px, float py) {
    // Ray-casting algorithm
    if (poly.size() < 3) return false;
    bool inside = false;
    std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = poly[i].x, yi = poly[i].y;
        float xj = poly[j].x, yj = poly[j].y;
        bool intersect = ((yi > py) != (yj > py))
            && (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

float Map::polygonArea(const PolygonPoints& poly) {
    float area = 0.0f;
    std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        area += (poly[j].x + poly[i].x) * (poly[j].y - poly[i].y);
    }
    return std::fabs(area) * 0.5f;
}

bool Map::lineIntersects(const Point& p0, const Point& p1,
                          const Point& p2, const Point& p3) const {
    float d1x = p1.x - p0.x, d1y = p1.y - p0.y;
    float d2x = p3.x - p2.x, d2y = p3.y - p2.y;
    float cross = d1x * d2y - d1y * d2x;
    if (std::fabs(cross) < 1e-10f) return false;
    float t = ((p2.x - p0.x) * d2y - (p2.y - p0.y) * d2x) / cross;
    float u = ((p2.x - p0.x) * d1y - (p2.y - p0.y) * d1x) / cross;
    return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
}

bool Map::linePolygonIntersection(const Point& src, const Point& dst,
                                   const PolygonPoints& poly) const {
    std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        if (lineIntersects(src, dst, poly[i], poly[j])) return true;
    }
    return false;
}

bool Map::findPath(const Point& src, const Point& dst) {
    // Phase 1: direct line (no obstacle avoidance).
    // A* will be added in Phase 2.
    freePoints_.clear();
    freePoints_.push_back(dst);
    freePointsIdx = 0;
    return true;
}

} // namespace nav
} // namespace sunray
