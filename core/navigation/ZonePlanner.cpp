/// ZonePlanner.cpp — self-contained zone coverage planner.
///
/// Algorithm:
///   1. computeWorkingArea(zone ∩ perimeter − hardExclusions) → one or more polygons ("blocks").
///      Multiple blocks arise when a hard exclusion splits the zone at its boundary.
///   2. Sort blocks deterministically by centroid (x then y).
///   3. For each block:
///        a. Edge rounds: inset contours, walk polygon perimeter with mowOn=true.
///        b. Infill: inset by edgeRounds×stripWidth, generate boustrophedon stripes.
///   4. Connect blocks/stripes with direct transit waypoints (mowOn=false).
///   5. Fill ZonePlan.waypoints, startPoint, endPoint.
///
/// Geometry helpers are all file-local (anonymous namespace) so this file has no
/// dependency on MissionCompiler, RuntimeState, GridMap, Costmap or PathPlanner.

#include "core/navigation/ZonePlanner.h"
#include "core/navigation/StripingAlgorithm.h"

#include <clipper2/clipper.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    using namespace sunray::nav;

    // ── Clipper2 helpers ──────────────────────────────────────────────────────

    static Clipper2Lib::PathD toClipperPath(const PolygonPoints &poly)
    {
        Clipper2Lib::PathD path;
        path.reserve(poly.size());
        for (const auto &p : poly)
            path.emplace_back(static_cast<double>(p.x), static_cast<double>(p.y));
        return path;
    }

    static PolygonPoints fromClipperPath(const Clipper2Lib::PathD &path)
    {
        PolygonPoints poly;
        poly.reserve(path.size());
        for (const auto &p : path)
            poly.push_back({static_cast<float>(p.x), static_cast<float>(p.y)});
        return poly;
    }

    /// Inset polygon; positive inset = shrink, negative = expand.
    static std::vector<PolygonPoints> offsetPolygon(const PolygonPoints &poly, float inset)
    {
        if (poly.size() < 3)
            return {};
        if (std::fabs(inset) < 1e-6f)
            return {poly};

        Clipper2Lib::PathsD paths = {toClipperPath(poly)};
        const auto result = Clipper2Lib::InflatePaths(
            paths,
            static_cast<double>(-inset), // Clipper2: negative delta = inset/shrink
            Clipper2Lib::JoinType::Miter,
            Clipper2Lib::EndType::Polygon);

        std::vector<PolygonPoints> out;
        for (const auto &p : result)
            if (Clipper2Lib::Area(p) > 0.0 && p.size() >= 3)
                out.push_back(fromClipperPath(p));
        return out;
    }

    /// workingArea = zone ∩ perimeter − hardExclusions.
    /// Returns outer-ring polygons only (hole rings are skipped).
    /// Multiple polygons arise when exclusions split the zone at a boundary.
    static std::vector<PolygonPoints> computeWorkingArea(
        const PolygonPoints &zone,
        const PolygonPoints &perimeter,
        const std::vector<PolygonPoints> &exclusions)
    {
        if (zone.size() < 3)
            return {};

        Clipper2Lib::PathsD work;
        work.push_back(toClipperPath(zone));

        if (perimeter.size() >= 3)
        {
            Clipper2Lib::PathsD perimPaths = {toClipperPath(perimeter)};
            work = Clipper2Lib::Intersect(work, perimPaths, Clipper2Lib::FillRule::NonZero);
            if (work.empty())
                return {};
        }

        for (const auto &excl : exclusions)
        {
            if (excl.size() < 3)
                continue;
            Clipper2Lib::PathsD exclPaths = {toClipperPath(excl)};
            work = Clipper2Lib::Difference(work, exclPaths, Clipper2Lib::FillRule::NonZero);
            if (work.empty())
                return {};
        }

        std::vector<PolygonPoints> result;
        for (const auto &path : work)
        {
            // Positive area = outer ring (CCW); negative = hole ring (CW, skip).
            if (Clipper2Lib::Area(path) <= 0.0)
                continue;
            auto poly = fromClipperPath(path);
            if (poly.size() >= 3)
                result.push_back(std::move(poly));
        }
        return result;
    }

    // ── Point / polygon utilities ─────────────────────────────────────────────

    static Point polygonCentroid(const PolygonPoints &poly)
    {
        Point c{0.0f, 0.0f};
        for (const auto &p : poly)
        {
            c.x += p.x;
            c.y += p.y;
        }
        if (!poly.empty())
        {
            const float inv = 1.0f / static_cast<float>(poly.size());
            c.x *= inv;
            c.y *= inv;
        }
        return c;
    }

    /// Stable sort: deterministic order regardless of Clipper2 output order.
    static std::vector<PolygonPoints> sortBlocksStable(std::vector<PolygonPoints> blocks)
    {
        std::sort(blocks.begin(), blocks.end(), [](const PolygonPoints &a, const PolygonPoints &b)
                  {
            const Point ca = polygonCentroid(a), cb = polygonCentroid(b);
            if (std::fabs(ca.x - cb.x) > 1e-4f) return ca.x < cb.x;
            if (std::fabs(ca.y - cb.y) > 1e-4f) return ca.y < cb.y;
            return a.size() < b.size(); });
        return blocks;
    }

    // ── Waypoint builder ──────────────────────────────────────────────────────

    /// Append a waypoint, skipping exact duplicates (same position, same mowOn).
    static void appendWaypoint(std::vector<Waypoint> &wps, float x, float y, bool mowOn)
    {
        if (!wps.empty())
        {
            const auto &last = wps.back();
            if (std::fabs(last.x - x) < 5e-4f && std::fabs(last.y - y) < 5e-4f)
            {
                // At the same position: only allow mowOn=false → mowOn=true transition
                if (last.mowOn == mowOn || (last.mowOn && !mowOn))
                    return; // skip
            }
        }
        wps.push_back({x, y, mowOn});
    }

    /// Build all waypoints for one block polygon (edge rounds + infill stripes).
    static std::vector<Waypoint> buildBlockWaypoints(
        const PolygonPoints &block,
        const std::vector<PolygonPoints> &exclusions,
        const ZoneSettings &settings)
    {
        std::vector<Waypoint> wps;

        auto transit = [&](float x, float y)
        {
            if (wps.empty())
                return; // first position: no transit needed
            appendWaypoint(wps, x, y, false);
        };
        auto cover = [&](float x, float y)
        { appendWaypoint(wps, x, y, true); };

        // ── Edge rounds ───────────────────────────────────────────────────────
        if (settings.edgeMowing && settings.edgeRounds > 0)
        {
            for (int r = 1; r <= settings.edgeRounds; ++r)
            {
                // Each round r rides the contour at (r-0.5)×stripWidth inset from the boundary
                const float inset = (static_cast<float>(r) - 0.5f) * settings.stripWidth;
                for (const auto &contour : offsetPolygon(block, inset))
                {
                    if (contour.size() < 2)
                        continue;
                    transit(contour.front().x, contour.front().y);
                    for (const auto &pt : contour)
                        cover(pt.x, pt.y);
                    cover(contour.front().x, contour.front().y); // close the loop
                }
            }
        }

        // ── Infill stripes ────────────────────────────────────────────────────
        const float infillInset = static_cast<float>(settings.edgeRounds) * settings.stripWidth;
        const auto infillPolys = (infillInset > 1e-4f)
                                     ? offsetPolygon(block, infillInset)
                                     : std::vector<PolygonPoints>{block};

        for (const auto &infillPoly : infillPolys)
        {
            const auto stripes = sunray::nav::buildStripeSegments(infillPoly, {}, exclusions,
                                                                  settings.stripWidth, settings.angle, 0.0f);
            for (const auto &stripe : stripes)
            {
                // Transit to stripe start (mowOn=false while navigating there)
                transit(stripe.a.x, stripe.a.y);
                // Explicitly start mowing AT the beginning of the stripe
                cover(stripe.a.x, stripe.a.y);
                // Mow to the stripe end
                cover(stripe.b.x, stripe.b.y);
            }
        }

        return wps;
    }

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
namespace sunray
{
    namespace nav
    {

