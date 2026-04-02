// Map.cpp — Waypoint management + polygon operations.

#include "core/navigation/Map.h"
#include "core/navigation/GridMap.h"
#include "core/navigation/Planner.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace sunray {
namespace nav {

static bool lineCircleIntersects(const Point& src, const Point& dst,
                                 const Point& center, float radius);
static constexpr float TRACK_TARGET_REACHED_TOLERANCE_M = 0.2f;

// ── Construction ───────────────────────────────────────────────────────────────

Map::Map(std::shared_ptr<Config> config)
    : config_(std::move(config))
{
    applyConfigDefaults();
}

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

static ExclusionType parseExclusionType(const std::string& value) {
    return value == "soft" ? ExclusionType::SOFT : ExclusionType::HARD;
}

static const char* encodeExclusionType(ExclusionType value) {
    return value == ExclusionType::SOFT ? "soft" : "hard";
}

static DockApproachMode parseDockApproachMode(const std::string& value) {
    return value == "reverse_allowed" ? DockApproachMode::REVERSE_ALLOWED
                                      : DockApproachMode::FORWARD_ONLY;
}

static const char* encodeDockApproachMode(DockApproachMode value) {
    return value == DockApproachMode::REVERSE_ALLOWED ? "reverse_allowed" : "forward_only";
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

static std::vector<MowPoint> exportMowPoints(const RoutePlan& route) {
    std::vector<MowPoint> points;
    points.reserve(route.points.size());
    for (const auto& routePoint : route.points) {
        points.push_back(MowPoint{
            routePoint.p,
            routePoint.reverse,
            routePoint.slow,
        });
    }
    return points;
}

static PolygonPoints exportPolygonPoints(const RoutePlan& route) {
    PolygonPoints points;
    points.reserve(route.points.size());
    for (const auto& routePoint : route.points) {
        points.push_back(routePoint.p);
    }
    return points;
}

static PlannerSettings parsePlannerSettings(const nlohmann::json& jp) {
    PlannerSettings settings;
    settings.defaultClearance_m    = jp.value("defaultClearance", settings.defaultClearance_m);
    settings.perimeterSoftMargin_m = jp.value("perimeterSoftMargin", settings.perimeterSoftMargin_m);
    settings.perimeterHardMargin_m = jp.value("perimeterHardMargin", settings.perimeterHardMargin_m);
    settings.obstacleInflation_m   = jp.value("obstacleInflation", settings.obstacleInflation_m);
    settings.softNoGoCostScale     = jp.value("softNoGoCostScale", settings.softNoGoCostScale);
    settings.replanPeriod_ms       = jp.value("replanPeriodMs", settings.replanPeriod_ms);
    settings.gridCellSize_m        = jp.value("gridCellSize", settings.gridCellSize_m);
    return settings;
}

static nlohmann::json encodePlannerSettings(const PlannerSettings& settings) {
    return {
        { "defaultClearance", settings.defaultClearance_m },
        { "perimeterSoftMargin", settings.perimeterSoftMargin_m },
        { "perimeterHardMargin", settings.perimeterHardMargin_m },
        { "obstacleInflation", settings.obstacleInflation_m },
        { "softNoGoCostScale", settings.softNoGoCostScale },
        { "replanPeriodMs", settings.replanPeriod_ms },
        { "gridCellSize", settings.gridCellSize_m },
    };
}

static std::vector<ExclusionMeta> parseExclusionMeta(const nlohmann::json& arr) {
    std::vector<ExclusionMeta> meta;
    meta.reserve(arr.size());
    for (const auto& item : arr) {
        ExclusionMeta entry;
        entry.type = parseExclusionType(item.value("type", "hard"));
        entry.clearance_m = item.value("clearance", entry.clearance_m);
        entry.costScale = item.value("costScale", entry.costScale);
        meta.push_back(entry);
    }
    return meta;
}

static nlohmann::json encodeExclusionMeta(const std::vector<ExclusionMeta>& meta) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& entry : meta) {
        arr.push_back({
            { "type", encodeExclusionType(entry.type) },
            { "clearance", entry.clearance_m },
            { "costScale", entry.costScale },
        });
    }
    return arr;
}

static DockMeta parseDockMeta(const nlohmann::json& jd) {
    DockMeta meta;
    meta.approachMode = parseDockApproachMode(jd.value("approachMode", "forward_only"));
    if (jd.contains("corridor")) meta.corridor = parsePoints(jd["corridor"]);
    if (jd.contains("finalAlignHeadingDeg") && !jd["finalAlignHeadingDeg"].is_null()) {
        meta.finalAlignHeading_deg = jd["finalAlignHeadingDeg"].get<float>();
        meta.hasFinalAlignHeading = true;
    }
    meta.slowZoneRadius_m = jd.value("slowZoneRadius", meta.slowZoneRadius_m);
    return meta;
}

