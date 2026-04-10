#pragma once

/// ZonePlanner — pure-function zone planner.
///
/// Input : zone polygon, perimeter, hard exclusions, ZoneSettings.
/// Output: ZonePlan.waypoints — ordered list of mowing positions.
///
/// No Map dependency, no A*, no GridMap, no Costmap.
/// Transit between stripes = direct straight-line with mowOn=false.
/// Calling plan() twice with identical input produces identical output.

#include "core/navigation/Route.h"
#include "core/navigation/Map.h" // ZoneSettings

#include <string>
#include <vector>

namespace sunray
{
    namespace nav
    {

        class ZonePlanner
        {
        public:
            /// Plan a single zone.
            /// @param zoneId    stored in the returned ZonePlan.
            /// @param zone      zone polygon (world coordinates, metres).
            /// @param perimeter outer perimeter polygon; empty = no clipping.
            /// @param hardExclusions hard no-go polygons to subtract from the zone.
            /// @param settings  strip width, angle, edge rounds, etc.
            /// @returns ZonePlan with .waypoints populated; .valid = false on failure.
            static ZonePlan plan(
                const std::string &zoneId,
                const PolygonPoints &zone,
                const PolygonPoints &perimeter,
                const std::vector<PolygonPoints> &hardExclusions,
                const ZoneSettings &settings);
        };

    } // namespace nav
} // namespace sunray
