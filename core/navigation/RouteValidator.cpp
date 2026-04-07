#include "core/navigation/RouteValidator.h"

#include <cmath>
#include <sstream>

namespace sunray
{
    namespace nav
    {

        // ── Geometry helpers (local) ──────────────────────────────────────────────────

        bool RouteValidator::pointInPolygon(const PolygonPoints &poly, float px, float py)
        {
            bool inside = false;
            const size_t n = poly.size();
            for (size_t i = 0, j = n - 1; i < n; j = i++)
            {
                const float xi = poly[i].x, yi = poly[i].y;
                const float xj = poly[j].x, yj = poly[j].y;
                if (((yi > py) != (yj > py)) &&
                    (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
                {
                    inside = !inside;
                }
            }
            return inside;
        }

        bool RouteValidator::segmentCrossesPolygon(const Point &a, const Point &b,
                                                   const PolygonPoints &poly)
        {
            if (poly.size() < 2)
                return false;
            const auto crossProduct = [](const Point &o, const Point &u, const Point &v)
            {
                return (u.x - o.x) * (v.y - o.y) - (u.y - o.y) * (v.x - o.x);
            };
            for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
            {
                const Point &c = poly[j];
                const Point &d = poly[i];
                const float d1 = crossProduct(c, d, a);
                const float d2 = crossProduct(c, d, b);
                const float d3 = crossProduct(a, b, c);
                const float d4 = crossProduct(a, b, d);
                if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
                    ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
                {
                    return true;
                }
            }
            return false;
        }

        bool RouteValidator::segmentStaysInsideZoneAllowedArea(const Map &map,
                                                               const PolygonPoints &zonePoly,
                                                               const Point &a,
                                                               const Point &b)
        {
            const float distance = a.distanceTo(b);
            const int steps = std::max(1, static_cast<int>(std::ceil(distance / 0.05f)));
            for (int step = 0; step <= steps; ++step)
            {
                const float t = static_cast<float>(step) / static_cast<float>(steps);
                const float px = a.x + (b.x - a.x) * t;
                const float py = a.y + (b.y - a.y) * t;
                if (!pointInPolygon(zonePoly, px, py) || !map.isInsideAllowedArea(px, py))
                    return false;
            }
            return true;
        }

        // ── RouteValidator::validate ──────────────────────────────────────────────────

        RouteValidationResult RouteValidator::validate(const Map &map, const RoutePlan &route)
        {
            RouteValidationResult result;

            const auto isCoverageSemantic = [](RouteSemantic sem)
            {
                return sem == RouteSemantic::COVERAGE_EDGE || sem == RouteSemantic::COVERAGE_INFILL;
            };

            const auto requiresZoneBoundSurface = [](const RoutePoint &point)
            {
                if (point.sourceMode != WayType::MOW)
                    return false;
                if (point.zoneId.empty())
                    return false;
                return point.semantic != RouteSemantic::TRANSIT_INTER_ZONE;
            };

            if (route.points.empty())
            {
                result.errors.push_back({RouteErrorCode::ROUTE_EMPTY,
                                         -1,
                                         "Route is empty — no waypoints to follow",
                                         {}});
                result.valid = false;
                return result;
            }

            // Check route-level valid flag set by appendTransition() when A* fails
            if (!route.valid)
            {
                result.errors.push_back({RouteErrorCode::ROUTE_INVALID_TRANSITION,
                                         -1,
                                         route.invalidReason.empty()
                                             ? "Route contains an unresolvable transition (A* found no path)"
                                             : route.invalidReason,
                                         {}});
            }

            const auto &exclusions = map.exclusions();
            const auto &exclusionMeta = map.exclusionMeta();
            const auto &zones = map.zones();

            // Build a lookup: zoneId → zone polygon
            auto findZonePoly = [&](const std::string &id) -> const PolygonPoints *
            {
                for (const auto &z : zones)
                {
                    if (z.id == id)
                        return &z.polygon;
                }
                return nullptr;
            };

            for (int i = 0; i < static_cast<int>(route.points.size()); ++i)
            {
                const RoutePoint &rp = route.points[i];

                // 1. Coverage and transit points must be inside the allowed area
                if (rp.sourceMode == WayType::MOW)
                {
                    if (!map.isInsideAllowedArea(rp.p.x, rp.p.y))
                    {
                        std::ostringstream msg;
                        msg << "Point " << i << " (" << rp.p.x << "," << rp.p.y
                            << ") is outside the allowed area";
                        result.errors.push_back({RouteErrorCode::POINT_OUTSIDE_PERIMETER, i, msg.str(), rp.zoneId});
                    }
                }

                // 2. MOW points must not be inside a hard exclusion
                if (rp.sourceMode == WayType::MOW)
                {
                    for (size_t ei = 0; ei < exclusions.size(); ++ei)
                    {
                        const bool isHard = (ei < exclusionMeta.size())
                                                ? (exclusionMeta[ei].type == ExclusionType::HARD)
                                                : true;
                        if (isHard && pointInPolygon(exclusions[ei], rp.p.x, rp.p.y))
                        {
                            std::ostringstream msg;
                            msg << "Point " << i << " (" << rp.p.x << "," << rp.p.y
                                << ") is inside a hard exclusion zone";
                            result.errors.push_back({RouteErrorCode::POINT_IN_EXCLUSION, i, msg.str(), rp.zoneId});
                            break;
                        }
                    }
                }

                // 3. All zone-bound MOW semantics must remain on the planner surface of that zone.
                if (requiresZoneBoundSurface(rp))
                {
                    const PolygonPoints *zonePoly = findZonePoly(rp.zoneId);
                    if (zonePoly && zonePoly->size() >= 3 &&
                        (!pointInPolygon(*zonePoly, rp.p.x, rp.p.y) || !map.isInsideAllowedArea(rp.p.x, rp.p.y)))
                    {
                        std::ostringstream msg;
                        msg << "Point " << i << " (" << rp.p.x << ","
                            << rp.p.y << ") leaves the planner-allowed surface of zone '" << rp.zoneId << "'";
                        result.errors.push_back({RouteErrorCode::TRANSIT_OUTSIDE_ZONE, i, msg.str(), rp.zoneId});
                    }
                }

                // 4. Segments between consecutive MOW points must not leave the same-zone planner surface.
                if (i > 0 && rp.sourceMode == WayType::MOW)
                {
                    const RoutePoint &prevPoint = route.points[i - 1];
                    if (requiresZoneBoundSurface(prevPoint) &&
                        requiresZoneBoundSurface(rp) &&
                        prevPoint.zoneId == rp.zoneId)
                    {
                        const PolygonPoints *zonePoly = findZonePoly(rp.zoneId);
                        if (zonePoly && zonePoly->size() >= 3 &&
                            !segmentStaysInsideZoneAllowedArea(map, *zonePoly, prevPoint.p, rp.p))
                        {
                            std::ostringstream msg;
                            msg << "Segment " << (i - 1) << "->" << i
                                << " leaves the planner-allowed surface of zone '" << rp.zoneId << "'";
                            result.errors.push_back({RouteErrorCode::SEGMENT_OUTSIDE_ALLOWED_ZONE, i, msg.str(), rp.zoneId});
                        }
                    }

                    // 5. Segments between consecutive MOW points must not cross hard exclusions
                    const Point &prev = route.points[i - 1].p;
                    for (size_t ei = 0; ei < exclusions.size(); ++ei)
                    {
                        const bool isHard = (ei < exclusionMeta.size())
                                                ? (exclusionMeta[ei].type == ExclusionType::HARD)
                                                : true;
                        if (isHard && segmentCrossesPolygon(prev, rp.p, exclusions[ei]))
                        {
                            std::ostringstream msg;
                            msg << "Segment " << (i - 1) << "->" << i
                                << " crosses a hard exclusion zone";
                            result.errors.push_back({RouteErrorCode::SEGMENT_CROSSES_EXCLUSION, i, msg.str(), rp.zoneId});
                            break;
                        }
                    }
                }

                // 6. Coverage/transit points should have a known semantic (warn on UNKNOWN)
                if (rp.sourceMode == WayType::MOW &&
                    rp.semantic == RouteSemantic::UNKNOWN)
                {
                    std::ostringstream msg;
                    msg << "Point " << i << " has UNKNOWN semantic (unset by planner)";
                    result.errors.push_back({RouteErrorCode::UNKNOWN_SEMANTIC, i, msg.str(), rp.zoneId});
                }

                // 7. Coverage points must have a stable component id.
                if (rp.sourceMode == WayType::MOW && isCoverageSemantic(rp.semantic) && rp.componentId.empty())
                {
                    std::ostringstream msg;
                    msg << "Coverage point " << i << " is missing componentId";
                    result.errors.push_back({RouteErrorCode::MISSING_COMPONENT_ID, i, msg.str(), rp.zoneId});
                }

                // 8. Component switches may not happen as a silent coverage-to-coverage jump.
                if (i > 0 && rp.sourceMode == WayType::MOW)
                {
                    const RoutePoint &prev = route.points[i - 1];
                    if (prev.sourceMode == WayType::MOW &&
                        isCoverageSemantic(prev.semantic) &&
                        isCoverageSemantic(rp.semantic) &&
                        !prev.zoneId.empty() && prev.zoneId == rp.zoneId &&
                        !prev.componentId.empty() && !rp.componentId.empty() &&
                        prev.componentId != rp.componentId)
                    {
                        std::ostringstream msg;
                        msg << "Coverage jumped from component '" << prev.componentId
                            << "' to '" << rp.componentId
                            << "' in zone '" << rp.zoneId
                            << "' without explicit inter-component transit";
                        result.errors.push_back({RouteErrorCode::INVALID_COMPONENT_TRANSITION, i, msg.str(), rp.zoneId});
                    }
                }
            }

            result.valid = result.errors.empty();
            return result;
        }

    } // namespace nav
} // namespace sunray