static nlohmann::json encodeDockMeta(const DockMeta& meta) {
    nlohmann::json jd;
    jd["approachMode"] = encodeDockApproachMode(meta.approachMode);
    jd["corridor"] = encodePoints(meta.corridor);
    jd["finalAlignHeadingDeg"] = meta.hasFinalAlignHeading
        ? nlohmann::json(meta.finalAlignHeading_deg)
        : nlohmann::json(nullptr);
    jd["slowZoneRadius"] = meta.slowZoneRadius_m;
    return jd;
}

bool Map::load(const std::filesystem::path& path) {
    clear();
    try {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        nlohmann::json j;
        f >> j;

        if (j.contains("perimeter"))  perimeterPoints_ = parsePoints(j["perimeter"]);
        std::vector<MowPoint> parsedMowPoints;
        if (j.contains("mow"))        parsedMowPoints = parseMowPoints(j["mow"]);
        PolygonPoints parsedDockPoints;
        if (j.contains("dock"))       parsedDockPoints = parsePoints(j["dock"]);
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
                    z.settings.angle      = js.value("angle", 0.0f);
                    z.settings.edgeMowing = js.value("edgeMowing", true);
                    z.settings.edgeRounds = js.value("edgeRounds", 1);
                    z.settings.speed      = js.value("speed", 1.0f);
                    std::string pat       = js.value("pattern", "stripe");
                    z.settings.pattern    = (pat == "spiral") ? ZonePattern::SPIRAL : ZonePattern::STRIPE;
                    z.settings.reverseAllowed = js.value("reverseAllowed", false);
                    z.settings.clearance = js.value("clearance", 0.25f);
                }
                zones_.push_back(std::move(z));
            }
            // Sort by order
            std::sort(zones_.begin(), zones_.end(), [](const Zone& a, const Zone& b){ return a.order < b.order; });
        }
        if (j.contains("planner") && j["planner"].is_object()) {
            planner_ = parsePlannerSettings(j["planner"]);
        }
        if (j.contains("dockMeta") && j["dockMeta"].is_object()) {
            dockMeta_ = parseDockMeta(j["dockMeta"]);
        }
        if (j.contains("exclusionMeta") && j["exclusionMeta"].is_array()) {
            exclusionMeta_ = parseExclusionMeta(j["exclusionMeta"]);
        }
        while (exclusionMeta_.size() < exclusions_.size()) {
            exclusionMeta_.push_back(ExclusionMeta{});
        }
        if (j.contains("captureMeta") && j["captureMeta"].is_object()) {
            captureMeta_ = j["captureMeta"];
        }
        allMowRoute_ = buildMowRoutePlan(parsedMowPoints);
        activateMowRoute(allMowRoute_);
        dockRoute_ = buildDockRoutePlan(parsedDockPoints);
        for (auto& routePoint : dockRoute_.points) {
            routePoint.clearance_m = routeClearanceForPoint(routePoint.p, WayType::DOCK);
            routePoint.reverseAllowed = routeReverseAllowedForPoint(routePoint.p, WayType::DOCK);
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
        const nlohmann::json j = exportMapJson();

        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

nlohmann::json Map::exportMapJson() const {
    nlohmann::json j;
    j["perimeter"] = encodePoints(perimeterPoints_);
    j["mow"] = encodeMowPoints(exportMowPoints(mowRoute_));
    j["dock"] = encodePoints(exportPolygonPoints(dockRoute_));
    j["exclusions"] = nlohmann::json::array();
    for (const auto& ex : exclusions_) j["exclusions"].push_back(encodePoints(ex));

    j["zones"] = nlohmann::json::array();
    for (const auto& z : zones_) {
        nlohmann::json jz;
        jz["id"] = z.id;
        jz["order"] = z.order;
        jz["polygon"] = encodePoints(z.polygon);
        jz["settings"] = {
            { "name", z.settings.name },
            { "stripWidth", z.settings.stripWidth },
            { "angle", z.settings.angle },
            { "edgeMowing", z.settings.edgeMowing },
            { "edgeRounds", z.settings.edgeRounds },
            { "speed", z.settings.speed },
            { "pattern", z.settings.pattern == ZonePattern::SPIRAL ? "spiral" : "stripe" },
            { "reverseAllowed", z.settings.reverseAllowed },
            { "clearance", z.settings.clearance },
        };
        j["zones"].push_back(jz);
    }

    j["planner"] = encodePlannerSettings(planner_);
    j["dockMeta"] = encodeDockMeta(dockMeta_);
    if (!exclusionMeta_.empty()) {
        j["exclusionMeta"] = encodeExclusionMeta(exclusionMeta_);
    }
    if (!captureMeta_.empty()) {
        j["captureMeta"] = captureMeta_;
    }
    return j;
}

void Map::clear() {
    perimeterPoints_.clear();
    freePoints_.clear();
    allMowRoute_ = RoutePlan{};
    mowRoute_ = RoutePlan{};
    dockRoute_ = RoutePlan{};
    localRoute_ = RoutePlan{};
    exclusions_.clear();
    exclusionMeta_.clear();
    obstacles_.clear();
    zones_.clear();
    applyConfigDefaults();
    freeRoute_ = FreeRouteContext{};
    lastReplanAttempt_ms_ = 0;
    captureMeta_ = nlohmann::json::object();
    mowPointsIdx  = 0;
    dockPointsIdx = 0;
    freePointsIdx = 0;
    shouldDock_ = false;
    shouldMow_  = false;
    isDocked_   = false;
    mapCRC_     = 0;
}

RoutePlan Map::buildMowRoutePlan(const std::vector<MowPoint>& points) {
    RoutePlan route;
    route.sourceMode = WayType::MOW;
    route.active = !points.empty();
    route.points.reserve(points.size());
    for (const auto& mp : points) {
        RoutePoint routePoint;
        routePoint.p = mp.p;
        routePoint.reverse = mp.rev;
        routePoint.slow = mp.slow;
        routePoint.reverseAllowed = false;
        routePoint.clearance_m = 0.25f;
        routePoint.sourceMode = WayType::MOW;
        route.points.push_back(routePoint);
    }
    return route;
}

RoutePlan Map::buildDockRoutePlan(const PolygonPoints& points) {
    RoutePlan route;
    route.sourceMode = WayType::DOCK;
    route.active = !points.empty();
    route.points.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        RoutePoint routePoint;
        routePoint.p = points[i];
        routePoint.reverse = false;
        routePoint.slow = i >= (points.size() > 2 ? points.size() - 2 : 0);
        routePoint.reverseAllowed = false;
        routePoint.clearance_m = 0.25f;
        routePoint.sourceMode = WayType::DOCK;
        route.points.push_back(routePoint);
    }
    return route;
}