        ZonePlan ZonePlanner::plan(
            const std::string &zoneId,
            const PolygonPoints &zone,
            const PolygonPoints &perimeter,
            const std::vector<PolygonPoints> &hardExclusions,
            const ZoneSettings &settings)
        {
            ZonePlan result;
            result.zoneId = zoneId;
            result.valid = false;

            auto blocks = computeWorkingArea(zone, perimeter, hardExclusions);
            if (blocks.empty())
            {
                result.invalidReason = "no mowable area (zone outside perimeter or fully excluded)";
                return result;
            }

            blocks = sortBlocksStable(std::move(blocks));

            std::vector<Waypoint> allWaypoints;
            for (std::size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx)
            {
                auto blockWps = buildBlockWaypoints(blocks[blockIdx], hardExclusions, settings);
                if (blockWps.empty())
                    continue;

                // Transit between blocks
                if (!allWaypoints.empty())
                    allWaypoints.push_back({blockWps.front().x, blockWps.front().y, false});

                allWaypoints.insert(allWaypoints.end(), blockWps.begin(), blockWps.end());
            }

            if (allWaypoints.empty())
            {
                result.invalidReason = "no waypoints generated (zone too small for strip width)";
                return result;
            }

            result.waypoints = std::move(allWaypoints);
            result.startPoint = {result.waypoints.front().x, result.waypoints.front().y};
            result.endPoint = {result.waypoints.back().x, result.waypoints.back().y};
            result.hasStartPoint = true;
            result.hasEndPoint = true;
            result.valid = true;
            return result;
        }

    } // namespace nav
} // namespace sunray
