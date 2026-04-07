#include "core/navigation/RouteValidator.h"

#include <cmath>
#include <sstream>

namespace sunray {
namespace nav {

// ── Geometry helpers (local) ──────────────────────────────────────────────────

bool RouteValidator::pointInPolygon(const PolygonPoints& poly, float px, float py) {
    bool inside = false;
    const size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const float xi = poly[i].x, yi = poly[i].y;
        const float xj = poly[j].x, yj = poly[j].y;
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

bool RouteValidator::segmentCrossesPolygon(const Point& a, const Point& b,
                                           const PolygonPoints& poly) {
    if (poly.size() < 2) return false;
    const auto crossProduct = [](const Point& o, const Point& u, const Point& v) {
        return (u.x - o.x) * (v.y - o.y) - (u.y - o.y) * (v.x - o.x);
    };
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const Point& c = poly[j];
        const Point& d = poly[i];
        const float d1 = crossProduct(c, d, a);
        const float d2 = crossProduct(c, d, b);
        const float d3 = crossProduct(a, b, c);
        const float d4 = crossProduct(a, b, d);
        if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
            ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
            return true;
        }
    }
    return false;
}

// ── RouteValidator::validate ──────────────────────────────────────────────────

RouteValidationResult RouteValidator::validate(const Map& map, const RoutePlan& route) {
    RouteValidationResult result;

    if (route.points.empty()) {
        result.errors.push_back({
            RouteErrorCode::ROUTE_EMPTY,
            -1,
            "Route is empty — no waypoints to follow",
            {}
        });
        result.valid = false;
        return result;
    }

    // Check route-level valid flag set by appendTransition() when A* fails
    if (!route.valid) {
        result.errors.push_back({
            RouteErrorCode::ROUTE_INVALID_TRANSITION,
            -1,
            route.invalidReason.empty()
                ? "Route contains an unresolvable transition (A* found no path)"
                : route.invalidReason,
            {}
        });
    }

    const auto& exclusions    = map.exclusions();
    const auto& exclusionMeta = map.exclusionMeta();
    const auto& zones         = map.zones();

    // Build a lookup: zoneId → zone polygon
    auto findZonePoly = [&](const std::string& id) -> const PolygonPoints* {
        for (const auto& z : zones) {
            if (z.id == id) return &z.polygon;
        }
        return nullptr;
    };

    for (int i = 0; i < static_cast<int>(route.points.size()); ++i) {
        const RoutePoint& rp = route.points[i];

        // 1. Coverage and transit points must be inside the allowed area
        if (rp.sourceMode == WayType::MOW) {
            if (!map.isInsideAllowedArea(rp.p.x, rp.p.y)) {
                std::ostringstream msg;
                msg << "Point " << i << " (" << rp.p.x << "," << rp.p.y
                    << ") is outside the allowed area";
                result.errors.push_back({
                    RouteErrorCode::POINT_OUTSIDE_PERIMETER, i, msg.str(), rp.zoneId
                });
            }
        }

        // 2. MOW points must not be inside a hard exclusion
        if (rp.sourceMode == WayType::MOW) {
            for (size_t ei = 0; ei < exclusions.size(); ++ei) {
                const bool isHard = (ei < exclusionMeta.size())
                    ? (exclusionMeta[ei].type == ExclusionType::HARD)
                    : true;
                if (isHard && pointInPolygon(exclusions[ei], rp.p.x, rp.p.y)) {
                    std::ostringstream msg;
                    msg << "Point " << i << " (" << rp.p.x << "," << rp.p.y
                        << ") is inside a hard exclusion zone";
                    result.errors.push_back({
                        RouteErrorCode::POINT_IN_EXCLUSION, i, msg.str(), rp.zoneId
                    });
                    break;
                }
            }
        }

        // 3. TRANSIT_WITHIN_ZONE must not leave its zone polygon
        if (rp.semantic == RouteSemantic::TRANSIT_WITHIN_ZONE && !rp.zoneId.empty()) {
            const PolygonPoints* zonePoly = findZonePoly(rp.zoneId);
            if (zonePoly && zonePoly->size() >= 3 &&
                !pointInPolygon(*zonePoly, rp.p.x, rp.p.y)) {
                std::ostringstream msg;
                msg << "TRANSIT_WITHIN_ZONE point " << i << " (" << rp.p.x << ","
                    << rp.p.y << ") is outside zone '" << rp.zoneId << "'";
                result.errors.push_back({
                    RouteErrorCode::TRANSIT_OUTSIDE_ZONE, i, msg.str(), rp.zoneId
                });
            }
        }

        // 4. Segments between consecutive MOW points must not cross hard exclusions
        if (i > 0 && rp.sourceMode == WayType::MOW) {
            const Point& prev = route.points[i - 1].p;
            for (size_t ei = 0; ei < exclusions.size(); ++ei) {
                const bool isHard = (ei < exclusionMeta.size())
                    ? (exclusionMeta[ei].type == ExclusionType::HARD)
                    : true;
                if (isHard && segmentCrossesPolygon(prev, rp.p, exclusions[ei])) {
                    std::ostringstream msg;
                    msg << "Segment " << (i - 1) << "->" << i
                        << " crosses a hard exclusion zone";
                    result.errors.push_back({
                        RouteErrorCode::SEGMENT_CROSSES_EXCLUSION, i, msg.str(), rp.zoneId
                    });
                    break;
                }
            }
        }

        // 5. Coverage/transit points should have a known semantic (warn on UNKNOWN)
        if (rp.sourceMode == WayType::MOW &&
            rp.semantic == RouteSemantic::UNKNOWN) {
            std::ostringstream msg;
            msg << "Point " << i << " has UNKNOWN semantic (unset by planner)";
            result.errors.push_back({
                RouteErrorCode::UNKNOWN_SEMANTIC, i, msg.str(), rp.zoneId
            });
        }
    }

    result.valid = result.errors.empty();
    return result;
}

} // namespace nav
} // namespace sunray