void Map::activateMowRoute(const RoutePlan& route) {
    mowRoute_ = route;
    mowRoute_.sourceMode = WayType::MOW;
    mowRoute_.active = !mowRoute_.points.empty();
    for (auto& routePoint : mowRoute_.points) {
        routePoint.sourceMode = WayType::MOW;
        routePoint.clearance_m = routeClearanceForPoint(routePoint.p, WayType::MOW);
        routePoint.reverseAllowed = routeReverseAllowedForPoint(routePoint.p, WayType::MOW);
        if (!routePoint.reverseAllowed) routePoint.reverse = false;
    }
}

void Map::applyConfigDefaults() {
    planner_ = PlannerSettings{};
    dockMeta_ = DockMeta{};

    if (!config_) return;

    planner_.defaultClearance_m = config_->get<float>("planner_default_clearance_m", planner_.defaultClearance_m);
    planner_.perimeterSoftMargin_m = config_->get<float>("planner_perimeter_soft_margin_m", planner_.perimeterSoftMargin_m);
    planner_.perimeterHardMargin_m = config_->get<float>("planner_perimeter_hard_margin_m", planner_.perimeterHardMargin_m);
    planner_.obstacleInflation_m = config_->get<float>("planner_obstacle_inflation_m", planner_.obstacleInflation_m);
    planner_.softNoGoCostScale = config_->get<float>("planner_soft_nogo_cost_scale", planner_.softNoGoCostScale);
    planner_.replanPeriod_ms = config_->get<unsigned long>("planner_replan_period_ms", planner_.replanPeriod_ms);
    planner_.gridCellSize_m = config_->get<float>("planner_grid_cell_size_m", planner_.gridCellSize_m);

    dockMeta_.approachMode = parseDockApproachMode(
        config_->get<std::string>("dock_approach_mode", encodeDockApproachMode(dockMeta_.approachMode)));
    dockMeta_.slowZoneRadius_m = config_->get<float>("dock_slow_zone_radius_m", dockMeta_.slowZoneRadius_m);
    dockMeta_.hasFinalAlignHeading = config_->get<bool>("dock_final_align_heading_enabled", false);
    if (dockMeta_.hasFinalAlignHeading) {
        dockMeta_.finalAlignHeading_deg = config_->get<float>(
            "dock_final_align_heading_deg",
            dockMeta_.finalAlignHeading_deg);
    }
}

long Map::calcCRC() const {
    long crc = 0;
    auto accum = [&](const PolygonPoints& pts) {
        for (auto& p : pts) {
            crc ^= static_cast<long>(p.x * 1000) ^ static_cast<long>(p.y * 1000);
        }
    };
    accum(perimeterPoints_);
    for (const auto& routePoint : mowRoute_.points) {
        crc ^= static_cast<long>(routePoint.p.x * 1000) ^ static_cast<long>(routePoint.p.y * 1000);
    }
    accum(exportPolygonPoints(dockRoute_));
    for (auto& ex : exclusions_) accum(ex);
    return crc;
}

// ── Mission control ────────────────────────────────────────────────────────────

