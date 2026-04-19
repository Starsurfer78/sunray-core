#include "core/navigation/MissionCompiler.h"
#include "core/navigation/StripingAlgorithm.h"

#include <clipper2/clipper.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace sunray
{
    namespace nav
    {

        namespace
        {

            struct PreparedCoverageArea
            {
                PolygonPoints zone;      // raw zone polygon (normalised)
                PolygonPoints perimeter; // raw perimeter polygon
                std::vector<PolygonPoints> exclusions;

                // Explicit working area: zone ∩ perimeter − hardExclusions.
                // All coverage steps (headland, infill, transitions) MUST use this,
                // not the raw zone, so they all operate on the same surface.
                std::vector<PolygonPoints> workingArea;
            };

            static bool samePoint(const Point &a, const Point &b, float eps = 1e-4f)
            {
                return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
            }

            static Point rotatePoint(const Point &p, float cosA, float sinA)
            {
                return {
                    p.x * cosA - p.y * sinA,
                    p.x * sinA + p.y * cosA,
                };
            }

            static PolygonPoints rotatePolygon(const PolygonPoints &poly, float angleRad)
            {
                const float cosA = std::cos(angleRad);
                const float sinA = std::sin(angleRad);
                PolygonPoints out;
                out.reserve(poly.size());
                for (const auto &p : poly)
                    out.push_back(rotatePoint(p, cosA, sinA));
                return out;
            }

            static void appendUniquePoint(PolygonPoints &pts, const Point &p, float eps = 1e-4f)
            {
                if (!pts.empty() && samePoint(pts.back(), p, eps))
                    return;
                pts.push_back(p);
            }

            static PolygonPoints normalizePolygon(const PolygonPoints &poly)
            {
                PolygonPoints out;
                out.reserve(poly.size());
                for (const auto &point : poly)
                {
                    appendUniquePoint(out, point);
                }
                if (out.size() > 1 && samePoint(out.front(), out.back()))
                {
                    out.pop_back();
                }
                return out.size() >= 3 ? out : PolygonPoints{};
            }

            static PreparedCoverageArea prepareCoverageArea(const Zone &zone, const Map &map)
            {
                PreparedCoverageArea prepared;
                prepared.zone = normalizePolygon(zone.polygon);
                prepared.perimeter = normalizePolygon(map.perimeterPoints());
                const auto &exclusions = map.exclusions();
                const auto &exclusionMeta = map.exclusionMeta();
                prepared.exclusions.reserve(exclusions.size());
                for (std::size_t exclusionIndex = 0; exclusionIndex < exclusions.size(); ++exclusionIndex)
                {
                    const bool isHard = exclusionIndex >= exclusionMeta.size() ||
                                        exclusionMeta[exclusionIndex].type == ExclusionType::HARD;
                    if (!isHard)
                        continue;

                    PolygonPoints normalized = normalizePolygon(exclusions[exclusionIndex]);
                    if (normalized.size() >= 3)
                        prepared.exclusions.push_back(std::move(normalized));
                }
                // workingArea and primaryWorkingPoly are filled by computeWorkingArea()
                // after Clipper2 helpers are available (called from buildMissionMowRoutePreview).
                return prepared;
            }

            static Clipper2Lib::PathD toClipperPath(const PolygonPoints &poly)
            {
                Clipper2Lib::PathD path;
                path.reserve(poly.size());
                for (const auto &p : poly)
                    path.emplace_back(p.x, p.y);
                return path;
            }

            static PolygonPoints fromClipperPath(const Clipper2Lib::PathD &path)
            {
                PolygonPoints out;
                out.reserve(path.size());
                for (const auto &pt : path)
                    out.push_back({static_cast<float>(pt.x), static_cast<float>(pt.y)});
                return out;
            }

            static Point polygonCentroid(const PolygonPoints &poly);

            static std::vector<PolygonPoints> sortComponentsStable(std::vector<PolygonPoints> components);

            static bool polygonsEquivalent(const PolygonPoints &lhs, const PolygonPoints &rhs, float eps = 1e-4f);

            static Point rotatedCentroid(const PolygonPoints &poly, float angleDeg)
            {
                return polygonCentroid(rotatePolygon(poly, -angleDeg * static_cast<float>(M_PI) / 180.0f));
            }

            static float rotatedMinY(const PolygonPoints &poly, float angleDeg)
            {
                const PolygonPoints rotated = rotatePolygon(poly, -angleDeg * static_cast<float>(M_PI) / 180.0f);
                if (rotated.empty())
                    return 0.0f;
                float minY = rotated.front().y;
                for (const auto &point : rotated)
                    minY = std::min(minY, point.y);
                return minY;
            }

            static std::vector<PolygonPoints> sortComponentsForCoverageOrder(std::vector<PolygonPoints> components,
                                                                             float angleDeg)
            {
                std::sort(components.begin(), components.end(), [angleDeg](const PolygonPoints &lhs, const PolygonPoints &rhs)
                          {
        const float lhsRow = rotatedMinY(lhs, angleDeg);
        const float rhsRow = rotatedMinY(rhs, angleDeg);
        if (std::fabs(lhsRow - rhsRow) > 1e-4f) return lhsRow < rhsRow;

        const Point lhsCenter = rotatedCentroid(lhs, angleDeg);
        const Point rhsCenter = rotatedCentroid(rhs, angleDeg);
        if (std::fabs(lhsCenter.x - rhsCenter.x) > 1e-4f) return lhsCenter.x < rhsCenter.x;
        if (std::fabs(lhsCenter.y - rhsCenter.y) > 1e-4f) return lhsCenter.y < rhsCenter.y;

        const std::vector<PolygonPoints> stablePair = sortComponentsStable({lhs, rhs});
        return polygonsEquivalent(lhs, stablePair.front()); });
                return components;
            }

            static std::vector<PolygonPoints> outerPolygonsFromClipper(const Clipper2Lib::PathsD &paths)
            {
                std::vector<PolygonPoints> result;
                result.reserve(paths.size());
                for (const auto &path : paths)
                {
                    if (Clipper2Lib::Area(path) <= 0.0)
                        continue;
                    PolygonPoints poly = normalizePolygon(fromClipperPath(path));
                    if (poly.size() >= 3)
                        result.push_back(std::move(poly));
                }
                return sortComponentsStable(std::move(result));
            }

            /// Inset (positive inset = shrink) via Clipper2 with miter join.
            /// Returns all outer result polygons, or empty if the polygon vanishes.
            static std::vector<PolygonPoints> offsetPolygon(const PolygonPoints &poly, float inset)
            {
                if (poly.size() < 3)
                    return {};
                if (std::fabs(inset) < 1e-6f)
                    return {poly};

                Clipper2Lib::PathsD paths = {toClipperPath(poly)};
                // Clipper2 InflatePaths: negative delta = inset
                const Clipper2Lib::PathsD result = Clipper2Lib::InflatePaths(
                    paths,
                    -static_cast<double>(inset),
                    Clipper2Lib::JoinType::Miter,
                    Clipper2Lib::EndType::Polygon);

                return outerPolygonsFromClipper(result);
            }

            /// Inset with round join at convex corners (used for headland contours).
            /// Returns all outer result polygons, or the original if inset is near zero.
            static std::vector<PolygonPoints> offsetPolygonRounded(const PolygonPoints &poly, float inset)
            {
                if (poly.size() < 3)
                    return {};
                if (std::fabs(inset) < 1e-6f)
                    return {poly};

                Clipper2Lib::PathsD paths = {toClipperPath(poly)};
                const Clipper2Lib::PathsD result = Clipper2Lib::InflatePaths(
                    paths,
                    -static_cast<double>(inset),
                    Clipper2Lib::JoinType::Round,
                    Clipper2Lib::EndType::Polygon);

                return outerPolygonsFromClipper(result);
            }

            /// Compute workingArea = zone ∩ perimeter − hardExclusions using Clipper2.
            /// Returns all result polygons (may be multiple if exclusions split the zone).
            /// Must be called after toClipperPath / fromClipperPath are defined.
            static std::vector<PolygonPoints> computeWorkingArea(
                const PolygonPoints &zone,
                const PolygonPoints &perimeter,
                const std::vector<PolygonPoints> &exclusions)
            {
                if (zone.size() < 3)
                    return {};

                Clipper2Lib::PathsD work;
                work.push_back(toClipperPath(zone));

                // Step 1: zone ∩ perimeter
                if (perimeter.size() >= 3)
                {
                    Clipper2Lib::PathsD perimPaths;
                    perimPaths.push_back(toClipperPath(perimeter));
                    work = Clipper2Lib::Intersect(work, perimPaths, Clipper2Lib::FillRule::NonZero);
                    if (work.empty())
                        return {};
                }

                // Step 2: subtract each hard exclusion
                for (const auto &excl : exclusions)
                {
                    if (excl.size() < 3)
                        continue;
                    Clipper2Lib::PathsD exclPaths;
                    exclPaths.push_back(toClipperPath(excl));
                    work = Clipper2Lib::Difference(work, exclPaths, Clipper2Lib::FillRule::NonZero);
                    if (work.empty())
                        return {};
                }

                std::vector<PolygonPoints> result;
                result.reserve(work.size());
                for (const auto &path : work)
                {
                    // Clipper2 uses sign of area to distinguish outer rings (positive / CCW)
                    // from hole rings (negative / CW). Only outer rings become working polygons.
                    if (Clipper2Lib::Area(path) < 0.0)
                        continue;
                    PolygonPoints poly = fromClipperPath(path);
                    if (poly.size() >= 3)
                        result.push_back(std::move(poly));
                }
                return result;
            }

            static Point polygonCentroid(const PolygonPoints &poly)
            {
                Point centroid{0.0f, 0.0f};
                if (poly.empty())
                    return centroid;
                for (const auto &point : poly)
                {
                    centroid.x += point.x;
                    centroid.y += point.y;
                }
                const float invCount = 1.0f / static_cast<float>(poly.size());
                centroid.x *= invCount;
                centroid.y *= invCount;
                return centroid;
            }

            static std::vector<PolygonPoints> sortComponentsStable(std::vector<PolygonPoints> components)
            {
                std::sort(components.begin(), components.end(), [](const PolygonPoints &lhs, const PolygonPoints &rhs)
                          {
        const Point lhsCenter = polygonCentroid(lhs);
        const Point rhsCenter = polygonCentroid(rhs);
        if (std::fabs(lhsCenter.x - rhsCenter.x) > 1e-4f) return lhsCenter.x < rhsCenter.x;
        if (std::fabs(lhsCenter.y - rhsCenter.y) > 1e-4f) return lhsCenter.y < rhsCenter.y;
        if (lhs.size() != rhs.size()) return lhs.size() < rhs.size();
        for (std::size_t i = 0; i < lhs.size(); ++i)
        {
            if (std::fabs(lhs[i].x - rhs[i].x) > 1e-4f) return lhs[i].x < rhs[i].x;
            if (std::fabs(lhs[i].y - rhs[i].y) > 1e-4f) return lhs[i].y < rhs[i].y;
        }
        return false; });
                return components;
            }

            static bool polygonsEquivalent(const PolygonPoints &lhs, const PolygonPoints &rhs, float eps)
            {
                if (lhs.size() != rhs.size())
                    return false;
                for (std::size_t i = 0; i < lhs.size(); ++i)
                {
                    if (!samePoint(lhs[i], rhs[i], eps))
                        return false;
                }
                return true;
            }

            static RoutePlan reversedRoute(RoutePlan route)
            {
                std::reverse(route.points.begin(), route.points.end());
                route.active = route.points.size() >= 2;
                return route;
            }

            static float routeLength(const RoutePlan &route)
            {
                float total = 0.0f;
                for (std::size_t i = 1; i < route.points.size(); ++i)
                    total += route.points[i - 1].p.distanceTo(route.points[i].p);
                return total;
            }

            static RoutePlan planTransitionRoute(const Map &map,
                                                 const Point &from,
                                                 const Point &to,
                                                 const ZoneSettings &settings,
                                                 const PolygonPoints &zonePoly,
                                                 const std::string &zoneId,
                                                 const std::string &componentId,
                                                 RouteSemantic semantic)
            {
                RoutePlan transition = map.previewPath(
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

                for (auto &point : transition.points)
                {
                    point.semantic = semantic;
                    point.zoneId = zoneId;
                    point.componentId = componentId;
                }
                return transition;
            }

            struct ComponentJoinChoice
            {
                bool ok = false;
                RoutePlan current;
                RoutePlan next;
                RoutePlan transition;
            };

            static ComponentJoinChoice selectComponentJoin(
                const Map &map,
                const RoutePlan &currentRoute,
                const RoutePlan &nextRoute,
                const ZoneSettings &settings,
                const PolygonPoints &zonePoly,
                const std::string &zoneId,
                bool allowReverseCurrent,
                RouteSemantic transitionSemantic = RouteSemantic::TRANSIT_BETWEEN_COMPONENTS)
            {
                ComponentJoinChoice best;
                if (currentRoute.points.empty() || nextRoute.points.empty())
                    return best;

                const std::vector<RoutePlan> currentCandidates = allowReverseCurrent
                                                                     ? std::vector<RoutePlan>{currentRoute, reversedRoute(currentRoute)}
                                                                     : std::vector<RoutePlan>{currentRoute};
                const std::vector<RoutePlan> nextCandidates = {nextRoute, reversedRoute(nextRoute)};

                for (const auto &currentCandidate : currentCandidates)
                {
                    for (const auto &nextCandidate : nextCandidates)
                    {
                        const RoutePlan transition = planTransitionRoute(
                            map,
                            currentCandidate.points.back().p,
                            nextCandidate.points.front().p,
                            settings,
                            zonePoly,
                            zoneId,
                            {},
                            transitionSemantic);
                        if (transition.points.empty())
                            continue;

                        const float cost = routeLength(transition);
                        if (!best.ok || cost < routeLength(best.transition))
                        {
                            best.ok = true;
                            best.current = currentCandidate;
                            best.next = nextCandidate;
                            best.transition = transition;
                        }
                    }
                }

                return best;
            }

            // ── Coverage helpers ──────────────────────────────────────────────────────────
            // Stripe generation is in StripingAlgorithm.cpp (shared with ZonePlanner).
            // These functions produce route transitions and headland contours.

            static RoutePlan routeFromPolyline(const PolygonPoints &points,
                                               const ZoneSettings &settings,
                                               RouteSemantic semantic = RouteSemantic::COVERAGE_EDGE,
                                               const std::string &zoneId = {},
                                               const std::string &componentId = {})
            {
                RoutePlan route;
                route.sourceMode = WayType::MOW;
                route.active = points.size() >= 2;
                if (points.size() < 2)
                    return route;

                for (const auto &p : points)
                {
                    RoutePoint rp;
                    rp.p = p;
                    rp.reverse = false;
                    rp.slow = false;
                    rp.reverseAllowed = settings.reverseAllowed;
                    rp.clearance_m = settings.clearance;
                    rp.sourceMode = WayType::MOW;
                    rp.semantic = semantic;
                    rp.zoneId = zoneId;
                    rp.componentId = componentId;
                    route.points.push_back(rp);
                }
                return route;
            }

            static RoutePlan appendRoute(RoutePlan base, const RoutePlan &add)
            {
                if (add.points.empty())
                    return base;
                if (base.points.empty())
                    return add;

                if (!samePoint(base.points.back().p, add.points.front().p))
                {
                    base.points.push_back(add.points.front());
                }
                for (size_t i = 0; i < add.points.size(); ++i)
                {
                    if (!base.points.empty() && samePoint(base.points.back().p, add.points[i].p))
                        continue;
                    base.points.push_back(add.points[i]);
                }
                base.active = !base.points.empty();
                // Propagate invalidity from either side
                if (!add.valid)
                {
                    base.valid = false;
                    if (base.invalidReason.empty())
                        base.invalidReason = add.invalidReason;
                }
                return base;
            }

            // ── Transition helpers ────────────────────────────────────────────────────────
            // These functions insert A*-planned transitions between Coverage blocks.
            // Transitions are zone-aware: TRANSIT_WITHIN_ZONE paths are constrained to the
            // active zone polygon; TRANSIT_INTER_ZONE paths have no zone constraint.

            static bool appendTransition(RoutePlan &route,
                                         const Map &map,
                                         const Point &from,
                                         const Point &to,
                                         const ZoneSettings &settings,
                                         const PolygonPoints &zonePoly,
                                         const std::string &zoneId = {},
                                         const std::string &componentId = {},
                                         RouteSemantic semanticOverride = RouteSemantic::UNKNOWN);

            /// Point-in-polygon test (ray-casting).
            static bool pointInPolygonLocal(const PolygonPoints &poly, float px, float py)
            {
                bool inside = false;
                const size_t n = poly.size();
                for (size_t i = 0, j = n - 1; i < n; j = i++)
                {
                    const float xi = poly[i].x, yi = poly[i].y;
                    const float xj = poly[j].x, yj = poly[j].y;
                    if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
                    {
                        inside = !inside;
                    }
                }
                return inside;
            }

            static bool segmentsIntersectLocal(const Point &a0, const Point &a1,
                                               const Point &b0, const Point &b1)
            {
                const float d1x = a1.x - a0.x;
                const float d1y = a1.y - a0.y;
                const float d2x = b1.x - b0.x;
                const float d2y = b1.y - b0.y;
                const float cross = d1x * d2y - d1y * d2x;
                if (std::fabs(cross) < 1e-10f)
                    return false;

                const float t = ((b0.x - a0.x) * d2y - (b0.y - a0.y) * d2x) / cross;
                const float u = ((b0.x - a0.x) * d1y - (b0.y - a0.y) * d1x) / cross;
                return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
            }

            static bool segmentCrossesPolygonLocal(const Point &from, const Point &to,
                                                   const PolygonPoints &poly)
            {
                if (poly.size() < 3)
                    return false;
                for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
                {
                    if (segmentsIntersectLocal(from, to, poly[j], poly[i]))
                        return true;
                }
                return false;
            }

            static bool lineCircleIntersectsLocal(const Point &from, const Point &to,
                                                  const Point &center, float radius)
            {
                const float dx = to.x - from.x;
                const float dy = to.y - from.y;
                const float lengthSq = dx * dx + dy * dy;
                if (lengthSq < 1e-8f)
                    return from.distanceTo(center) <= radius;

                const float t = std::clamp(((center.x - from.x) * dx + (center.y - from.y) * dy) / lengthSq,
                                           0.0f,
                                           1.0f);
                const Point closest{from.x + t * dx, from.y + t * dy};
                return closest.distanceTo(center) <= radius;
            }

            static bool segmentInsidePolygonSurface(const PolygonPoints &poly,
                                                    const Point &from,
                                                    const Point &to)
            {
                if (poly.size() < 3)
                    return true;
                if (!pointInPolygonLocal(poly, from.x, from.y) ||
                    !pointInPolygonLocal(poly, to.x, to.y))
                    return false;

                // Stripe endpoints often lie exactly on the zone boundary, so a naive
                // "any crossing → false" produces false positives (t ≈ 0 or t ≈ 1 on the
                // segment). The old single-midpoint check was insufficient for concave
                // polygons where the segment exits through a concave notch but the midpoint
                // is still inside the main body.
                //
                // Correct approach: only strictly-interior crossings (t well away from the
                // endpoints) indicate that the segment genuinely leaves the polygon.
                static constexpr float kEndpointEps = 1e-4f;
                const float sdx = to.x - from.x;
                const float sdy = to.y - from.y;
                const std::size_t n = poly.size();
                for (std::size_t i = 0, j = n - 1; i < n; j = i++)
                {
                    const float edx = poly[i].x - poly[j].x;
                    const float edy = poly[i].y - poly[j].y;
                    const float cross = sdx * edy - sdy * edx;
                    if (std::fabs(cross) < 1e-10f)
                        continue;
                    // t: position along from→to (0 = from, 1 = to)
                    // u: position along poly[j]→poly[i] (0..1 = on the edge)
                    const float t = ((poly[j].x - from.x) * edy - (poly[j].y - from.y) * edx) / cross;
                    const float u = ((poly[j].x - from.x) * sdy - (poly[j].y - from.y) * sdx) / cross;
                    if (t > kEndpointEps && t < 1.0f - kEndpointEps && u >= 0.0f && u <= 1.0f)
                        return false; // segment exits the polygon through a concave notch
                }
                return true;
            }

            static bool directTransitionAllowed(const Map &map,
                                                const Point &from,
                                                const Point &to,
                                                const PolygonPoints &zonePoly)
            {
                if (!map.isInsideAllowedArea(from.x, from.y) ||
                    !map.isInsideAllowedArea(to.x, to.y))
                    return false;

                if (!zonePoly.empty() && !segmentInsidePolygonSurface(zonePoly, from, to))
                    return false;

                const auto &perimeter = map.perimeterPoints();
                if (perimeter.size() >= 3 && segmentCrossesPolygonLocal(from, to, perimeter))
                {
                    const Point mid{(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f};
                    if (!map.isInsideAllowedArea(mid.x, mid.y))
                        return false;
                }

                const auto &exclusions = map.exclusions();
                const auto &exclusionMeta = map.exclusionMeta();
                for (std::size_t exclusionIndex = 0; exclusionIndex < exclusions.size(); ++exclusionIndex)
                {
                    const bool isHard = exclusionIndex >= exclusionMeta.size() ||
                                        exclusionMeta[exclusionIndex].type == ExclusionType::HARD;
                    if (isHard && segmentCrossesPolygonLocal(from, to, exclusions[exclusionIndex]))
                        return false;
                }

                for (const auto &obstacle : map.obstacles())
                {
                    const float radius = obstacle.radius_m + map.plannerSettings().obstacleInflation_m;
                    if (lineCircleIntersectsLocal(from, to, obstacle.center, radius))
                        return false;
                }

                return true;
            }

            static Segment reversedSegment(const Segment &segment)
            {
                return Segment{segment.b, segment.a};
            }

            static std::vector<Segment> orientSegmentsGreedy(std::vector<Segment> segments,
                                                             const Point *entryPoint)
            {
                if (segments.empty())
                    return segments;

                Point last = entryPoint ? *entryPoint : segments.front().b;
                for (auto &segment : segments)
                {
                    if (last.distanceTo(segment.b) < last.distanceTo(segment.a))
                        segment = reversedSegment(segment);
                    last = segment.b;
                }
                return segments;
            }

            static float transitionCostForStripeOrdering(const Map &map,
                                                         const Point &from,
                                                         const Point &to,
                                                         const ZoneSettings &settings,
                                                         const PolygonPoints &zonePoly)
            {
                if (directTransitionAllowed(map, from, to, zonePoly))
                    return from.distanceTo(to);

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

                if (transition.points.empty())
                    return std::numeric_limits<float>::infinity();

                float length = 0.0f;
                Point prev = from;
                for (const auto &point : transition.points)
                {
                    length += prev.distanceTo(point.p);
                    prev = point.p;
                }
                return length;
            }

            static std::vector<Segment> orientStripeLane(std::vector<Segment> lane,
                                                         bool reverseRows,
                                                         const Point *entryPoint)
            {
                if (reverseRows)
                    std::reverse(lane.begin(), lane.end());
                return orientSegmentsGreedy(std::move(lane), entryPoint);
            }

            static std::vector<Segment> orderStripeSegmentsByLane(const Map &map,
                                                                  const std::vector<Segment> &segments,
                                                                  const ZoneSettings &settings,
                                                                  const PolygonPoints &zonePoly,
                                                                  float angleDeg,
                                                                  const Point *entryPoint = nullptr)
            {
                if (segments.size() < 2)
                    return segments;

                struct StripeRun
                {
                    Segment segment;
                    float rowY = 0.0f;
                    float minX = 0.0f;
                    std::size_t lane = 0;
                };

                const float angle = -angleDeg * static_cast<float>(M_PI) / 180.0f;
                const float cosA = std::cos(angle);
                const float sinA = std::sin(angle);
                std::vector<StripeRun> runs;
                runs.reserve(segments.size());
                for (const auto &segment : segments)
                {
                    const Point a = rotatePoint(segment.a, cosA, sinA);
                    const Point b = rotatePoint(segment.b, cosA, sinA);
                    runs.push_back({segment, (a.y + b.y) * 0.5f, std::min(a.x, b.x), 0});
                }

                std::sort(runs.begin(), runs.end(), [](const StripeRun &lhs, const StripeRun &rhs)
                          {
                    if (std::fabs(lhs.rowY - rhs.rowY) > 1e-3f) return lhs.rowY < rhs.rowY;
                    return lhs.minX < rhs.minX; });

                std::vector<std::vector<StripeRun>> rows;
                for (const auto &run : runs)
                {
                    if (rows.empty() || std::fabs(rows.back().front().rowY - run.rowY) > 1e-3f)
                        rows.push_back({run});
                    else
                        rows.back().push_back(run);
                }

                std::size_t laneCount = 0;
                for (auto &row : rows)
                {
                    std::sort(row.begin(), row.end(), [](const StripeRun &lhs, const StripeRun &rhs)
                              { return lhs.minX < rhs.minX; });
                    for (std::size_t lane = 0; lane < row.size(); ++lane)
                    {
                        row[lane].lane = lane;
                        laneCount = std::max(laneCount, lane + 1);
                    }
                }

                std::vector<std::vector<Segment>> lanes(laneCount);
                for (const auto &row : rows)
                {
                    for (const auto &run : row)
                        lanes[run.lane].push_back(run.segment);
                }

                std::vector<Segment> ordered;
                ordered.reserve(segments.size());
                std::vector<bool> laneUsed(lanes.size(), false);
                std::size_t lanesRemaining = 0;
                for (const auto &lane : lanes)
                {
                    if (!lane.empty())
                        ++lanesRemaining;
                }
                bool haveLast = entryPoint != nullptr;
                Point last = entryPoint ? *entryPoint : Point{};
                std::size_t lastUsedLane = lanes.size(); // sentinel: no lane used yet

                while (lanesRemaining > 0)
                {
                    std::size_t bestLane = lanes.size();
                    float bestCost = std::numeric_limits<float>::infinity();
                    std::vector<Segment> bestOriented;

                    auto tryLane = [&](std::size_t laneIndex)
                    {
                        if (laneIndex >= lanes.size() || laneUsed[laneIndex] || lanes[laneIndex].empty())
                            return;
                        for (const bool reverseRows : {false, true})
                        {
                            std::vector<Segment> oriented =
                                orientStripeLane(lanes[laneIndex], reverseRows, haveLast ? &last : nullptr);
                            if (oriented.empty())
                                continue;
                            const float cost = haveLast
                                                   ? transitionCostForStripeOrdering(map, last, oriented.front().a, settings, zonePoly)
                                                   : 0.0f;
                            if (cost < bestCost)
                            {
                                bestCost = cost;
                                bestLane = laneIndex;
                                bestOriented = std::move(oriented);
                            }
                        }
                    };

                    // Phase 1: prefer the nearest adjacent lane to maintain boustrophedon order
                    // and avoid long diagonal transits. Expand outward from lastUsedLane until a
                    // reachable lane is found. Only fall through to phase 2 if all adjacent lanes
                    // are blocked (infinite A* cost).
                    if (lastUsedLane < lanes.size())
                    {
                        for (std::size_t delta = 1; delta < lanes.size(); ++delta)
                        {
                            if (lastUsedLane >= delta)
                                tryLane(lastUsedLane - delta);
                            if (lastUsedLane + delta < lanes.size())
                                tryLane(lastUsedLane + delta);
                            if (bestLane != lanes.size())
                                break; // found a reachable lane at this delta, stop expanding
                        }
                    }

                    // Phase 2: global greedy — used for the very first lane (no lastUsedLane),
                    // or when all adjacent lanes are unreachable (e.g. blocked by obstacles).
                    if (bestLane == lanes.size())
                    {
                        for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
                            tryLane(laneIndex);
                    }

                    if (bestLane == lanes.size())
                    {
                        // If no planner-visible transition exists, keep deterministic row order.
                        // appendTransition() will mark the route invalid instead of inserting an
                        // unsafe shortcut, but this keeps the remaining coverage stable.
                        for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
                        {
                            if (!laneUsed[laneIndex] && !lanes[laneIndex].empty())
                            {
                                bestLane = laneIndex;
                                bestOriented = orientStripeLane(lanes[laneIndex], false, haveLast ? &last : nullptr);
                                break;
                            }
                        }
                    }

                    if (bestLane == lanes.size() || bestOriented.empty())
                        continue;

                    laneUsed[bestLane] = true;
                    lastUsedLane = bestLane;
                    --lanesRemaining;
                    ordered.insert(ordered.end(), bestOriented.begin(), bestOriented.end());
                    last = bestOriented.back().b;
                    haveLast = true;
                }

                return ordered;
            }

            /// Clip contour to runs inside zonePoly and outside exclusions.
            /// Used for headland generation: zonePoly is the outer boundary of a working-area
            /// component; exclusions are subtracted separately (not baked into zonePoly).
            static std::vector<PolygonPoints> clipContourToAllowedRuns(
                const PolygonPoints &contour,
                const PolygonPoints &zonePoly,
                const std::vector<PolygonPoints> &exclusions)
            {
                std::vector<PolygonPoints> runs;
                if (contour.size() < 2 || zonePoly.empty())
                    return runs;

                PolygonPoints current;
                const size_t n = contour.size();
                for (size_t i = 0; i < n; ++i)
                {
                    const Point &a = contour[i];
                    const Point &b = contour[(i + 1) % n];
                    const float distance = a.distanceTo(b);
                    const int steps = std::max(1, static_cast<int>(std::ceil(distance / 0.05f)));

                    for (int step = 0; step <= steps; ++step)
                    {
                        const float t = static_cast<float>(step) / static_cast<float>(steps);
                        const Point p{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};

                        bool allowed = pointInPolygonLocal(zonePoly, p.x, p.y);
                        if (allowed)
                        {
                            for (const auto &ex : exclusions)
                            {
                                if (pointInPolygonLocal(ex, p.x, p.y))
                                {
                                    allowed = false;
                                    break;
                                }
                            }
                        }
                        if (allowed)
                        {
                            appendUniquePoint(current, p, 1e-3f);
                        }
                        else if (current.size() >= 2)
                        {
                            runs.push_back(current);
                            current.clear();
                        }
                        else
                        {
                            current.clear();
                        }
                    }
                }
                if (current.size() >= 2)
                    runs.push_back(current);
                return runs;
            }

            static void appendSegmentRoute(RoutePlan &route,
                                           const Map &map,
                                           const Segment &segment,
                                           const ZoneSettings &settings,
                                           const PolygonPoints &zonePoly,
                                           const std::string &zoneId = {},
                                           const std::string &componentId = {})
            {
                auto makePoint = [&](const Point &p)
                {
                    RoutePoint rp;
                    rp.p = p;
                    rp.reverse = false;
                    rp.slow = false;
                    rp.reverseAllowed = settings.reverseAllowed;
                    rp.clearance_m = settings.clearance;
                    rp.sourceMode = WayType::MOW;
                    rp.semantic = RouteSemantic::COVERAGE_INFILL;
                    rp.zoneId = zoneId;
                    rp.componentId = componentId;
                    return rp;
                };

                if (route.points.empty())
                {
                    route.points.push_back(makePoint(segment.a));
                    route.points.push_back(makePoint(segment.b));
                    route.sourceMode = WayType::MOW;
                    route.active = true;
                    return;
                }

                if (!appendTransition(route, map, route.points.back().p, segment.a, settings, zonePoly, zoneId, componentId))
                    return;
                if (!samePoint(route.points.back().p, segment.a))
                {
                    route.points.push_back(makePoint(segment.a));
                }
                route.points.push_back(makePoint(segment.b));
                route.sourceMode = WayType::MOW;
                route.active = true;
            }

            static nav::ZoneSettings applyMissionOverrides(const nav::ZoneSettings &base,
                                                           const nlohmann::json &overrides)
            {
                nav::ZoneSettings settings = base;
                if (!overrides.is_object())
                    return settings;
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

            static ZonePlanSettingsSnapshot snapshotZoneSettings(const ZoneSettings &settings)
            {
                ZonePlanSettingsSnapshot snapshot;
                snapshot.stripWidth_m = settings.stripWidth;
                snapshot.angle_deg = settings.angle;
                snapshot.edgeMowing = settings.edgeMowing;
                snapshot.edgeRounds = settings.edgeRounds;
                snapshot.speed_mps = settings.speed;
                snapshot.pattern = (settings.pattern == ZonePattern::SPIRAL) ? "spiral" : "stripe";
                snapshot.reverseAllowed = settings.reverseAllowed;
                snapshot.clearance_m = settings.clearance;
                return snapshot;
            }

            /// Zone-aware transition: the A* grid is bounded to zonePoly so the path
            /// cannot take shortcuts through areas outside the active zone.
            /// If no path is found, route.valid is set to false — no silent direct-point fallback.
            static bool appendTransition(RoutePlan &route,
                                         const Map &map,
                                         const Point &from,
                                         const Point &to,
                                         const ZoneSettings &settings,
                                         const PolygonPoints &zonePoly,
                                         const std::string &zoneId,
                                         const std::string &componentId,
                                         RouteSemantic semanticOverride)
            {
                if (samePoint(from, to))
                    return true;
                const RouteSemantic sem = semanticOverride != RouteSemantic::UNKNOWN
                                              ? semanticOverride
                                              : (zonePoly.empty() ? RouteSemantic::TRANSIT_INTER_ZONE
                                                                  : RouteSemantic::TRANSIT_WITHIN_ZONE);

                if (directTransitionAllowed(map, from, to, zonePoly))
                {
                    RoutePoint rp;
                    rp.p = to;
                    rp.reverse = false;
                    rp.slow = false;
                    rp.reverseAllowed = settings.reverseAllowed;
                    rp.clearance_m = settings.clearance;
                    rp.sourceMode = WayType::MOW;
                    rp.semantic = sem;
                    rp.zoneId = zoneId;
                    rp.componentId = componentId;
                    if (route.points.empty() || !samePoint(route.points.back().p, rp.p))
                        route.points.push_back(rp);
                    route.sourceMode = WayType::MOW;
                    route.active = true;
                    return true;
                }

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

                if (transition.points.empty())
                {
                    // Constrained A* failed (zone polygon blocked the path, e.g. concave zone
                    // or narrow section). Retry without constraintZone so A* can briefly exit
                    // the zone polygon. Points get TRANSIT_INTER_ZONE semantic so the
                    // RouteValidator skips zone-containment but still checks perimeter/exclusions.
                    if (!zonePoly.empty())
                    {
                        const RoutePlan fallback = map.previewPath(
                            from, to, WayType::MOW, WayType::MOW, 0.0f, false,
                            settings.reverseAllowed, settings.clearance,
                            settings.clearance + map.plannerSettings().obstacleInflation_m,
                            {});
                        if (!fallback.points.empty())
                        {
                            for (auto point : fallback.points)
                            {
                                if (!route.points.empty() && samePoint(route.points.back().p, point.p))
                                    continue;
                                point.semantic = RouteSemantic::TRANSIT_INTER_ZONE;
                                point.zoneId = zoneId;
                                point.componentId = componentId;
                                route.points.push_back(point);
                            }
                            route.sourceMode = WayType::MOW;
                            route.active = true;
                            return true;
                        }
                    }

                    route.valid = false;
                    if (route.invalidReason.empty())
                    {
                        route.invalidReason = "no transition path from (" + std::to_string(from.x) + "," + std::to_string(from.y) + ") to (" + std::to_string(to.x) + "," + std::to_string(to.y) + ")" + (zoneId.empty() ? "" : " in zone '" + zoneId + "'");
                    }
                    return false;
                }

                for (auto point : transition.points)
                {
                    if (!route.points.empty() && samePoint(route.points.back().p, point.p))
                        continue;
                    point.semantic = sem;
                    point.zoneId = zoneId;
                    point.componentId = componentId;
                    route.points.push_back(point);
                }
                route.sourceMode = WayType::MOW;
                route.active = true;
                return true;
            }

            static void appendSegmentsWithTransitions(RoutePlan &route,
                                                      const Map &map,
                                                      const std::vector<Segment> &segments,
                                                      const ZoneSettings &settings,
                                                      const PolygonPoints &zonePoly,
                                                      const std::string &zoneId = {},
                                                      const std::string &componentId = {})
            {
                for (const auto &segment : segments)
                {
                    appendSegmentRoute(route, map, segment, settings, zonePoly, zoneId, componentId);
                }
            }

            static void appendContourRuns(RoutePlan &route,
                                          const Map &map,
                                          const std::vector<PolygonPoints> &runs,
                                          const ZoneSettings &settings,
                                          const PolygonPoints &zonePoly,
                                          const std::string &zoneId = {},
                                          const std::string &componentId = {})
            {
                for (const auto &run : runs)
                {
                    RoutePlan runRoute = routeFromPolyline(run, settings, RouteSemantic::COVERAGE_EDGE, zoneId, componentId);
                    if (runRoute.points.empty())
                        continue;
                    if (!route.points.empty())
                    {
                        if (!appendTransition(route, map, route.points.back().p, runRoute.points.front().p, settings, zonePoly, zoneId, componentId))
                            continue;
                    }
                    route = appendRoute(route, runRoute);
                }
            }

        } // namespace

        ZonePlan buildZonePlanPreview(const Map &map,
                                      const Zone &zone,
                                      const nlohmann::json &overrides)
        {
            nlohmann::json mission = {
                {"zoneIds", {zone.id}},
                {"overrides", {{zone.id, overrides.is_object() ? overrides : nlohmann::json::object()}}}};

            const ZoneSettings effectiveSettings = [&]()
            {
                ZoneSettings base{};
                if (!zone.activeProfileId.empty())
                {
                    auto it = zone.profiles.find(zone.activeProfileId);
                    if (it != zone.profiles.end())
                        base = it->second.settings;
                }
                return applyMissionOverrides(base, overrides);
            }();
            const RoutePlan route = buildMissionMowRoutePreview(map, mission);

            ZonePlan plan;
            plan.zoneId = zone.id;
            plan.zoneName = zone.name;
            plan.settings = snapshotZoneSettings(effectiveSettings);
            plan.route = route;
            plan.valid = route.valid;
            plan.invalidReason = route.invalidReason;
            if (!route.points.empty())
            {
                plan.startPoint = route.points.front().p;
                plan.endPoint = route.points.back().p;
                plan.hasStartPoint = true;
                plan.hasEndPoint = true;
            }

            plan.waypoints = routePlanToWaypoints(plan.route);

            return plan;
        }

        MissionPlan buildMissionPlanPreview(const Map &map, const nlohmann::json &missionJson)
        {
            MissionPlan plan;
            plan.missionId = missionJson.value("id", std::string{});

            if (missionJson.contains("zoneIds") && missionJson["zoneIds"].is_array())
            {
                for (const auto &zoneId : missionJson["zoneIds"])
                {
                    if (zoneId.is_string())
                        plan.zoneOrder.push_back(zoneId.get<std::string>());
                }
            }

            const nlohmann::json overrides = missionJson.contains("overrides") && missionJson["overrides"].is_object()
                                                 ? missionJson["overrides"]
                                                 : nlohmann::json::object();

            for (const auto &zoneId : plan.zoneOrder)
            {
                for (const auto &zone : map.zones())
                {
                    if (zone.id != zoneId)
                        continue;

                    const nlohmann::json zoneOverrides = overrides.value(zone.id, nlohmann::json::object());
                    plan.zonePlans.push_back(buildZonePlanPreview(map, zone, zoneOverrides));
                    break;
                }
            }

            plan.route = buildMissionMowRoutePreview(map, missionJson);
            plan.valid = plan.route.valid;
            plan.invalidReason = plan.route.invalidReason;

            plan.waypoints = routePlanToWaypoints(plan.route);

            return plan;
        }

        RoutePlan buildMissionMowRoutePreview(const Map &map, const nlohmann::json &missionJson)
        {
            RoutePlan route;
            route.sourceMode = WayType::MOW;
            route.active = false;

            std::vector<std::string> requestedZoneIds;
            if (missionJson.contains("zoneIds") && missionJson["zoneIds"].is_array())
            {
                for (const auto &id : missionJson["zoneIds"])
                {
                    if (id.is_string())
                        requestedZoneIds.push_back(id.get<std::string>());
                }
            }

            const auto applyForZone = [&](const Zone &zone)
            {
                const nlohmann::json overrides = missionJson.contains("overrides") && missionJson["overrides"].is_object()
                                                     ? missionJson["overrides"].value(zone.id, nlohmann::json::object())
                                                     : nlohmann::json::object();
                // Start from the zone's active profile settings (if any), then apply
                // mission-level overrides on top.  This gives the full priority chain:
                //   defaults → active profile → mission overrides
                ZoneSettings base{};
                if (!zone.activeProfileId.empty())
                {
                    auto it = zone.profiles.find(zone.activeProfileId);
                    if (it != zone.profiles.end())
                        base = it->second.settings;
                }
                return applyMissionOverrides(base, overrides);
            };

            const auto findZone = [&](const std::string &id) -> const Zone *
            {
                for (const auto &zone : map.zones())
                {
                    if (zone.id == id)
                        return &zone;
                }
                return nullptr;
            };

            for (const auto &zoneId : requestedZoneIds)
            {
                const Zone *zone = findZone(zoneId);
                if (!zone || zone->polygon.size() < 3)
                    continue;

                const ZoneSettings settings = applyForZone(*zone);
                PreparedCoverageArea prepared = prepareCoverageArea(*zone, map);
                if (prepared.zone.size() < 3)
                    continue;

                // Compute workingArea = zone ∩ perimeter − exclusions.
                // If nothing remains, this zone is not mowable and must not silently
                // fall back to the raw zone geometry.
                prepared.workingArea = computeWorkingArea(prepared.zone, prepared.perimeter, prepared.exclusions);
                if (prepared.workingArea.empty())
                    continue;

                // Use all connected components of the working area independently.
                // Components are ordered deterministically so componentId assignment does
                // not depend on polygon output order from the clipping stage.
                const std::vector<PolygonPoints> stableComponents = sortComponentsStable(prepared.workingArea);
                const std::vector<PolygonPoints> components = sortComponentsForCoverageOrder(prepared.workingArea, settings.angle);
                std::vector<RoutePlan> componentRoutes;
                componentRoutes.reserve(components.size());

                for (std::size_t componentIndex = 0; componentIndex < components.size(); ++componentIndex)
                {
                    const auto &compPoly = components[componentIndex];
                    if (compPoly.size() < 3)
                        continue;
                    std::size_t stableIndex = componentIndex;
                    for (std::size_t sortedIndex = 0; sortedIndex < stableComponents.size(); ++sortedIndex)
                    {
                        if (polygonsEquivalent(compPoly, stableComponents[sortedIndex]))
                        {
                            stableIndex = sortedIndex;
                            break;
                        }
                    }
                    const std::string componentId = zoneId + ":c" + std::to_string(stableIndex + 1);
                    RoutePlan compRoute;
                    compRoute.sourceMode = WayType::MOW;
                    compRoute.active = false;

                    // Headland: offset compPoly and clip with exclusions subtracted.
                    // compPoly is the outer boundary; exclusions are subtracted separately
                    // (not baked in — Clipper2 hole rings are excluded from workingArea).
                    if (settings.edgeMowing && settings.edgeRounds > 0)
                    {
                        for (int round = 1; round <= settings.edgeRounds; ++round)
                        {
                            const float inset = (static_cast<float>(round) - 0.5f) * settings.stripWidth;
                            const std::vector<PolygonPoints> contours = offsetPolygonRounded(compPoly, inset);
                            for (const auto &contour : contours)
                            {
                                const std::vector<PolygonPoints> contourRuns =
                                    clipContourToAllowedRuns(contour, compPoly, prepared.exclusions);
                                if (!contourRuns.empty())
                                    appendContourRuns(compRoute, map, contourRuns, settings, compPoly, zoneId, componentId);
                            }
                        }
                    }

                    // Infill: pass exclusions explicitly so effectiveIntervalsAtY subtracts them.
                    // compPoly is already zone ∩ perimeter; passing perimeter again is a no-op.
                    const float infillInset = std::max(0.0f, static_cast<float>(settings.edgeRounds) * settings.stripWidth);
                    RoutePlan stripeRoute;
                    stripeRoute.sourceMode = WayType::MOW;
                    stripeRoute.active = false;
                    const std::vector<PolygonPoints> infillPolys = infillInset > 0.0f
                                                                       ? offsetPolygon(compPoly, infillInset)
                                                                       : std::vector<PolygonPoints>{compPoly};
                    for (const auto &infillPoly : infillPolys)
                    {
                        const float exclusionPadding = settings.clearance +
                                                       map.plannerSettings().obstacleInflation_m +
                                                       map.plannerSettings().gridCellSize_m * 0.5f;
                        const std::vector<Segment> stripes = buildStripeSegments(
                            infillPoly,
                            prepared.perimeter,
                            prepared.exclusions,
                            settings.stripWidth,
                            settings.angle,
                            exclusionPadding);
                        // For subsequent infill polygons continue from the last stripe endpoint;
                        // for the first polygon pass no hint so the lane ordering begins with a
                        // clean boustrophedon (bottom-to-top). The headland→infill jump is already
                        // handled by appendTransition below and does not need an ordering bias here.
                        const Point *stripeEntry = stripeRoute.points.empty()
                                                       ? nullptr
                                                       : &stripeRoute.points.back().p;
                        const std::vector<Segment> orderedStripes =
                            orderStripeSegmentsByLane(map, stripes, settings, compPoly, settings.angle, stripeEntry);
                        appendSegmentsWithTransitions(stripeRoute, map, orderedStripes, settings, compPoly, zoneId, componentId);
                    }
                    if (!stripeRoute.points.empty())
                    {
                        if (!compRoute.points.empty())
                        {
                            if (!appendTransition(compRoute, map, compRoute.points.back().p, stripeRoute.points.front().p, settings, compPoly, zoneId, componentId))
                                continue;
                        }
                        compRoute = appendRoute(compRoute, stripeRoute);
                    }
                    if (!compRoute.points.empty())
                        componentRoutes.push_back(std::move(compRoute));
                }

                if (componentRoutes.empty())
                    continue;

                RoutePlan zoneRoute = componentRoutes.front();
                zoneRoute.sourceMode = WayType::MOW;
                zoneRoute.active = !zoneRoute.points.empty();

                bool allowReverseCurrent = true;
                for (std::size_t componentIndex = 1; componentIndex < componentRoutes.size(); ++componentIndex)
                {
                    const ComponentJoinChoice join = selectComponentJoin(
                        map,
                        zoneRoute,
                        componentRoutes[componentIndex],
                        settings,
                        prepared.zone,
                        zoneId,
                        allowReverseCurrent,
                        RouteSemantic::TRANSIT_BETWEEN_COMPONENTS);

                    if (!join.ok)
                    {
                        zoneRoute.valid = false;
                        if (zoneRoute.invalidReason.empty())
                        {
                            zoneRoute.invalidReason = "no legal transition between working-area components in zone '" + zoneId + "'";
                        }
                        break;
                    }

                    zoneRoute = join.current;
                    zoneRoute = appendRoute(zoneRoute, join.transition);
                    zoneRoute = appendRoute(zoneRoute, join.next);
                    allowReverseCurrent = false;
                }

                if (route.points.empty())
                {
                    route = zoneRoute;
                }
                else if (!zoneRoute.points.empty())
                {
                    // Inter-zone transition: no zone constraint (robot must cross between zones)
                    if (!appendTransition(route, map, route.points.back().p, zoneRoute.points.front().p, settings, {}, {}, {}, RouteSemantic::TRANSIT_INTER_ZONE))
                        break;
                    route = appendRoute(route, zoneRoute);
                }
            }

            route.active = !route.points.empty();
            return route;
        }

    } // namespace nav
} // namespace sunray
