#pragma once

/// WaypointExecutor — runtime waypoint list follower.
///
/// Owns an active plan (list of Waypoints) and a current index.
/// The Op code (MowOp) calls:
///   - startPlan()        — load a new plan, reset index.
///   - currentWaypoint()  — get next navigation target (nullptr when done or no plan).
///   - advanceToNext()    — called by LineTracker / MowOp when target is reached.
///   - isDone()           — true once all waypoints have been advanced past.
///
/// Does NOT plan anything. Pure execution.

#include "core/navigation/Route.h"

#include <vector>
#include <string>

namespace sunray
{
    namespace nav
    {

        class WaypointExecutor
        {
        public:
            /// Load a new plan from a ZonePlan's route; resets progress.
            void startPlan(const ZonePlan &plan);

            /// Load a new plan from a compiled route; resets progress.
            void startPlan(const RoutePlan &plan);

            /// Load a raw waypoint list directly (e.g. from NavPlanner output).
            void startPlan(std::vector<Waypoint> waypoints);

            /// Clear the active plan. isDone() returns true, currentWaypoint() returns nullptr.
            void reset();

            // ── Query ─────────────────────────────────────────────────────────

            bool hasPlan() const { return !waypoints_.empty(); }
            bool isDone() const { return done_; }

            /// Current navigation target. nullptr when done or no plan loaded.
            const Waypoint *currentWaypoint() const;

            int currentIndex() const { return index_; }
            int totalWaypoints() const { return static_cast<int>(waypoints_.size()); }

            /// Progress fraction [0, 1]. 0 before start, 1 when done.
            float progress() const;

            /// Seek to a specific waypoint index after loading a plan.
            void seekToIndex(int index);

            // ── Advance ───────────────────────────────────────────────────────

            /// Move to the next waypoint. Call when LineTracker reports target reached.
            void advanceToNext();

            // ── N5 placeholder ────────────────────────────────────────────────
            // Dynamic obstacles for temporary detour support (N5-scope, not yet used).
            void addDynamicObstacle(PolygonPoints obs);
            void clearDynamicObstacles();

        private:
            std::vector<Waypoint> waypoints_;
            std::vector<PolygonPoints> dynamicObstacles_; ///< N5 placeholder
            int index_ = 0;
            bool done_ = true;
        };

    } // namespace nav
} // namespace sunray