bool Map::startMowing(float robotX, float robotY) {
    if (!allMowRoute_.points.empty()) {
        activateMowRoute(allMowRoute_);
    }
    if (mowRoute_.points.empty()) return false;

    // Find nearest mow point to current position.
    float bestDist = 1e9f;
    int   bestIdx  = 0;
    for (int i = 0; i < static_cast<int>(mowRoute_.points.size()); ++i) {
        float d = Point(robotX, robotY).distanceTo(mowRoute_.points[i].p);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }

    mowPointsIdx = bestIdx;
    shouldMow_   = true;
    shouldDock_  = false;
    wayMode      = WayType::MOW;

    setLastTargetPoint(robotX, robotY);
    trackReverse = mowRoute_.points[mowPointsIdx].reverse;
    trackSlow    = mowRoute_.points[mowPointsIdx].slow;
    targetPoint  = mowRoute_.points[mowPointsIdx].p;
    return true;
}

bool Map::startMowingZones(float robotX, float robotY, const std::vector<std::string>& zoneIds) {
    if (zoneIds.empty()) return startMowing(robotX, robotY);
    if (allMowRoute_.points.empty()) return startMowing(robotX, robotY);

    // Build set of requested zone IDs for fast lookup
    std::unordered_set<std::string> requested(zoneIds.begin(), zoneIds.end());

    // Collect mow points that fall inside any of the requested zone polygons
    std::vector<MowPoint> filtered;
    for (const auto& routePoint : allMowRoute_.points) {
        for (const auto& zone : zones_) {
            if (requested.count(zone.id) && pointInPolygon(zone.polygon, routePoint.p.x, routePoint.p.y)) {
                filtered.push_back(MowPoint{routePoint.p, routePoint.reverse, routePoint.slow});
                break;
            }
        }
    }

    if (filtered.empty()) return startMowing(robotX, robotY);  // fallback: all zones

    // Activate filtered subset for this mowing mission.
    activateMowRoute(buildMowRoutePlan(filtered));
    if (mowRoute_.points.empty()) return false;

    float bestDist = 1e9f;
    int   bestIdx  = 0;
    for (int i = 0; i < static_cast<int>(mowRoute_.points.size()); ++i) {
        float d = Point(robotX, robotY).distanceTo(mowRoute_.points[i].p);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }

    mowPointsIdx = bestIdx;
    shouldMow_   = true;
    shouldDock_  = false;
    wayMode      = WayType::MOW;

    setLastTargetPoint(robotX, robotY);
    trackReverse = mowRoute_.points[mowPointsIdx].reverse;
    trackSlow    = mowRoute_.points[mowPointsIdx].slow;
    targetPoint  = mowRoute_.points[mowPointsIdx].p;
    return true;
}

bool Map::startDocking(float robotX, float robotY) {
    if (dockRoute_.points.empty()) return false;

    // Route from current position to first dock point, avoiding obstacles (C.7).
    const Point approachTarget = dockApproachTarget();
    const bool havePath = findPath({robotX, robotY}, approachTarget);
    if (!havePath || freePoints_.empty()) {
        if (!isInsideAllowedArea(robotX, robotY)) {
            dockPointsIdx = 0;
            shouldDock_   = true;
            shouldMow_    = false;
            wayMode       = WayType::DOCK;
            freeRoute_    = FreeRouteContext{};
            localRoute_   = RoutePlan{};
            freePoints_.clear();
            freePointsIdx = 0;
            setLastTargetPoint(robotX, robotY);
            trackReverse = dockRoute_.points[dockPointsIdx].reverse;
            trackSlow    = true;
            targetPoint  = dockRoute_.points[dockPointsIdx].p;
            return true;
        }
        return false;
    }

    dockPointsIdx = 0;
    shouldDock_   = true;
    shouldMow_    = false;
    setLastTargetPoint(robotX, robotY);
    beginFreeRoute(WayType::DOCK,
                   dockRoute_.points.front().p,
                   false,
                   (dockRoute_.points.size() <= 2),
                   mowPointsIdx,
                   0,
                   localRoute_);
    return true;
}

bool Map::retryDocking(float robotX, float robotY, float lateralOffsetM) {
    if (std::fabs(lateralOffsetM) < 1e-4f) {
        return startDocking(robotX, robotY);
    }

    if (dockRoute_.points.size() < 2) {
        return startDocking(robotX, robotY);
    }

    const Point& last = dockRoute_.points.back().p;
    const Point& penultimate = dockRoute_.points[dockRoute_.points.size() - 2].p;
    const float dx = last.x - penultimate.x;
    const float dy = last.y - penultimate.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) {
        return startDocking(robotX, robotY);
    }

    const Point baseApproach = dockApproachTarget();
    const float nx = (-dy / len) * lateralOffsetM;
    const float ny = ( dx / len) * lateralOffsetM;
    const Point offsetApproach{baseApproach.x + nx, baseApproach.y + ny};

    if (!findPath({robotX, robotY}, offsetApproach) || freePoints_.empty()) {
        return startDocking(robotX, robotY);
    }

    dockPointsIdx = 0;
    shouldDock_   = true;
    shouldMow_    = false;
    setLastTargetPoint(robotX, robotY);
    beginFreeRoute(WayType::DOCK,
                   dockRoute_.points.front().p,
                   false,
                   (dockRoute_.points.size() <= 2),
                   mowPointsIdx,
                   0,
                   localRoute_);
    return true;
}

