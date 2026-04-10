/// WaypointExecutor.cpp — runtime waypoint list follower.

#include "core/navigation/WaypointExecutor.h"

#include <algorithm>

namespace sunray
{
    namespace nav
    {

        void WaypointExecutor::startPlan(const ZonePlan &plan)
        {
            if (!plan.route.points.empty())
            {
                startPlan(plan.route);
                return;
            }
            startPlan(plan.waypoints);
        }

        void WaypointExecutor::startPlan(const RoutePlan &plan)
        {
            startPlan(routePlanToWaypoints(plan));
        }

        void WaypointExecutor::startPlan(std::vector<Waypoint> waypoints)
        {
            waypoints_ = std::move(waypoints);
            index_ = 0;
            done_ = waypoints_.empty();
        }

        void WaypointExecutor::reset()
        {
            waypoints_.clear();
            dynamicObstacles_.clear();
            index_ = 0;
            done_ = true;
        }

        const Waypoint *WaypointExecutor::currentWaypoint() const
        {
            if (done_ || waypoints_.empty() || index_ >= static_cast<int>(waypoints_.size()))
                return nullptr;
            return &waypoints_[static_cast<std::size_t>(index_)];
        }

        float WaypointExecutor::progress() const
        {
            if (waypoints_.empty())
                return 0.0f;
            if (done_)
                return 1.0f;
            return static_cast<float>(index_) / static_cast<float>(waypoints_.size());
        }

        void WaypointExecutor::seekToIndex(int index)
        {
            if (waypoints_.empty())
            {
                index_ = 0;
                done_ = true;
                return;
            }

            index_ = std::clamp(index, 0, static_cast<int>(waypoints_.size()) - 1);
            done_ = false;
        }

        void WaypointExecutor::advanceToNext()
        {
            if (done_ || waypoints_.empty())
                return;
            ++index_;
            if (index_ >= static_cast<int>(waypoints_.size()))
                done_ = true;
        }

        void WaypointExecutor::addDynamicObstacle(PolygonPoints obs)
        {
            dynamicObstacles_.push_back(std::move(obs));
        }

        void WaypointExecutor::clearDynamicObstacles()
        {
            dynamicObstacles_.clear();
        }

    } // namespace nav
} // namespace sunray
