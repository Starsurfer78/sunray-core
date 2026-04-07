#include "core/navigation/MowRoutePlanner.h"

#include <clipper2/clipper.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace sunray {
namespace nav {

namespace {

struct Interval {
    float left = 0.0f;
    float right = 0.0f;
};

struct Segment {
    Point a;
    Point b;
};

struct PreparedCoverageArea {
    PolygonPoints zone;
    PolygonPoints perimeter;
    std::vector<PolygonPoints> exclusions;
};

static bool samePoint(const Point& a, const Point& b, float eps = 1e-4f) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
}

static Point rotatePoint(const Point& p, float cosA, float sinA) {
    return {
        p.x * cosA - p.y * sinA,
        p.x * sinA + p.y * cosA,
    };
}

static PolygonPoints rotatePolygon(const PolygonPoints& poly, float angleRad) {
    const float cosA = std::cos(angleRad);
    const float sinA = std::sin(angleRad);
    PolygonPoints out;
    out.reserve(poly.size());
    for (const auto& p : poly) out.push_back(rotatePoint(p, cosA, sinA));
    return out;
}

static void appendUniquePoint(PolygonPoints& pts, const Point& p, float eps = 1e-4f) {
    if (!pts.empty() && samePoint(pts.back(), p, eps)) return;
    pts.push_back(p);
}

static PolygonPoints normalizePolygon(const PolygonPoints& poly) {
    PolygonPoints out;
    out.reserve(poly.size());
    for (const auto& point : poly) {
        appendUniquePoint(out, point);
    }
    if (out.size() > 1 && samePoint(out.front(), out.back())) {
        out.pop_back();
    }
    return out.size() >= 3 ? out : PolygonPoints{};
}

static PreparedCoverageArea prepareCoverageArea(const Zone& zone, const Map& map) {
    PreparedCoverageArea prepared;
    prepared.zone = normalizePolygon(zone.polygon);
    prepared.perimeter = normalizePolygon(map.perimeterPoints());
    prepared.exclusions.reserve(map.exclusions().size());
    for (const auto& exclusion : map.exclusions()) {
        PolygonPoints normalized = normalizePolygon(exclusion);
        if (normalized.size() >= 3) prepared.exclusions.push_back(std::move(normalized));
    }
    return prepared;
}

// ── Polygon offset via Clipper2 ───────────────────────────────────────────────
// Clipper2 works in integer space. We scale by 1000 (mm precision).
static constexpr double kClipperScale = 1000.0;

static Clipper2Lib::PathD toClipperPath(const PolygonPoints& poly) {
    Clipper2Lib::PathD path;
    path.reserve(poly.size());
    for (const auto& p : poly) path.emplace_back(p.x, p.y);
    return path;
}

static PolygonPoints fromClipperPath(const Clipper2Lib::PathD& path) {
    PolygonPoints out;
    out.reserve(path.size());
    for (const auto& pt : path) out.push_back({static_cast<float>(pt.x), static_cast<float>(pt.y)});
    return out;
}

/// Inset (positive inset = shrink) via Clipper2 with miter join.
/// Returns the largest result polygon, or empty if the polygon vanishes.
static PolygonPoints offsetPolygon(const PolygonPoints& poly, float inset) {
    if (poly.size() < 3) return poly;
    if (std::fabs(inset) < 1e-6f) return poly;

    Clipper2Lib::PathsD paths = {toClipperPath(poly)};
    // Clipper2 InflatePaths: negative delta = inset
    const Clipper2Lib::PathsD result = Clipper2Lib::InflatePaths(
        paths,
        -static_cast<double>(inset),
        Clipper2Lib::JoinType::Miter,
        Clipper2Lib::EndType::Polygon);

    if (result.empty()) return {};
    // Return the largest polygon by signed area
    const Clipper2Lib::PathD* best = &result[0];
    double bestArea = std::fabs(Clipper2Lib::Area(*best));
    for (size_t i = 1; i < result.size(); ++i) {
        double a = std::fabs(Clipper2Lib::Area(result[i]));
        if (a > bestArea) { bestArea = a; best = &result[i]; }
    }
    return fromClipperPath(*best);
}

/// Inset with round join at convex corners (used for headland contours).
/// Returns the largest result polygon, or the original if inset is near zero.
static PolygonPoints offsetPolygonRounded(const PolygonPoints& poly, float inset) {
    if (poly.size() < 3) return poly;
    if (std::fabs(inset) < 1e-6f) return poly;

    Clipper2Lib::PathsD paths = {toClipperPath(poly)};
    const Clipper2Lib::PathsD result = Clipper2Lib::InflatePaths(
        paths,
        -static_cast<double>(inset),
        Clipper2Lib::JoinType::Round,
        Clipper2Lib::EndType::Polygon);

    if (result.empty()) return {};
    const Clipper2Lib::PathD* best = &result[0];
    double bestArea = std::fabs(Clipper2Lib::Area(*best));
    for (size_t i = 1; i < result.size(); ++i) {
        double a = std::fabs(Clipper2Lib::Area(result[i]));
        if (a > bestArea) { bestArea = a; best = &result[i]; }
    }
    return fromClipperPath(*best);
}

static std::vector<Interval> normalizeIntervals(std::vector<Interval> intervals) {
    if (intervals.empty()) return {};
    std::sort(intervals.begin(), intervals.end(), [](const Interval& a, const Interval& b) {
        if (a.left == b.left) return a.right < b.right;
        return a.left < b.left;
    });

    std::vector<Interval> merged;
    merged.push_back(intervals.front());
    for (size_t i = 1; i < intervals.size(); ++i) {
        Interval& last = merged.back();
        if (intervals[i].left <= last.right + 1e-6f) {
            last.right = std::max(last.right, intervals[i].right);
        } else {
            merged.push_back(intervals[i]);
        }
    }
    return merged;
}

static std::vector<Interval> intersectIntervals(const std::vector<Interval>& a,
                                                const std::vector<Interval>& b) {
    std::vector<Interval> out;
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        const float left = std::max(a[i].left, b[j].left);
        const float right = std::min(a[i].right, b[j].right);
        if (left <= right) out.push_back({left, right});
        if (a[i].right < b[j].right) {
            ++i;
        } else {
            ++j;
        }
    }
    return out;
}