bool Map::isDocking() const {
    return shouldDock_ && (wayMode == WayType::DOCK || wayMode == WayType::FREE);
}

bool Map::isUndocking() const {
    return !shouldDock_ && (wayMode == WayType::DOCK);
}

// ── Waypoint navigation ────────────────────────────────────────────────────────

bool Map::nextPoint(bool sim, float robotX, float robotY) {
    (void)robotX;
    (void)robotY;
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
            const float r = obs.radius_m + planner_.obstacleInflation_m;
            if (lineCircleIntersects(targetPoint, lastTargetPoint, obs.center, r)) {
                const WayType resumeMode = wayMode;
                const Point resumeTarget = targetPoint;
                const bool resumeTrackReverse = trackReverse;
                const bool resumeTrackSlow = trackSlow;
                const int resumeMowIdx = mowPointsIdx;
                const int resumeDockIdx = dockPointsIdx;

                if (findPath(lastTargetPoint, targetPoint)) {
                    beginFreeRoute(resumeMode,
                                   resumeTarget,
                                   resumeTrackReverse,
                                   resumeTrackSlow,
                                   resumeMowIdx,
                                   resumeDockIdx,
                                   localRoute_);
                }
                break;
            }
        }
    }
    return ok;
}

bool Map::nextMowPoint(bool sim) {
    if (mowRoute_.points.empty()) return false;
    ++mowPointsIdx;
    if (mowPointsIdx >= static_cast<int>(mowRoute_.points.size())) {
        mowPointsIdx = static_cast<int>(mowRoute_.points.size()) - 1;
        percentCompleted = 100;
        return false;
    }
    percentCompleted = static_cast<int>(100.0f * mowPointsIdx / mowRoute_.points.size());
    lastTargetPoint = targetPoint;
    trackReverse    = mowRoute_.points[mowPointsIdx].reverse;
    trackSlow       = mowRoute_.points[mowPointsIdx].slow;
    targetPoint     = mowRoute_.points[mowPointsIdx].p;
    return true;
}

bool Map::nextDockPoint(bool sim) {
    if (dockRoute_.points.empty()) return false;
    ++dockPointsIdx;
    if (dockPointsIdx >= static_cast<int>(dockRoute_.points.size())) return false;
    lastTargetPoint = targetPoint;
    targetPoint     = dockRoute_.points[dockPointsIdx].p;
    trackReverse    = dockRoute_.points[dockPointsIdx].reverse;
    trackSlow       = dockRoute_.points[dockPointsIdx].slow;
    return true;
}

bool Map::nextFreePoint(bool sim) {
    (void)sim;
    if (freePoints_.empty()) return false;
    ++freePointsIdx;
    if (freePointsIdx >= static_cast<int>(freePoints_.size())) {
        if (!freeRoute_.active) return false;

        lastTargetPoint = targetPoint;
        wayMode         = freeRoute_.resumeMode;
        targetPoint     = freeRoute_.resumeTarget;
        trackReverse    = freeRoute_.resumeTrackReverse;
        trackSlow       = freeRoute_.resumeTrackSlow;
        mowPointsIdx    = freeRoute_.resumeMowIdx;
        dockPointsIdx   = freeRoute_.resumeDockIdx;
        freeRoute_      = FreeRouteContext{};
        localRoute_      = RoutePlan{};
        freePoints_.clear();
        freePointsIdx = 0;
        return true;
    }
    lastTargetPoint = targetPoint;
    targetPoint     = localRoute_.points[freePointsIdx].p;
    trackReverse    = localRoute_.points[freePointsIdx].reverse;
    trackSlow       = localRoute_.points[freePointsIdx].slow;
    return true;
}

