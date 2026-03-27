// Map.cpp — Waypoint management + polygon operations.

#include "core/navigation/Map.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace sunray {
namespace nav {

static bool lineCircleIntersects(const Point& src, const Point& dst,
                                 const Point& center, float radius);

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

/// Parse mow points — supports both legacy [[x,y],...] and new [{"p":[x,y],"rev":bool},...]
static std::vector<MowPoint> parseMowPoints(const nlohmann::json& arr) {
    std::vector<MowPoint> pts;
    pts.reserve(arr.size());
    for (auto& e : arr) {
        MowPoint mp;
        if (e.is_array()) {
            // Legacy format: [x, y]
            mp.p = { e.at(0).get<float>(), e.at(1).get<float>() };
        } else {
            // New format: { "p": [x,y], "rev": bool, "slow": bool }
            auto& pArr = e.at("p");
            mp.p    = { pArr.at(0).get<float>(), pArr.at(1).get<float>() };
            mp.rev  = e.value("rev",  false);
            mp.slow = e.value("slow", false);
        }
        pts.push_back(mp);
    }
    return pts;
}

static nlohmann::json encodeMowPoints(const std::vector<MowPoint>& pts) {
    nlohmann::json arr = nlohmann::json::array();
    for (auto& mp : pts) {
        if (!mp.rev && !mp.slow) {
            // Compact: [x, y]  (no flags set → backwards-compatible legacy format)
            arr.push_back({ mp.p.x, mp.p.y });
        } else {
            nlohmann::json e;
            e["p"] = { mp.p.x, mp.p.y };
            if (mp.rev)  e["rev"]  = true;
            if (mp.slow) e["slow"] = true;
            arr.push_back(e);
        }
    }
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
        if (j.contains("mow"))        mowPoints_        = parseMowPoints(j["mow"]);
        allMowPoints_ = mowPoints_;
        if (j.contains("dock"))       dockPoints_       = parsePoints(j["dock"]);
        if (j.contains("exclusions")) {
            for (auto& ex : j["exclusions"])
                exclusions_.push_back(parsePoints(ex));
        }
        if (j.contains("zones")) {
            for (const auto& jz : j["zones"]) {
                Zone z;
                z.id    = jz.value("id", "");
                z.order = jz.value("order", 1);
                if (jz.contains("polygon")) z.polygon = parsePoints(jz["polygon"]);
                if (jz.contains("settings")) {
                    auto& js = jz["settings"];
                    z.settings.name       = js.value("name", "Zone");
                    z.settings.stripWidth = js.value("stripWidth", 0.18f);
                    z.settings.speed      = js.value("speed", 1.0f);
                    std::string pat       = js.value("pattern", "stripe");
                    z.settings.pattern    = (pat == "spiral") ? ZonePattern::SPIRAL : ZonePattern::STRIPE;
                }
                zones_.push_back(std::move(z));
            }
            // Sort by order
            std::sort(zones_.begin(), zones_.end(), [](const Zone& a, const Zone& b){ return a.order < b.order; });
        }
        if (j.contains("captureMeta") && j["captureMeta"].is_object()) {
            captureMeta_ = j["captureMeta"];
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
        j["mow"]       = encodeMowPoints(mowPoints_);
        j["dock"]      = encodePoints(dockPoints_);
        j["exclusions"] = nlohmann::json::array();
        for (auto& ex : exclusions_) j["exclusions"].push_back(encodePoints(ex));

        j["zones"] = nlohmann::json::array();
        for (auto& z : zones_) {
            nlohmann::json jz;
            jz["id"]      = z.id;
            jz["order"]   = z.order;
            jz["polygon"] = encodePoints(z.polygon);
            jz["settings"] = {
                { "name",       z.settings.name },
                { "stripWidth", z.settings.stripWidth },
                { "speed",      z.settings.speed },
                { "pattern",    z.settings.pattern == ZonePattern::SPIRAL ? "spiral" : "stripe" },
            };
            j["zones"].push_back(jz);
        }
        if (!captureMeta_.empty()) {
            j["captureMeta"] = captureMeta_;
        }

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
    allMowPoints_.clear();
    dockPoints_.clear();
    freePoints_.clear();
    exclusions_.clear();
    obstacles_.clear();
    zones_.clear();
    captureMeta_ = nlohmann::json::object();
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
    for (auto& mp : mowPoints_) crc ^= static_cast<long>(mp.p.x * 1000) ^ static_cast<long>(mp.p.y * 1000);
    accum(dockPoints_);
    for (auto& ex : exclusions_) accum(ex);
    return crc;
}

// ── Mission control ────────────────────────────────────────────────────────────

bool Map::startMowing(float robotX, float robotY) {
    if (!allMowPoints_.empty()) {
        mowPoints_ = allMowPoints_;
    }
    if (mowPoints_.empty()) return false;

    // Find nearest mow point to current position.
    float bestDist = 1e9f;
    int   bestIdx  = 0;
    for (int i = 0; i < static_cast<int>(mowPoints_.size()); ++i) {
        float d = Point(robotX, robotY).distanceTo(mowPoints_[i].p);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }

    mowPointsIdx = bestIdx;
    shouldMow_   = true;
    shouldDock_  = false;
    wayMode      = WayType::MOW;

    setLastTargetPoint(robotX, robotY);
    trackReverse = mowPoints_[mowPointsIdx].rev;
    trackSlow    = mowPoints_[mowPointsIdx].slow;
    targetPoint  = mowPoints_[mowPointsIdx].p;
    return true;
}

bool Map::startMowingZones(float robotX, float robotY, const std::vector<std::string>& zoneIds) {
    if (zoneIds.empty()) return startMowing(robotX, robotY);
    if (allMowPoints_.empty()) return startMowing(robotX, robotY);

    // Build set of requested zone IDs for fast lookup
    std::unordered_set<std::string> requested(zoneIds.begin(), zoneIds.end());

    // Collect mow points that fall inside any of the requested zone polygons
    std::vector<MowPoint> filtered;
    for (const auto& mp : allMowPoints_) {
        for (const auto& zone : zones_) {
            if (requested.count(zone.id) && pointInPolygon(zone.polygon, mp.p.x, mp.p.y)) {
                filtered.push_back(mp);
                break;
            }
        }
    }

    if (filtered.empty()) return startMowing(robotX, robotY);  // fallback: all zones

    // Activate filtered subset for this mowing mission.
    mowPoints_ = std::move(filtered);
    if (mowPoints_.empty()) return false;

    float bestDist = 1e9f;
    int   bestIdx  = 0;
    for (int i = 0; i < static_cast<int>(mowPoints_.size()); ++i) {
        float d = Point(robotX, robotY).distanceTo(mowPoints_[i].p);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }

    mowPointsIdx = bestIdx;
    shouldMow_   = true;
    shouldDock_  = false;
    wayMode      = WayType::MOW;

    setLastTargetPoint(robotX, robotY);
    trackReverse = mowPoints_[mowPointsIdx].rev;
    trackSlow    = mowPoints_[mowPointsIdx].slow;
    targetPoint  = mowPoints_[mowPointsIdx].p;
    return true;
}

bool Map::startDocking(float robotX, float robotY) {
    if (dockPoints_.empty()) return false;

    // Route from current position to first dock point, avoiding obstacles (C.7).
    const bool havePath = findPath({robotX, robotY}, dockPoints_.front());
    if (!havePath || freePoints_.empty()) return false;

    freePointsIdx = 0;
    dockPointsIdx = 0;
    shouldDock_   = true;
    shouldMow_    = false;
    wayMode       = WayType::FREE;

    setLastTargetPoint(robotX, robotY);
    trackReverse = false;
    trackSlow    = false;
    targetPoint  = freePoints_.front();
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
    bool ok = false;
    switch (wayMode) {
        case WayType::MOW:  ok = nextMowPoint(sim); break;
        case WayType::DOCK: ok = nextDockPoint(sim); break;
        case WayType::FREE: ok = nextFreePoint(sim); break;
        default:            return false;
    }

    // Dynamic replanning: if the new segment is blocked by a known obstacle,
    // compute a free-path detour.
    if (ok && wayMode != WayType::FREE) {
        for (const auto& obs : obstacles_) {
            const float r = obs.radius_m + 0.4f;
            if (lineCircleIntersects(targetPoint, lastTargetPoint, obs.center, r)) {
                previousWayMode = wayMode;
                findPath(lastTargetPoint, targetPoint);
                wayMode = WayType::FREE;
                break;
            }
        }
    }
    return ok;
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
    trackReverse    = mowPoints_[mowPointsIdx].rev;
    trackSlow       = mowPoints_[mowPointsIdx].slow;
    targetPoint     = mowPoints_[mowPointsIdx].p;
    return true;
}

bool Map::nextDockPoint(bool sim) {
    if (dockPoints_.empty()) return false;
    ++dockPointsIdx;
    if (dockPointsIdx >= static_cast<int>(dockPoints_.size())) return false;
    lastTargetPoint = targetPoint;
    targetPoint     = dockPoints_[dockPointsIdx];
    trackReverse    = false;
    // Slow down near last dock point.
    trackSlow = (dockPointsIdx >= static_cast<int>(dockPoints_.size()) - 2);
    return true;
}

bool Map::nextFreePoint(bool sim) {
    if (freePoints_.empty()) return false;
    ++freePointsIdx;
    if (freePointsIdx >= static_cast<int>(freePoints_.size())) {
        // End of free-path detour.
        if (shouldDock_ && !dockPoints_.empty()) {
            wayMode         = WayType::DOCK;
            dockPointsIdx   = 0;
            lastTargetPoint = targetPoint;
            targetPoint     = dockPoints_.front();
            trackReverse    = false;
            trackSlow       = (dockPoints_.size() <= 2);
            return true;
        } else if (shouldMow_) {
            wayMode         = WayType::MOW;
            // Target point stays what it was (the end of the free-path detour).
            // But we need to update the tracking state.
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
    Point nextPt;
    bool  hasNext = false;

    if (wayMode == WayType::MOW && mowPointsIdx + 1 < static_cast<int>(mowPoints_.size())) {
        nextPt  = mowPoints_[mowPointsIdx + 1].p;
        hasNext = true;
    } else if (wayMode == WayType::DOCK && dockPointsIdx + 1 < static_cast<int>(dockPoints_.size())) {
        nextPt  = dockPoints_[dockPointsIdx + 1];
        hasNext = true;
    }

    if (!hasNext) return true;  // no next → assume straight

    const float a1 = pointsAngle(lastTargetPoint.x, lastTargetPoint.y,
                                  targetPoint.x, targetPoint.y);
    const float a2 = pointsAngle(targetPoint.x, targetPoint.y,
                                  nextPt.x, nextPt.y);
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
        trackReverse = mowPoints_[mowPointsIdx].rev;
        trackSlow    = mowPoints_[mowPointsIdx].slow;
        targetPoint  = mowPoints_[mowPointsIdx].p;
    }
}

void Map::setLastTargetPoint(float x, float y) {
    lastTargetPoint = Point(x, y);
}

// ── Obstacle management ────────────────────────────────────────────────────────

bool Map::addObstacle(float x, float y, unsigned now_ms, bool persistent) {
    // If an existing obstacle is within merge_radius, just increment its hitCount.
    constexpr float mergeRadius = 0.5f;
    for (auto& obs : obstacles_) {
        if (obs.center.distanceTo({x, y}) < mergeRadius) {
            obs.hitCount++;
            obs.detectedAt_ms = now_ms;  // refresh timestamp
            return true;
        }
    }
    float radius_m = 0.3f;
    if (config_) radius_m = config_->get<float>("obstacle_diameter_m", 0.6f) * 0.5f;
    obstacles_.push_back({ {x, y}, radius_m, now_ms, 1, persistent });
    return true;
}

void Map::clearObstacles() {
    obstacles_.clear();
}

void Map::clearAutoObstacles() {
    obstacles_.erase(
        std::remove_if(obstacles_.begin(), obstacles_.end(),
                       [](const OnTheFlyObstacle& o){ return !o.persistent; }),
        obstacles_.end());
}

void Map::cleanupExpiredObstacles(unsigned now_ms, unsigned expiry_ms) {
    obstacles_.erase(
        std::remove_if(obstacles_.begin(), obstacles_.end(),
                       [&](const OnTheFlyObstacle& o){
                           return !o.persistent && (now_ms - o.detectedAt_ms) >= expiry_ms;
                       }),
        obstacles_.end());
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

/// Returns true if the line segment src→dst passes within radius of center.
static bool lineCircleIntersects(const Point& src, const Point& dst,
                                  const Point& center, float radius) {
    float dx = dst.x - src.x, dy = dst.y - src.y;
    float len2 = dx*dx + dy*dy;
    if (len2 < 1e-8f) return src.distanceTo(center) < radius;
    // Projection of center onto line, clamped to segment
    float t = ((center.x - src.x) * dx + (center.y - src.y) * dy) / len2;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    float cx = src.x + t * dx - center.x;
    float cy = src.y + t * dy - center.y;
    return (cx*cx + cy*cy) < radius * radius;
}

void Map::injectFreePath(const std::vector<Point>& waypoints) {
    if (waypoints.empty()) return;
    freePoints_   = waypoints;
    freePointsIdx = 0;
    previousWayMode = wayMode;
    wayMode         = WayType::FREE;
    lastTargetPoint = targetPoint;
    targetPoint     = freePoints_.front();
}

bool Map::findPath(const Point& src, const Point& dst) {
    // C.7: Iterative obstacle-avoidance routing.
    // We start with a direct path and insert detour points as long as any segment is blocked.
    freePoints_.clear();
    freePointsIdx = 0;

    std::vector<Point> path = {src, dst};
    bool changed = true;
    int iterations = 0;
    const int maxIterations = 5;  // safety limit to prevent infinite loops in dense areas

    while (changed && iterations < maxIterations) {
        changed = false;
        iterations++;
        for (size_t i = 0; i < path.size() - 1; ++i) {
            const Point& p1 = path[i];
            const Point& p2 = path[i+1];

            for (const auto& obs : obstacles_) {
                const float r = obs.radius_m + 0.4f;
                if (lineCircleIntersects(p1, p2, obs.center, r)) {
                    // Perpendicular unit vector to p1→p2
                    float dx = p2.x - p1.x, dy = p2.y - p1.y;
                    float len = std::sqrt(dx*dx + dy*dy);
                    if (len < 0.01f) continue;
                    float px = -dy / len, py = dx / len;

                    // Project obstacle center onto segment to find closest point on path
                    float t = ((obs.center.x - p1.x) * dx + (obs.center.y - p1.y) * dy) / (len * len);
                    t = std::clamp(t, 0.0f, 1.0f);
                    Point closest{p1.x + t * dx, p1.y + t * dy};

                    // Determine which side the obstacle is on — detour to the opposite side
                    float cross = dx * (obs.center.y - p1.y) - dy * (obs.center.x - p1.x);
                    float side  = (cross >= 0.0f) ? -1.0f : 1.0f;

                    Point detour{closest.x + side * px * r, closest.y + side * py * r};
                    path.insert(path.begin() + i + 1, detour);
                    changed = true;
                    break;
                }
            }
            if (changed) break;
        }
    }

    // Skip the start point (robot position) and store the rest as free-path waypoints.
    for (size_t i = 1; i < path.size(); ++i) {
        freePoints_.push_back(path[i]);
    }

    return !freePoints_.empty();
}

} // namespace nav
} // namespace sunray