static std::vector<Interval> subtractIntervals(const std::vector<Interval>& base,
                                               const std::vector<Interval>& cuts) {
    if (base.empty()) return {};
    if (cuts.empty()) return base;

    std::vector<Interval> out;
    size_t j = 0;
    for (const auto& b : base) {
        float cursor = b.left;
        while (j < cuts.size() && cuts[j].right < cursor) ++j;
        size_t k = j;
        while (k < cuts.size() && cuts[k].left <= b.right) {
            if (cuts[k].left > cursor) {
                out.push_back({cursor, std::min(b.right, cuts[k].left)});
            }
            cursor = std::max(cursor, cuts[k].right);
            if (cursor >= b.right) break;
            ++k;
        }
        if (cursor < b.right) out.push_back({cursor, b.right});
    }
    return normalizeIntervals(out);
}

static std::vector<Interval> polygonIntervalsAtY(const PolygonPoints& poly, float y) {
    std::vector<float> xs;
    if (poly.size() < 3) return {};
    xs.reserve(poly.size());
    for (size_t i = 0; i < poly.size(); ++i) {
        const Point& a = poly[i];
        const Point& b = poly[(i + 1) % poly.size()];
        if ((a.y < y && b.y >= y) || (b.y < y && a.y >= y)) {
            const float t = (y - a.y) / (b.y - a.y);
            xs.push_back(a.x + t * (b.x - a.x));
        }
    }
    std::sort(xs.begin(), xs.end());
    std::vector<Interval> out;
    for (size_t i = 0; i + 1 < xs.size(); i += 2) {
        if (xs[i + 1] > xs[i]) out.push_back({xs[i], xs[i + 1]});
    }
    return out;
}

