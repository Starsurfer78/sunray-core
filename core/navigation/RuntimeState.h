#pragma once

#include "core/navigation/Route.h"

#include <memory>
#include <vector>

namespace sunray
{
    class Config;

    namespace nav
    {
        class Map;

        class RuntimeState
        {
        public:
            explicit RuntimeState(std::shared_ptr<Config> config = nullptr);

            void clear();

            bool startPlannedMowing(const Map &map, float robotX, float robotY, const RoutePlan &route);
            bool startPlannedMowing(const Map &map, float robotX, float robotY, const MissionPlan &plan);
            bool startDocking(const Map &map, float robotX, float robotY);
            bool retryDocking(const Map &map, float robotX, float robotY, float lateralOffsetM = 0.0f);

            void setIsDocked(bool flag) { isDocked_ = flag; }
            bool isDocking() const;
            bool isUndocking() const;

            bool nextPoint(const Map &map, bool sim, float robotX, float robotY);
            bool nextPointIsStraight() const;
            float distanceToTargetPoint(float rx, float ry) const;
            float distanceToLastTargetPoint(float rx, float ry) const;
            bool mowingCompleted() const;
            void setMowingPointPercent(float pct);
            void skipNextMowingPoint();

            bool hasActiveMowingPlan() const { return !mowRoute_.points.empty(); }
            /// N6.3: seek to a specific mow waypoint index (after dock interrupt).
            void seekToMowIndex(int idx);
            const RoutePlan &mowRoutePlan() const { return mowRoute_; }
            const RoutePlan &freeRoutePlan() const { return localRoute_; }

            const Point &targetPoint() const { return targetPoint_; }
            const Point &lastTargetPoint() const { return lastTargetPoint_; }
            bool trackReverse() const { return trackReverse_; }
            bool trackSlow() const { return trackSlow_; }
            bool currentMowOn() const;
            WayType wayMode() const { return wayMode_; }
            int percentCompleted() const { return percentCompleted_; }
            int mowPointsIdx() const { return mowPointsIdx_; }
            int dockPointsIdx() const { return dockPointsIdx_; }
            int freePointsIdx() const { return freePointsIdx_; }

            RouteSegment currentTrackingSegment(const Map &map, float robotX, float robotY, float robotHeadingRad) const;
            bool refreshTracking(const Map &map, unsigned long now_ms, float robotX, float robotY);
            bool advanceTracking(const Map &map, float robotX, float robotY);
            bool dockingFinalAlignActive(const Map &map, float robotX, float robotY) const;
            float dockingFinalAlignHeadingRad(const Map &map) const;
            bool dockingReverseAllowed(const Map &map) const;
            bool dockingShouldReverseOnFinalApproach(const Map &map, float robotX, float robotY, float robotHeadingRad) const;

            void injectFreePath(const Map &map, const std::vector<Point> &waypoints);
            bool replanToCurrentTarget(const Map &map, float robotX, float robotY);
            bool maybeReplanToCurrentTarget(const Map &map, unsigned long now_ms, float robotX, float robotY);

        private:
            struct FreeRouteContext
            {
                bool active = false;
                WayType resumeMode = WayType::MOW;
                Point resumeTarget;
                bool resumeTrackReverse = false;
                bool resumeTrackSlow = false;
                int resumeMowIdx = 0;
                int resumeDockIdx = 0;
            };

            void activateMowRoute(const RoutePlan &route);
            void beginFreeRoute(WayType resumeMode,
                                const Point &resumeTarget,
                                bool resumeTrackReverse,
                                bool resumeTrackSlow,
                                int resumeMowIdx,
                                int resumeDockIdx,
                                const RoutePlan &route);
            bool nextMowPoint(bool sim);
            bool nextDockPoint(const Map &map, bool sim);
            bool nextFreePoint();
            void setLastTargetPoint(float x, float y);
            bool currentSegmentNeedsReplan(const Map &map, float robotX, float robotY) const;
            bool planPath(const Map &map, const Point &src, const Point &dst);

            std::shared_ptr<Config> config_;
            Point targetPoint_;
            Point lastTargetPoint_;
            bool trackReverse_ = false;
            bool trackSlow_ = false;
            WayType wayMode_ = WayType::MOW;
            int percentCompleted_ = 0;
            RoutePlan mowRoute_;
            RoutePlan localRoute_;
            FreeRouteContext freeRoute_;
            unsigned long lastReplanAttempt_ms_ = 0;
            int mowPointsIdx_ = 0;
            int dockPointsIdx_ = 0;
            int freePointsIdx_ = 0;
            bool isDocked_ = false;
            bool shouldDock_ = false;
            bool shouldMow_ = false;
        };

    } // namespace nav
} // namespace sunray