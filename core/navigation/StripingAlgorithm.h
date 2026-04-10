#pragma once

/// StripingAlgorithm — shared boustrophedon stripe-generation logic.
///
/// Single source of truth for the scanline / rotation algorithm used by both
/// MissionCompiler (production A*-aware route) and ZonePlanner
/// (lightweight waypoint plan).  Neither planner duplicates this code anymore.
///
/// Algorithm overview:
///   1. Rotate zone, perimeter and exclusions by −angleDeg so stripes become
///      horizontal in the rotated frame.
///   2. For each scanline y (stepping stripWidth apart), compute the
///      x-intervals that lie inside the zone, intersected with the perimeter
///      and with any exclusions subtracted.
///   3. Rotate the resulting segment endpoints back by +angleDeg.
///   4. Emit pairs in boustrophedon (snake) order.
///
/// Callers that do not need perimeter clipping pass an empty PolygonPoints{}.
/// Callers that do not need exclusion padding pass exclusionPadding = 0.0f.

#include "core/navigation/Route.h" // Point, PolygonPoints

#include <vector>

namespace sunray
{
    namespace nav
    {

        /// A single mowing stripe: start point a → end point b (world coordinates).
        struct Segment
        {
            Point a;
            Point b;
        };

        /// Build boustrophedon stripe segments for one polygon block.
        ///
        /// @param zone             Polygon to stripe (world coordinates).
        /// @param perimeter        Outer boundary; empty = no additional clipping.
        /// @param exclusions       Hard no-go polygon list to subtract from each row.
        /// @param stripWidth       Distance between stripe centre-lines (metres > 0).
        /// @param angleDeg         Stripe direction in degrees (0 = along +X axis).
        /// @param exclusionPadding Extra outward expansion applied to each exclusion
        ///                         before subtracting (used to keep stripe endpoints
        ///                         outside the A* obstacle-inflation radius).
        ///                         Pass 0.0f when no padding is required.
        /// @returns                Ordered list of segments; empty if zone < 3 points
        ///                         or stripWidth ≤ 0.
        std::vector<Segment> buildStripeSegments(
            const PolygonPoints &zone,
            const PolygonPoints &perimeter,
            const std::vector<PolygonPoints> &exclusions,
            float stripWidth,
            float angleDeg,
            float exclusionPadding = 0.0f);

    } // namespace nav
} // namespace sunray