static std::vector<Interval> effectiveIntervalsAtY(const PolygonPoints& zone,
                                                   const PolygonPoints& perimeter,
                                                   const std::vector<PolygonPoints>& exclusions,
                                                   float y) {
    std::vector<Interval> intervals = polygonIntervalsAtY(zone, y);
    if (!perimeter.empty()) {
        intervals = intersectIntervals(intervals, polygonIntervalsAtY(perimeter, y));
    }
    for (const auto& exclusion : exclusions) {
        intervals = subtractIntervals(intervals, polygonIntervalsAtY(exclusion, y));
        if (intervals.empty()) break;
    }
    return intervals;
}

// ── Coverage helpers ──────────────────────────────────────────────────────────
// These functions produce Coverage segments (stripes and headland contours).
// They do NOT insert free-path transitions — that is the Transition layer's job.

static std::vector<Segment> buildStripeSegments(const PolygonPoints& zone,
                                                const PolygonPoints& perimeter,
                                                const std::vector<PolygonPoints>& exclusions,
                                                float stripWidth,
                                                float angleDeg) {
    if (zone.size() < 3 || stripWidth <= 0.0f) return {};

    const float angle = angleDeg * static_cast<float>(M_PI) / 180.0f;
    const float cosPos = std::cos(angle);
    const float sinPos = std::sin(angle);

    const PolygonPoints rotZone = rotatePolygon(zone, -angle);
    const PolygonPoints rotPerimeter = perimeter.empty() ? PolygonPoints{} : rotatePolygon(perimeter, -angle);
    std::vector<PolygonPoints> rotExclusions;
    rotExclusions.reserve(exclusions.size());
    for (const auto& ex : exclusions) rotExclusions.push_back(rotatePolygon(ex, -angle));

    std::vector<Point> allPts = rotZone;
    allPts.insert(allPts.end(), rotPerimeter.begin(), rotPerimeter.end());
    for (const auto& ex : rotExclusions) allPts.insert(allPts.end(), ex.begin(), ex.end());
    if (allPts.empty()) return {};

    float minY = allPts.front().y;
    float maxY = allPts.front().y;
    for (const auto& p : allPts) {
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    // Build segments in strict row-by-row zigzag order (boustrophedon).
    // Do NOT apply sortSegmentsNearestNeighbour() here: global nearest-neighbour
    // reordering destroys the row sequence, forces long diagonal transitions and
    // can lead to stripes being missed entirely on complex zone shapes.
    std::vector<Segment> segments;
    int rowIdx = 0;
    for (float y = minY + stripWidth * 0.5f; y <= maxY + stripWidth * 0.01f; y += stripWidth, ++rowIdx) {
        std::vector<Interval> intervals = effectiveIntervalsAtY(rotZone, rotPerimeter, rotExclusions, y);
        for (const auto& interval : intervals) {
            const Point a = rotatePoint({interval.left, y}, cosPos, sinPos);
            const Point b = rotatePoint({interval.right, y}, cosPos, sinPos);
            segments.push_back(rowIdx % 2 == 0 ? Segment{a, b} : Segment{b, a});
        }
    }

    return segments;
}

static RoutePlan routeFromPolyline(const PolygonPoints& points,
                                   const ZoneSettings& settings,
                                   RouteSemantic semantic = RouteSemantic::COVERAGE_EDGE,
                                   const std::string& zoneId = {}) {
    RoutePlan route;
    route.sourceMode = WayType::MOW;
    route.active = points.size() >= 2;
    if (points.size() < 2) return route;

    for (const auto& p : points) {
        RoutePoint rp;
        rp.p = p;
        rp.reverse = false;
        rp.slow = false;
        rp.reverseAllowed = settings.reverseAllowed;
        rp.clearance_m = settings.clearance;
        rp.sourceMode = WayType::MOW;
        rp.semantic = semantic;
        rp.zoneId = zoneId;
        route.points.push_back(rp);
    }
    return route;
}

static RoutePlan appendRoute(RoutePlan base, const RoutePlan& add) {
    if (add.points.empty()) return base;
    if (base.points.empty()) return add;

    if (!samePoint(base.points.back().p, add.points.front().p)) {
        base.points.push_back(add.points.front());
    }
    for (size_t i = 0; i < add.points.size(); ++i) {
        if (!base.points.empty() && samePoint(base.points.back().p, add.points[i].p)) continue;
        base.points.push_back(add.points[i]);
    }
    base.active = !base.points.empty();
    // Propagate invalidity from either side
    if (!add.valid) {
        base.valid = false;
        if (base.invalidReason.empty()) base.invalidReason = add.invalidReason;
    }
    return base;
}

// ── Transition helpers ────────────────────────────────────────────────────────
// These functions insert A*-planned transitions between Coverage blocks.
// Transitions are zone-aware: TRANSIT_WITHIN_ZONE paths are constrained to the
// active zone polygon; TRANSIT_INTER_ZONE paths have no zone constraint.

static void appendTransition(RoutePlan& route,
                             const Map& map,
                             const Point& from,
                             const Point& to,
                             const ZoneSettings& settings,
                             const PolygonPoints& zonePoly,
                             const std::string& zoneId = {});

/// Point-in-polygon test (ray-casting).
static bool pointInPolygonLocal(const PolygonPoints& poly, float px, float py) {
    bool inside = false;
    const size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const float xi = poly[i].x, yi = poly[i].y;
        const float xj = poly[j].x, yj = poly[j].y;
        if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

/// Clip contour to runs that stay inside the zone polygon and outside exclusions.
/// Uses the zone polygon directly instead of the global isInsideAllowedArea() (which
/// checks against the perimeter) so headland tracks are not incorrectly clipped when
/// the zone polygon overlaps or is coincident with the perimeter boundary.
static std::vector<PolygonPoints> clipContourToAllowedRuns(
    const PolygonPoints& contour,
    const PolygonPoints& zonePoly,
    const std::vector<PolygonPoints>& exclusions) {

    std::vector<PolygonPoints> runs;
    if (contour.size() < 2) return runs;

    PolygonPoints current;
    const size_t n = contour.size();
    for (size_t i = 0; i < n; ++i) {
        const Point& a = contour[i];
        const Point& b = contour[(i + 1) % n];
        const float distance = a.distanceTo(b);
        const int steps = std::max(1, static_cast<int>(std::ceil(distance / 0.05f)));

        for (int step = 0; step <= steps; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            const Point p{
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
            };
            // Allow if inside the zone polygon and not inside any exclusion.
            bool allowed = pointInPolygonLocal(zonePoly, p.x, p.y);
            if (allowed) {
                for (const auto& ex : exclusions) {
                    if (pointInPolygonLocal(ex, p.x, p.y)) { allowed = false; break; }
                }
            }
            if (allowed) {
                appendUniquePoint(current, p, 1e-3f);
            } else if (current.size() >= 2) {
                runs.push_back(current);
                current.clear();
            } else {
                current.clear();
            }
        }
    }

    if (current.size() >= 2) {
        runs.push_back(current);
    }
    return runs;
}

static void appendSegmentRoute(RoutePlan& route,
                               const Map& map,
                               const Segment& segment,
                               const ZoneSettings& settings,
                               const PolygonPoints& zonePoly,
                               const std::string& zoneId = {}) {
    auto makePoint = [&](const Point& p) {
        RoutePoint rp;
        rp.p = p;
        rp.reverse = false;
        rp.slow = false;
        rp.reverseAllowed = settings.reverseAllowed;
        rp.clearance_m = settings.clearance;
        rp.sourceMode = WayType::MOW;
        rp.semantic = RouteSemantic::COVERAGE_INFILL;
        rp.zoneId = zoneId;
        return rp;
    };

    if (route.points.empty()) {
        route.points.push_back(makePoint(segment.a));
        route.points.push_back(makePoint(segment.b));
        route.sourceMode = WayType::MOW;
        route.active = true;
        return;
    }

    appendTransition(route, map, route.points.back().p, segment.a, settings, zonePoly, zoneId);
    if (!samePoint(route.points.back().p, segment.a)) {
        route.points.push_back(makePoint(segment.a));
    }
    route.points.push_back(makePoint(segment.b));
    route.sourceMode = WayType::MOW;
    route.active = true;
}

static nav::ZoneSettings applyMissionOverrides(const nav::ZoneSettings& base,
                                               const nlohmann::json& overrides) {
    nav::ZoneSettings settings = base;
    if (!overrides.is_object()) return settings;
    settings.stripWidth = overrides.value("stripWidth", settings.stripWidth);
    settings.angle = overrides.value("angle", settings.angle);
    settings.edgeMowing = overrides.value("edgeMowing", settings.edgeMowing);
    settings.edgeRounds = overrides.value("edgeRounds", settings.edgeRounds);
    settings.speed = overrides.value("speed", settings.speed);
    const std::string pat = overrides.value("pattern", settings.pattern == ZonePattern::SPIRAL ? "spiral" : "stripe");
    settings.pattern = (pat == "spiral") ? ZonePattern::SPIRAL : ZonePattern::STRIPE;
    settings.reverseAllowed = overrides.value("reverseAllowed", settings.reverseAllowed);
    settings.clearance = overrides.value("clearance", settings.clearance);
    return settings;
}

/// Zone-aware transition: the A* grid is bounded to zonePoly so the path
/// cannot take shortcuts through areas outside the active zone.
/// If no path is found, route.valid is set to false — no silent direct-point fallback.
static void appendTransition(RoutePlan& route,
                             const Map& map,
                             const Point& from,
                             const Point& to,
                             const ZoneSettings& settings,
                             const PolygonPoints& zonePoly,
                             const std::string& zoneId) {
    if (samePoint(from, to)) return;
    const RouteSemantic sem = zonePoly.empty()
        ? RouteSemantic::TRANSIT_INTER_ZONE
        : RouteSemantic::TRANSIT_WITHIN_ZONE;

    const RoutePlan transition = map.previewPath(
        from,
        to,
        WayType::MOW,
        WayType::MOW,
        0.0f,
        false,
        settings.reverseAllowed,
        settings.clearance,
        settings.clearance + map.plannerSettings().obstacleInflation_m,
        zonePoly);

    if (transition.points.empty()) {
        // No A* path found. Mark the route as invalid so the validator and the
        // mission start gate can detect and reject it. Do NOT insert a direct
        // point: that would create a silent shortcut through obstacles or the
        // zone boundary that is invisible to the RouteValidator.
        route.valid = false;
        if (route.invalidReason.empty()) {
            route.invalidReason = "no transition path from ("
                + std::to_string(from.x) + "," + std::to_string(from.y)
                + ") to ("
                + std::to_string(to.x) + "," + std::to_string(to.y) + ")"
                + (zoneId.empty() ? "" : " in zone '" + zoneId + "'");
        }
        return;
    }

    for (auto point : transition.points) {
        if (!route.points.empty() && samePoint(route.points.back().p, point.p)) continue;
        point.semantic = sem;
        point.zoneId = zoneId;
        route.points.push_back(point);
    }
}

static void appendSegmentsWithTransitions(RoutePlan& route,
                                          const Map& map,
                                          const std::vector<Segment>& segments,
                                          const ZoneSettings& settings,
                                          const PolygonPoints& zonePoly,
                                          const std::string& zoneId = {}) {
    for (const auto& segment : segments) {
        appendSegmentRoute(route, map, segment, settings, zonePoly, zoneId);
    }
}

static void appendContourRuns(RoutePlan& route,
                              const Map& map,
                              const std::vector<PolygonPoints>& runs,
                              const ZoneSettings& settings,
                              const PolygonPoints& zonePoly,
                              const std::string& zoneId = {}) {
    for (const auto& run : runs) {
        RoutePlan runRoute = routeFromPolyline(run, settings, RouteSemantic::COVERAGE_EDGE, zoneId);
        if (runRoute.points.empty()) continue;
        if (!route.points.empty()) {
            appendTransition(route, map, route.points.back().p, runRoute.points.front().p, settings, zonePoly, zoneId);
        }
        route = appendRoute(route, runRoute);
    }
}

} // namespace

RoutePlan buildMissionMowRoutePreview(const Map& map, const nlohmann::json& missionJson) {
    RoutePlan route;
    route.sourceMode = WayType::MOW;
    route.active = false;

    std::vector<std::string> requestedZoneIds;
    if (missionJson.contains("zoneIds") && missionJson["zoneIds"].is_array()) {
        for (const auto& id : missionJson["zoneIds"]) {
            if (id.is_string()) requestedZoneIds.push_back(id.get<std::string>());
        }
    }
    if (requestedZoneIds.empty()) {
        for (const auto& zone : map.zones()) requestedZoneIds.push_back(zone.id);
    }

    const auto applyForZone = [&](const Zone& zone) {
        const nlohmann::json overrides = missionJson.contains("overrides") && missionJson["overrides"].is_object()
            ? missionJson["overrides"].value(zone.id, nlohmann::json::object())
            : nlohmann::json::object();
        return applyMissionOverrides(zone.settings, overrides);
    };

    const auto findZone = [&](const std::string& id) -> const Zone* {
        for (const auto& zone : map.zones()) {
            if (zone.id == id) return &zone;
        }
        return nullptr;
    };

    for (const auto& zoneId : requestedZoneIds) {
        const Zone* zone = findZone(zoneId);
        if (!zone || zone->polygon.size() < 3) continue;

        const ZoneSettings settings = applyForZone(*zone);
        const PreparedCoverageArea prepared = prepareCoverageArea(*zone, map);
        if (prepared.zone.size() < 3) continue;
        RoutePlan zoneRoute;
        zoneRoute.sourceMode = WayType::MOW;
        zoneRoute.active = false;

        if (settings.edgeMowing && settings.edgeRounds > 0) {
            for (int round = 1; round <= settings.edgeRounds; ++round) {
                const float inset = (static_cast<float>(round) - 0.5f) * settings.stripWidth;
                const PolygonPoints contour = offsetPolygonRounded(prepared.zone, inset);
                if (contour.empty()) continue;
                // Pass zone polygon + exclusions so the headland contour is clipped
                // against the zone itself, not the global perimeter. This prevents
                // incorrect clipping when the zone polygon overlaps the perimeter.
                const std::vector<PolygonPoints> contourRuns =
                    clipContourToAllowedRuns(contour, prepared.zone, prepared.exclusions);
                if (!contourRuns.empty()) appendContourRuns(zoneRoute, map, contourRuns, settings, prepared.zone, zoneId);
            }
        }

        const float infillInset = std::max(0.0f, static_cast<float>(settings.edgeRounds) * settings.stripWidth);
        PolygonPoints infillZone = offsetPolygon(prepared.zone, infillInset);
        const std::vector<Segment> stripes = buildStripeSegments(
            infillZone.empty() ? prepared.zone : infillZone,
            prepared.perimeter,
            prepared.exclusions,
            settings.stripWidth,
            settings.angle);
        RoutePlan stripeRoute;
        stripeRoute.sourceMode = WayType::MOW;
        stripeRoute.active = false;
        appendSegmentsWithTransitions(stripeRoute, map, stripes, settings, prepared.zone, zoneId);
        if (!stripeRoute.points.empty()) {
            if (!zoneRoute.points.empty()) {
                appendTransition(zoneRoute, map, zoneRoute.points.back().p, stripeRoute.points.front().p, settings, prepared.zone, zoneId);
            }
            zoneRoute = appendRoute(zoneRoute, stripeRoute);
        }

        if (route.points.empty()) {
            route = zoneRoute;
        } else if (!zoneRoute.points.empty()) {
            // Inter-zone transition: no zone constraint (robot must cross between zones)
            appendTransition(route, map, route.points.back().p, zoneRoute.points.front().p, settings, {}, {});
            route = appendRoute(route, zoneRoute);
        }
    }

    route.active = !route.points.empty();
    return route;
}

} // namespace nav
} // namespace sunray