bool Map::nextPointIsStraight() const {
    // Check angle change between current and next segment.
    Point nextPt;
    bool  hasNext = false;

    if (wayMode == WayType::MOW && mowPointsIdx + 1 < static_cast<int>(mowRoute_.points.size())) {
        nextPt  = mowRoute_.points[mowPointsIdx + 1].p;
        hasNext = true;
    } else if (wayMode == WayType::DOCK && dockPointsIdx + 1 < static_cast<int>(dockRoute_.points.size())) {
        nextPt  = dockRoute_.points[dockPointsIdx + 1].p;
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
    if (dockRoute_.points.empty()) return false;
    int i = (idx < 0) ? static_cast<int>(dockRoute_.points.size()) - 1 : idx;
    i = std::clamp(i, 0, static_cast<int>(dockRoute_.points.size()) - 1);

    x = dockRoute_.points[i].p.x;
    y = dockRoute_.points[i].p.y;

    // Heading: angle from previous dock point to this one.
    if (i > 0) {
        delta = pointsAngle(dockRoute_.points[i - 1].p.x, dockRoute_.points[i - 1].p.y, x, y);
    } else {
        delta = 0.0f;
    }
    return true;
}

bool Map::mowingCompleted() const {
    return !mowRoute_.points.empty()
        && mowPointsIdx >= static_cast<int>(mowRoute_.points.size()) - 1;
}

void Map::setMowingPointPercent(float pct) {
    if (mowRoute_.points.empty()) return;
    pct = std::clamp(pct, 0.0f, 100.0f);
    mowPointsIdx = static_cast<int>(pct / 100.0f * (mowRoute_.points.size() - 1));
}

void Map::skipNextMowingPoint() {
    if (mowPointsIdx + 1 < static_cast<int>(mowRoute_.points.size())) {
        ++mowPointsIdx;
        trackReverse = mowRoute_.points[mowPointsIdx].reverse;
        trackSlow    = mowRoute_.points[mowPointsIdx].slow;
        targetPoint  = mowRoute_.points[mowPointsIdx].p;
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

const ZoneSettings* Map::zoneSettingsForPoint(float x, float y) const {
    for (const auto& zone : zones_) {
        if (pointInPolygon(zone.polygon, x, y)) {
            return &zone.settings;
        }
    }
    return nullptr;
}

float Map::routeClearanceForPoint(const Point& point, WayType mode) const {
    if (mode == WayType::MOW || mode == WayType::FREE) {
        if (const auto* settings = zoneSettingsForPoint(point.x, point.y)) {
            return std::max(0.05f, settings->clearance);
        }
    }
    return std::max(0.05f, planner_.defaultClearance_m);
}

bool Map::routeReverseAllowedForPoint(const Point& point, WayType mode) const {
    if (mode == WayType::DOCK) {
        return dockMeta_.approachMode == DockApproachMode::REVERSE_ALLOWED;
    }
    if (mode == WayType::MOW || mode == WayType::FREE) {
        if (const auto* settings = zoneSettingsForPoint(point.x, point.y)) {
            return settings->reverseAllowed;
        }
    }
    return false;
}

std::string Map::zoneIdForPoint(float x, float y,
                                const std::vector<std::string>& preferredOrder) const {
    auto findPreferred = [this, x, y](const std::string& zoneId) -> std::string {
        for (const auto& zone : zones_) {
            if (zone.id == zoneId && pointInPolygon(zone.polygon, x, y)) {
                return zone.id;
            }
        }
        return {};
    };

    for (const auto& zoneId : preferredOrder) {
        const std::string match = findPreferred(zoneId);
        if (!match.empty()) return match;
    }

    for (const auto& zone : zones_) {
        if (pointInPolygon(zone.polygon, x, y)) return zone.id;
    }
    return {};
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

bool Map::segmentAllowed(const Point& src, const Point& dst) const {
    if (!isInsideAllowedArea(src.x, src.y) || !isInsideAllowedArea(dst.x, dst.y)) return false;

    if (!perimeterPoints_.empty() && linePolygonIntersection(src, dst, perimeterPoints_)) {
        const Point mid{(src.x + dst.x) * 0.5f, (src.y + dst.y) * 0.5f};
        if (!isInsideAllowedArea(mid.x, mid.y)) return false;
    }

    for (const auto& ex : exclusions_) {
        if (linePolygonIntersection(src, dst, ex)) return false;
    }

    for (const auto& obs : obstacles_) {
        const float radius = obs.radius_m + planner_.obstacleInflation_m;
        if (lineCircleIntersects(src, dst, obs.center, radius)) return false;
    }

    return true;
}

bool Map::pathAllowed(const std::vector<Point>& path) const {
    if (path.size() < 2) return false;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        if (!segmentAllowed(path[i], path[i + 1])) return false;
    }
    return true;
}

Point Map::dockApproachTarget() const {
    if (dockMeta_.corridor.empty()) return dockRoute_.points.front().p;

    Point center;
    for (const auto& p : dockMeta_.corridor) {
        center.x += p.x;
        center.y += p.y;
    }
    const float denom = static_cast<float>(dockMeta_.corridor.size());
    center.x /= denom;
    center.y /= denom;
    return center;
}

RouteSegment Map::currentTrackingSegment(float robotX, float robotY, float robotHeadingRad) const {
    RouteSegment segment;
    segment.lastTarget = lastTargetPoint;
    segment.target = targetPoint;
    segment.reverse = trackReverse;
    segment.slow = trackSlow;
    segment.sourceMode = wayMode;
    segment.clearance_m = planner_.defaultClearance_m;
    segment.reverseAllowed = false;

    if (wayMode == WayType::FREE && freePointsIdx >= 0 && freePointsIdx < static_cast<int>(localRoute_.points.size())) {
        segment.reverse = localRoute_.points[freePointsIdx].reverse;
        segment.slow = localRoute_.points[freePointsIdx].slow;
        segment.reverseAllowed = localRoute_.points[freePointsIdx].reverseAllowed;
        segment.clearance_m = localRoute_.points[freePointsIdx].clearance_m;
        segment.sourceMode = localRoute_.points[freePointsIdx].sourceMode;
    } else if (wayMode == WayType::MOW && mowPointsIdx >= 0 && mowPointsIdx < static_cast<int>(mowRoute_.points.size())) {
        segment.reverseAllowed = mowRoute_.points[mowPointsIdx].reverseAllowed;
        segment.clearance_m = mowRoute_.points[mowPointsIdx].clearance_m;
    } else if (wayMode == WayType::DOCK && dockPointsIdx >= 0 && dockPointsIdx < static_cast<int>(dockRoute_.points.size())) {
        segment.reverseAllowed = dockRoute_.points[dockPointsIdx].reverseAllowed;
        segment.clearance_m = dockRoute_.points[dockPointsIdx].clearance_m;
    }

    const bool finalAlignActive = dockingFinalAlignActive(robotX, robotY);
    if (finalAlignActive) {
        segment.reverse = dockingShouldReverseOnFinalApproach(robotX, robotY, robotHeadingRad);
        segment.slow = true;
        segment.sourceMode = WayType::DOCK;
        segment.reverseAllowed = dockingReverseAllowed();
        segment.hasTargetHeading = true;
        segment.targetHeadingRad = dockingFinalAlignHeadingRad();
    }

    segment.distanceToTarget_m = Point(robotX, robotY).distanceTo(segment.target);
    segment.distanceToLastTarget_m = Point(robotX, robotY).distanceTo(segment.lastTarget);
    segment.targetReached = segment.distanceToTarget_m < TRACK_TARGET_REACHED_TOLERANCE_M;
    segment.targetReachedTolerance_m = TRACK_TARGET_REACHED_TOLERANCE_M;
    if (!segment.hasTargetHeading) {
        segment.targetHeadingRad = std::atan2(segment.target.y - robotY, segment.target.x - robotX);
        segment.hasTargetHeading = true;
    }
    segment.nextSegmentStraight = nextPointIsStraight();
    segment.kidnapTolerance_m = kidnapToleranceForMode(segment.sourceMode);
    return segment;
}

bool Map::refreshTracking(unsigned long now_ms, float robotX, float robotY) {
    return maybeReplanToCurrentTarget(now_ms, robotX, robotY);
}

bool Map::advanceTracking(float robotX, float robotY) {
    return nextPoint(false, robotX, robotY);
}

bool Map::dockingFinalAlignActive(float robotX, float robotY) const {
    if (!shouldDock_ || dockRoute_.points.empty() || !dockMeta_.hasFinalAlignHeading) return false;
    if (wayMode != WayType::DOCK && wayMode != WayType::FREE) return false;

    const Point dockGoal = dockRoute_.points.back().p;
    const float activationRadius = std::max(0.15f, dockMeta_.slowZoneRadius_m);
    return (Point(robotX, robotY).distanceTo(dockGoal) <= activationRadius)
        || (targetPoint.distanceTo(dockGoal) <= activationRadius);
}

float Map::dockingFinalAlignHeadingRad() const {
    return dockMeta_.finalAlignHeading_deg / 180.0f * static_cast<float>(M_PI);
}

bool Map::dockingReverseAllowed() const {
    return shouldDock_ && (dockMeta_.approachMode == DockApproachMode::REVERSE_ALLOWED);
}

bool Map::dockingShouldReverseOnFinalApproach(float robotX, float robotY, float robotHeadingRad) const {
    if (!dockingReverseAllowed() || !dockingFinalAlignActive(robotX, robotY)) return false;

    const float forwardHeading = dockingFinalAlignHeadingRad();
    const float reverseHeading = scalePI(forwardHeading + static_cast<float>(M_PI));
    const float forwardError = std::fabs(distancePI(robotHeadingRad, forwardHeading));
    const float reverseError = std::fabs(distancePI(robotHeadingRad, reverseHeading));
    const float switchingMargin = 10.0f / 180.0f * static_cast<float>(M_PI);
    return (reverseError + switchingMargin) < forwardError;
}

float Map::kidnapToleranceForMode(WayType mode) const {
    switch (mode) {
        case WayType::DOCK:
            return std::max(0.6f, dockMeta_.slowZoneRadius_m + 0.25f);
        case WayType::FREE:
            return std::max(1.0f, planner_.defaultClearance_m + planner_.obstacleInflation_m + 0.5f);
        case WayType::MOW:
        default:
            return 3.0f;
    }
}

bool Map::currentSegmentNeedsReplan(float robotX, float robotY) const {
    PlannerContext context;
    context.robotPose = {robotX, robotY};
    context.source = {robotX, robotY};
    context.destination = targetPoint;
    context.missionMode = wayMode;
    context.planningMode = (shouldDock_ || wayMode == WayType::DOCK) ? WayType::DOCK : WayType::MOW;
    context.robotRadius_m = planner_.defaultClearance_m + planner_.obstacleInflation_m;
    return Planner::segmentNeedsReplan(*this, context);
}

void Map::beginFreeRoute(WayType resumeMode,
                         const Point& resumeTarget,
                         bool resumeTrackReverse,
                         bool resumeTrackSlow,
                         int resumeMowIdx,
                         int resumeDockIdx,
                         const RoutePlan& route) {
    if (route.points.empty()) return;

    freeRoute_.active             = true;
    freeRoute_.resumeMode         = resumeMode;
    freeRoute_.resumeTarget       = resumeTarget;
    freeRoute_.resumeTrackReverse = resumeTrackReverse;
    freeRoute_.resumeTrackSlow    = resumeTrackSlow;
    freeRoute_.resumeMowIdx       = resumeMowIdx;
    freeRoute_.resumeDockIdx      = resumeDockIdx;

    localRoute_ = route;
    localRoute_.sourceMode = resumeMode;
    localRoute_.active = true;
    freePoints_.clear();
    freePoints_.reserve(localRoute_.points.size());
    for (const auto& routePoint : localRoute_.points) {
        freePoints_.push_back(routePoint.p);
    }

    freePointsIdx = 0;
    wayMode       = WayType::FREE;
    trackReverse  = localRoute_.points.front().reverse;
    trackSlow     = localRoute_.points.front().slow;
    targetPoint   = localRoute_.points.front().p;
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

    RoutePlan route;
    route.sourceMode = freeRoute_.active ? freeRoute_.resumeMode : wayMode;
    route.active = true;
    route.points.reserve(waypoints.size());
    for (const auto& waypoint : waypoints) {
        RoutePoint routePoint;
        routePoint.p = waypoint;
        routePoint.sourceMode = route.sourceMode;
        routePoint.clearance_m = routeClearanceForPoint(waypoint, route.sourceMode);
        routePoint.reverseAllowed = routeReverseAllowedForPoint(waypoint, route.sourceMode);
        route.points.push_back(routePoint);
    }

    const WayType resumeMode = freeRoute_.active ? freeRoute_.resumeMode : wayMode;
    beginFreeRoute(resumeMode,
                   targetPoint,
                   trackReverse,
                   trackSlow,
                   mowPointsIdx,
                   dockPointsIdx,
                   route);
}

bool Map::findPath(const Point& src, const Point& dst) {
    freePoints_.clear();
    freePointsIdx = 0;

    PlannerContext context;
    context.robotPose = src;
    context.source = src;
    context.destination = dst;
    context.missionMode = wayMode;
    context.planningMode = (shouldDock_ || wayMode == WayType::DOCK) ? WayType::DOCK : WayType::MOW;
    context.clearance_m = routeClearanceForPoint(dst, context.missionMode);
    context.reverseAllowed = routeReverseAllowedForPoint(dst, context.missionMode);
    if (lastTargetPoint.distanceTo(src) > 1e-4f) {
        context.headingReferenceRad = pointsAngle(lastTargetPoint.x, lastTargetPoint.y, src.x, src.y);
        context.hasHeadingReference = true;
    }
    context.robotRadius_m = context.clearance_m + planner_.obstacleInflation_m;

    const RoutePlan route = Planner::planPath(*this, context);
    freePoints_.reserve(route.points.size());
    for (const auto& routePoint : route.points) {
        freePoints_.push_back(routePoint.p);
    }
    localRoute_ = route;
    return !freePoints_.empty();
}

bool Map::replanToCurrentTarget(float robotX, float robotY) {
    if (wayMode == WayType::FREE) return false;
    if (targetPoint.distanceTo({robotX, robotY}) < 1e-4f) return false;

    const WayType resumeMode = wayMode;
    const Point resumeTarget = targetPoint;
    const bool resumeTrackReverse = trackReverse;
    const bool resumeTrackSlow = trackSlow;
    const int resumeMowIdx = mowPointsIdx;
    const int resumeDockIdx = dockPointsIdx;

    if (!findPath({robotX, robotY}, targetPoint) || freePoints_.empty()) return false;

    setLastTargetPoint(robotX, robotY);
    beginFreeRoute(resumeMode,
                   resumeTarget,
                   resumeTrackReverse,
                   resumeTrackSlow,
                   resumeMowIdx,
                   resumeDockIdx,
                   localRoute_);
    return true;
}

bool Map::maybeReplanToCurrentTarget(unsigned long now_ms, float robotX, float robotY) {
    if (wayMode == WayType::FREE) return false;
    if (now_ms - lastReplanAttempt_ms_ < planner_.replanPeriod_ms) return false;
    if (!currentSegmentNeedsReplan(robotX, robotY)) return false;

    lastReplanAttempt_ms_ = now_ms;
    return replanToCurrentTarget(robotX, robotY);
}

} // namespace nav
} // namespace sunray
