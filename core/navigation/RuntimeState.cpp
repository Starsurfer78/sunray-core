#include "core/navigation/RuntimeState.h"

#include "core/navigation/Map.h"
#include "core/navigation/PathPlanner.h"

#include <algorithm>
#include <cmath>

namespace sunray
{
    namespace nav
    {
        namespace
        {
            static constexpr float TRACK_TARGET_REACHED_TOLERANCE_M = 0.2f;

            float scalePI(float v)
            {
                float res = std::fmod(v, 2.0f * static_cast<float>(M_PI));
                if (res > static_cast<float>(M_PI))
                    res -= 2.0f * static_cast<float>(M_PI);
                if (res < -static_cast<float>(M_PI))
                    res += 2.0f * static_cast<float>(M_PI);
                return res;
            }

            float distancePI(float x, float w)
            {
                float d = scalePI(x - w);
                if (d < -static_cast<float>(M_PI))
                    d += 2.0f * static_cast<float>(M_PI);
                if (d > static_cast<float>(M_PI))
                    d -= 2.0f * static_cast<float>(M_PI);
                return d;
            }

            float pointsAngle(float x1, float y1, float x2, float y2)
            {
                return std::atan2(y2 - y1, x2 - x1);
            }

            bool pointInPolygon(const PolygonPoints &poly, float px, float py)
            {
                if (poly.size() < 3)
                    return false;
                bool inside = false;
                const std::size_t n = poly.size();
                for (std::size_t i = 0, j = n - 1; i < n; j = i++)
                {
                    const float xi = poly[i].x;
                    const float yi = poly[i].y;
                    const float xj = poly[j].x;
                    const float yj = poly[j].y;
                    const bool intersect = ((yi > py) != (yj > py)) &&
                                           (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
                    if (intersect)
                        inside = !inside;
                }
                return inside;
            }

            bool lineCircleIntersects(const Point &src, const Point &dst,
                                      const Point &center, float radius)
            {
                const float dx = dst.x - src.x;
                const float dy = dst.y - src.y;
                const float len2 = dx * dx + dy * dy;
                if (len2 < 1e-8f)
                    return src.distanceTo(center) < radius;
                float t = ((center.x - src.x) * dx + (center.y - src.y) * dy) / len2;
                t = std::clamp(t, 0.0f, 1.0f);
                const float cx = src.x + t * dx - center.x;
                const float cy = src.y + t * dy - center.y;
                return (cx * cx + cy * cy) < radius * radius;
            }

            const Zone *findZoneById(const Map &map, const std::string &zoneId)
            {
                for (const auto &zone : map.zones())
                {
                    if (zone.id == zoneId)
                        return &zone;
                }
                return nullptr;
            }

            float routeClearanceForPoint(const Map &map, const Point &point, WayType mode)
            {
                (void)point;
                (void)mode;
                return std::max(0.05f, map.plannerSettings().defaultClearance_m);
            }

            bool routeReverseAllowedForPoint(const Map &map, const Point &point, WayType mode)
            {
                (void)point;
                if (mode == WayType::DOCK)
                {
                    return map.dockMeta().approachMode == DockApproachMode::REVERSE_ALLOWED;
                }
                return false;
            }

            Point dockApproachTarget(const Map &map)
            {
                if (map.dockMeta().corridor.empty())
                    return map.dockRoutePlan().points.front().p;

                Point center;
                for (const auto &p : map.dockMeta().corridor)
                {
                    center.x += p.x;
                    center.y += p.y;
                }
                const float denom = static_cast<float>(map.dockMeta().corridor.size());
                center.x /= denom;
                center.y /= denom;
                return center;
            }

            float kidnapToleranceForMode(const Map &map, WayType mode)
            {
                switch (mode)
                {
                case WayType::DOCK:
                    return std::max(0.6f, map.dockMeta().slowZoneRadius_m + 0.25f);
                case WayType::FREE:
                    return std::max(2.0f, map.plannerSettings().defaultClearance_m + map.plannerSettings().obstacleInflation_m + 0.5f);
                case WayType::MOW:
                default:
                    return 3.0f;
                }
            }

            const RoutePoint *activeRoutePointForPlanning(const Map &map, const RuntimeState &runtime)
            {
                if (runtime.wayMode() == WayType::FREE)
                {
                    const auto &route = runtime.freeRoutePlan();
                    if (runtime.freePointsIdx() >= 0 && runtime.freePointsIdx() < static_cast<int>(route.points.size()))
                        return &route.points[runtime.freePointsIdx()];
                }
                if (runtime.wayMode() == WayType::MOW)
                {
                    const auto &route = runtime.mowRoutePlan();
                    if (runtime.mowPointsIdx() >= 0 && runtime.mowPointsIdx() < static_cast<int>(route.points.size()))
                        return &route.points[runtime.mowPointsIdx()];
                }
                if (runtime.wayMode() == WayType::DOCK)
                {
                    const auto &route = map.dockRoutePlan();
                    if (runtime.dockPointsIdx() >= 0 && runtime.dockPointsIdx() < static_cast<int>(route.points.size()))
                        return &route.points[runtime.dockPointsIdx()];
                }
                return nullptr;
            }
        } // namespace

        RuntimeState::RuntimeState(std::shared_ptr<Config> config)
            : config_(std::move(config))
        {
        }

        void RuntimeState::clear()
        {
            targetPoint_ = Point{};
            lastTargetPoint_ = Point{};
            trackReverse_ = false;
            trackSlow_ = false;
            wayMode_ = WayType::MOW;
            percentCompleted_ = 0;
            mowRoute_ = RoutePlan{};
            localRoute_ = RoutePlan{};
            freeRoute_ = FreeRouteContext{};
            lastReplanAttempt_ms_ = 0;
            mowPointsIdx_ = 0;
            dockPointsIdx_ = 0;
            freePointsIdx_ = 0;
            isDocked_ = false;
            shouldDock_ = false;
            shouldMow_ = false;
        }

        void RuntimeState::seekToMowIndex(int idx)
        {
            if (mowRoute_.points.empty())
                return;
            if (idx < 0)
                idx = 0;
            if (idx >= static_cast<int>(mowRoute_.points.size()))
                idx = static_cast<int>(mowRoute_.points.size()) - 1;
            mowPointsIdx_ = idx;
            trackReverse_ = mowRoute_.points[mowPointsIdx_].reverse;
            trackSlow_ = mowRoute_.points[mowPointsIdx_].slow;
            targetPoint_ = mowRoute_.points[mowPointsIdx_].p;
        }

        void RuntimeState::activateMowRoute(const RoutePlan &route)
        {
            mowRoute_ = route;
            mowRoute_.active = true;
            percentCompleted_ = 0;
            localRoute_ = RoutePlan{};
            freeRoute_ = FreeRouteContext{};
            freePointsIdx_ = 0;
            dockPointsIdx_ = 0;
        }

        bool RuntimeState::startPlannedMowing(const Map &map, float robotX, float robotY, const RoutePlan &route)
        {
            (void)map;
            if (route.points.empty())
                return false;

            activateMowRoute(route);

            float bestDist = 1e9f;
            int bestIdx = 0;
            for (int i = 0; i < static_cast<int>(mowRoute_.points.size()); ++i)
            {
                const float d = Point(robotX, robotY).distanceTo(mowRoute_.points[i].p);
                if (d < bestDist)
                {
                    bestDist = d;
                    bestIdx = i;
                }
            }

            mowPointsIdx_ = bestIdx;
            shouldMow_ = true;
            shouldDock_ = false;
            wayMode_ = WayType::MOW;
            setLastTargetPoint(robotX, robotY);
            trackReverse_ = mowRoute_.points[mowPointsIdx_].reverse;
            trackSlow_ = mowRoute_.points[mowPointsIdx_].slow;
            targetPoint_ = mowRoute_.points[mowPointsIdx_].p;
            return true;
        }

        bool RuntimeState::startPlannedMowing(const Map &map, float robotX, float robotY, const MissionPlan &plan)
        {
            return startPlannedMowing(map, robotX, robotY, plan.route);
        }

        bool RuntimeState::startDocking(const Map &map, float robotX, float robotY)
        {
            if (map.dockRoutePlan().points.empty())
                return false;

            const Point approachTarget = dockApproachTarget(map);
            // Set shouldDock_ before planPath so planning mode resolves to DOCK,
            // allowing the path to exit the active mow zone.
            shouldDock_ = true;
            const bool havePath = planPath(map, {robotX, robotY}, approachTarget);
            if (!havePath || localRoute_.points.empty())
            {
                // planPath failed (e.g. dock approach target is at/outside perimeter edge).
                // Fall back to direct DOCK mode so DockOp's TransitionRouter can
                // still create a proper obstacle-aware path in begin().
                dockPointsIdx_ = 0;
                shouldDock_ = true;
                shouldMow_ = false;
                wayMode_ = WayType::DOCK;
                freeRoute_ = FreeRouteContext{};
                localRoute_ = RoutePlan{};
                freePointsIdx_ = 0;
                setLastTargetPoint(robotX, robotY);
                trackReverse_ = map.dockRoutePlan().points[dockPointsIdx_].reverse;
                trackSlow_ = true;
                targetPoint_ = map.dockRoutePlan().points[dockPointsIdx_].p;
                return true;
            }

            dockPointsIdx_ = 0;
            shouldDock_ = true;
            shouldMow_ = false;
            setLastTargetPoint(robotX, robotY);
            beginFreeRoute(WayType::DOCK,
                           map.dockRoutePlan().points.front().p,
                           false,
                           map.dockRoutePlan().points.size() <= 2,
                           mowPointsIdx_,
                           0,
                           localRoute_);
            return true;
        }

        bool RuntimeState::retryDocking(const Map &map, float robotX, float robotY, float lateralOffsetM)
        {
            if (std::fabs(lateralOffsetM) < 1e-4f)
                return startDocking(map, robotX, robotY);

            if (map.dockRoutePlan().points.size() < 2)
                return startDocking(map, robotX, robotY);

            const Point &last = map.dockRoutePlan().points.back().p;
            const Point &penultimate = map.dockRoutePlan().points[map.dockRoutePlan().points.size() - 2].p;
            const float dx = last.x - penultimate.x;
            const float dy = last.y - penultimate.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-4f)
                return startDocking(map, robotX, robotY);

            const Point baseApproach = dockApproachTarget(map);
            const float nx = (-dy / len) * lateralOffsetM;
            const float ny = (dx / len) * lateralOffsetM;
            const Point offsetApproach{baseApproach.x + nx, baseApproach.y + ny};

            if (!planPath(map, {robotX, robotY}, offsetApproach) || localRoute_.points.empty())
                return startDocking(map, robotX, robotY);

            dockPointsIdx_ = 0;
            shouldDock_ = true;
            shouldMow_ = false;
            setLastTargetPoint(robotX, robotY);
            beginFreeRoute(WayType::DOCK,
                           map.dockRoutePlan().points.front().p,
                           false,
                           map.dockRoutePlan().points.size() <= 2,
                           mowPointsIdx_,
                           0,
                           localRoute_);
            return true;
        }

        bool RuntimeState::startUndocking(const Map &map, float robotX, float robotY)
        {
            if (map.dockRoutePlan().points.size() < 2)
                return false;

            dockPointsIdx_ = static_cast<int>(map.dockRoutePlan().points.size()) - 1;
            shouldDock_ = false;
            shouldMow_ = false;
            wayMode_ = WayType::DOCK; // Use DOCK mode to follow the dock path backwards
            freeRoute_ = FreeRouteContext{};
            localRoute_ = RoutePlan{};
            freePointsIdx_ = 0;
            
            setLastTargetPoint(robotX, robotY);
            targetPoint_ = map.dockRoutePlan().points[dockPointsIdx_ - 1].p;
            trackReverse_ = true; // Force reverse for the first undock segment
            trackSlow_ = true;
            
            return true;
        }

        bool RuntimeState::isDocking() const
        {
            return shouldDock_ && (wayMode_ == WayType::DOCK || wayMode_ == WayType::FREE);
        }

        bool RuntimeState::isUndocking() const
        {
            return !shouldDock_ && (wayMode_ == WayType::DOCK);
        }

        bool RuntimeState::nextPoint(const Map &map, bool sim, float robotX, float robotY)
        {
            (void)robotX;
            (void)robotY;
            bool ok = false;
            switch (wayMode_)
            {
            case WayType::MOW:
                ok = nextMowPoint(sim);
                break;
            case WayType::DOCK:
                ok = nextDockPoint(map, sim);
                break;
            case WayType::FREE:
                ok = nextFreePoint();
                break;
            default:
                return false;
            }

            if (ok && wayMode_ != WayType::FREE)
            {
                for (const auto &obs : map.obstacles())
                {
                    const float r = obs.radius_m + map.plannerSettings().obstacleInflation_m;
                    if (lineCircleIntersects(targetPoint_, lastTargetPoint_, obs.center, r))
                    {
                        const WayType resumeMode = wayMode_;
                        const Point resumeTarget = targetPoint_;
                        const bool resumeTrackReverse = trackReverse_;
                        const bool resumeTrackSlow = trackSlow_;
                        const int resumeMowIdx = mowPointsIdx_;
                        const int resumeDockIdx = dockPointsIdx_;

                        if (planPath(map, lastTargetPoint_, targetPoint_))
                        {
                            beginFreeRoute(resumeMode,
                                           resumeTarget,
                                           resumeTrackReverse,
                                           resumeTrackSlow,
                                           resumeMowIdx,
                                           resumeDockIdx,
                                           localRoute_);
                        }
                        break;
                    }
                }
            }
            return ok;
        }

        bool RuntimeState::nextMowPoint(bool sim)
        {
            (void)sim;
            if (mowRoute_.points.empty())
                return false;
            ++mowPointsIdx_;
            if (mowPointsIdx_ >= static_cast<int>(mowRoute_.points.size()))
            {
                mowPointsIdx_ = static_cast<int>(mowRoute_.points.size()) - 1;
                percentCompleted_ = 100;
                return false;
            }
            percentCompleted_ = static_cast<int>(100.0f * mowPointsIdx_ / mowRoute_.points.size());
            lastTargetPoint_ = targetPoint_;
            trackReverse_ = mowRoute_.points[mowPointsIdx_].reverse;
            trackSlow_ = mowRoute_.points[mowPointsIdx_].slow;
            targetPoint_ = mowRoute_.points[mowPointsIdx_].p;
            return true;
        }

        bool RuntimeState::nextDockPoint(const Map &map, bool sim)
        {
            (void)sim;
            if (map.dockRoutePlan().points.empty())
                return false;
                
            if (!shouldDock_) {
                // We are undocking (going backwards along the dock path)
                --dockPointsIdx_;
                if (dockPointsIdx_ <= 0)
                    return false; // Reached the start of the dock path
                
                lastTargetPoint_ = targetPoint_;
                targetPoint_ = map.dockRoutePlan().points[dockPointsIdx_ - 1].p;
                // For the first segment leaving the station we force reverse,
                // afterwards we drive forward
                trackReverse_ = (dockPointsIdx_ == static_cast<int>(map.dockRoutePlan().points.size()) - 1);
                trackSlow_ = false;
                return true;
            }
            
            // We are docking
            ++dockPointsIdx_;
            if (dockPointsIdx_ >= static_cast<int>(map.dockRoutePlan().points.size()))
                return false;
            lastTargetPoint_ = targetPoint_;
            targetPoint_ = map.dockRoutePlan().points[dockPointsIdx_].p;
            trackReverse_ = map.dockRoutePlan().points[dockPointsIdx_].reverse;
            trackSlow_ = map.dockRoutePlan().points[dockPointsIdx_].slow;
            return true;
        }

        bool RuntimeState::nextFreePoint()
        {
            if (localRoute_.points.empty())
                return false;
            ++freePointsIdx_;
            if (freePointsIdx_ >= static_cast<int>(localRoute_.points.size()))
            {
                if (!freeRoute_.active)
                    return false;

                lastTargetPoint_ = targetPoint_;
                wayMode_ = freeRoute_.resumeMode;
                targetPoint_ = freeRoute_.resumeTarget;
                trackReverse_ = freeRoute_.resumeTrackReverse;
                trackSlow_ = freeRoute_.resumeTrackSlow;
                mowPointsIdx_ = freeRoute_.resumeMowIdx;
                dockPointsIdx_ = freeRoute_.resumeDockIdx;
                freeRoute_ = FreeRouteContext{};
                localRoute_ = RoutePlan{};
                freePointsIdx_ = 0;
                return true;
            }

            lastTargetPoint_ = targetPoint_;
            targetPoint_ = localRoute_.points[freePointsIdx_].p;
            trackReverse_ = localRoute_.points[freePointsIdx_].reverse;
            trackSlow_ = localRoute_.points[freePointsIdx_].slow;
            return true;
        }

        bool RuntimeState::currentMowOn() const
        {
            if (wayMode_ == WayType::FREE && freePointsIdx_ >= 0 && freePointsIdx_ < static_cast<int>(localRoute_.points.size()))
                return routePointMowOn(localRoute_.points[freePointsIdx_]);
            if (wayMode_ == WayType::MOW && mowPointsIdx_ >= 0 && mowPointsIdx_ < static_cast<int>(mowRoute_.points.size()))
                return routePointMowOn(mowRoute_.points[mowPointsIdx_]);
            return false;
        }

        bool RuntimeState::nextPointIsStraight() const
        {
            Point nextPt;
            bool hasNext = false;

            if (wayMode_ == WayType::MOW && mowPointsIdx_ + 1 < static_cast<int>(mowRoute_.points.size()))
            {
                nextPt = mowRoute_.points[mowPointsIdx_ + 1].p;
                hasNext = true;
            }

            if (!hasNext)
                return true;

            const float a1 = pointsAngle(lastTargetPoint_.x, lastTargetPoint_.y,
                                         targetPoint_.x, targetPoint_.y);
            const float a2 = pointsAngle(targetPoint_.x, targetPoint_.y,
                                         nextPt.x, nextPt.y);
            return std::fabs(distancePI(a1, a2)) < (20.0f / 180.0f * static_cast<float>(M_PI));
        }

        float RuntimeState::distanceToTargetPoint(float rx, float ry) const
        {
            return Point(rx, ry).distanceTo(targetPoint_);
        }

        float RuntimeState::distanceToLastTargetPoint(float rx, float ry) const
        {
            return Point(rx, ry).distanceTo(lastTargetPoint_);
        }

        bool RuntimeState::mowingCompleted() const
        {
            return !mowRoute_.points.empty() && mowPointsIdx_ >= static_cast<int>(mowRoute_.points.size()) - 1;
        }

        void RuntimeState::setMowingPointPercent(float pct)
        {
            if (mowRoute_.points.empty())
                return;
            pct = std::clamp(pct, 0.0f, 100.0f);
            mowPointsIdx_ = static_cast<int>(pct / 100.0f * (mowRoute_.points.size() - 1));
        }

        void RuntimeState::skipNextMowingPoint()
        {
            if (mowPointsIdx_ + 1 < static_cast<int>(mowRoute_.points.size()))
            {
                ++mowPointsIdx_;
                trackReverse_ = mowRoute_.points[mowPointsIdx_].reverse;
                trackSlow_ = mowRoute_.points[mowPointsIdx_].slow;
                targetPoint_ = mowRoute_.points[mowPointsIdx_].p;
            }
        }

        void RuntimeState::setLastTargetPoint(float x, float y)
        {
            lastTargetPoint_ = Point(x, y);
        }

        RouteSegment RuntimeState::currentTrackingSegment(const Map &map, float robotX, float robotY, float robotHeadingRad) const
        {
            RouteSegment segment;
            segment.lastTarget = lastTargetPoint_;
            segment.target = targetPoint_;
            segment.reverse = trackReverse_;
            segment.slow = trackSlow_;
            segment.sourceMode = wayMode_;
            segment.clearance_m = map.plannerSettings().defaultClearance_m;
            segment.reverseAllowed = false;

            if (wayMode_ == WayType::FREE && freePointsIdx_ >= 0 && freePointsIdx_ < static_cast<int>(localRoute_.points.size()))
            {
                segment.reverse = localRoute_.points[freePointsIdx_].reverse;
                segment.slow = localRoute_.points[freePointsIdx_].slow;
                segment.reverseAllowed = localRoute_.points[freePointsIdx_].reverseAllowed;
                segment.clearance_m = localRoute_.points[freePointsIdx_].clearance_m;
                segment.sourceMode = localRoute_.points[freePointsIdx_].sourceMode;
            }
            else if (wayMode_ == WayType::MOW && mowPointsIdx_ >= 0 && mowPointsIdx_ < static_cast<int>(mowRoute_.points.size()))
            {
                segment.reverseAllowed = mowRoute_.points[mowPointsIdx_].reverseAllowed;
                segment.clearance_m = mowRoute_.points[mowPointsIdx_].clearance_m;
            }
            else if (wayMode_ == WayType::DOCK && dockPointsIdx_ >= 0 && dockPointsIdx_ < static_cast<int>(map.dockRoutePlan().points.size()))
            {
                segment.reverseAllowed = map.dockRoutePlan().points[dockPointsIdx_].reverseAllowed;
                segment.clearance_m = map.dockRoutePlan().points[dockPointsIdx_].clearance_m;
            }

            const bool finalAlignActive = dockingFinalAlignActive(map, robotX, robotY);
            if (finalAlignActive)
            {
                segment.reverse = dockingShouldReverseOnFinalApproach(map, robotX, robotY, robotHeadingRad);
                segment.slow = true;
                segment.sourceMode = WayType::DOCK;
                segment.reverseAllowed = dockingReverseAllowed(map);
                segment.hasTargetHeading = true;
                segment.targetHeadingRad = dockingFinalAlignHeadingRad(map);
            }

            segment.distanceToTarget_m = Point(robotX, robotY).distanceTo(segment.target);
            segment.distanceToLastTarget_m = Point(robotX, robotY).distanceTo(segment.lastTarget);
            segment.targetReached = segment.distanceToTarget_m < TRACK_TARGET_REACHED_TOLERANCE_M;
            segment.targetReachedTolerance_m = TRACK_TARGET_REACHED_TOLERANCE_M;
            if (!segment.hasTargetHeading)
            {
                segment.targetHeadingRad = std::atan2(segment.target.y - robotY, segment.target.x - robotX);
                segment.hasTargetHeading = true;
            }
            segment.nextSegmentStraight = nextPointIsStraight();
            segment.kidnapTolerance_m = kidnapToleranceForMode(map, segment.sourceMode);
            return segment;
        }

        bool RuntimeState::refreshTracking(const Map &map, unsigned long now_ms, float robotX, float robotY)
        {
            return maybeReplanToCurrentTarget(map, now_ms, robotX, robotY);
        }

        bool RuntimeState::advanceTracking(const Map &map, float robotX, float robotY)
        {
            return nextPoint(map, false, robotX, robotY);
        }

        bool RuntimeState::dockingFinalAlignActive(const Map &map, float robotX, float robotY) const
        {
            if (!shouldDock_ || map.dockRoutePlan().points.empty() || !map.dockMeta().hasFinalAlignHeading)
                return false;
            if (wayMode_ != WayType::DOCK && wayMode_ != WayType::FREE)
                return false;

            const Point dockGoal = map.dockRoutePlan().points.back().p;
            const float activationRadius = std::max(0.15f, map.dockMeta().slowZoneRadius_m);
            return (Point(robotX, robotY).distanceTo(dockGoal) <= activationRadius) ||
                   (targetPoint_.distanceTo(dockGoal) <= activationRadius);
        }

        float RuntimeState::dockingFinalAlignHeadingRad(const Map &map) const
        {
            return map.dockMeta().finalAlignHeading_deg / 180.0f * static_cast<float>(M_PI);
        }

        bool RuntimeState::dockingReverseAllowed(const Map &map) const
        {
            return shouldDock_ && (map.dockMeta().approachMode == DockApproachMode::REVERSE_ALLOWED);
        }

        bool RuntimeState::dockingShouldReverseOnFinalApproach(const Map &map, float robotX, float robotY, float robotHeadingRad) const
        {
            if (!dockingReverseAllowed(map) || !dockingFinalAlignActive(map, robotX, robotY))
                return false;

            const float forwardHeading = dockingFinalAlignHeadingRad(map);
            const float reverseHeading = scalePI(forwardHeading + static_cast<float>(M_PI));
            const float forwardError = std::fabs(distancePI(robotHeadingRad, forwardHeading));
            const float reverseError = std::fabs(distancePI(robotHeadingRad, reverseHeading));
            const float switchingMargin = 10.0f / 180.0f * static_cast<float>(M_PI);
            return (reverseError + switchingMargin) < forwardError;
        }

        bool RuntimeState::currentSegmentNeedsReplan(const Map &map, float robotX, float robotY) const
        {
            PlannerContext context;
            context.robotPose = {robotX, robotY};
            context.source = {robotX, robotY};
            context.destination = targetPoint_;
            context.lastTarget = lastTargetPoint_;
            context.missionMode = wayMode_;
            context.planningMode = (shouldDock_ || wayMode_ == WayType::DOCK) ? WayType::DOCK : WayType::MOW;
            context.robotRadius_m = map.plannerSettings().defaultClearance_m + map.plannerSettings().obstacleInflation_m;
            return PathPlanner::segmentNeedsReplan(map, context);
        }

        void RuntimeState::beginFreeRoute(WayType resumeMode,
                                          const Point &resumeTarget,
                                          bool resumeTrackReverse,
                                          bool resumeTrackSlow,
                                          int resumeMowIdx,
                                          int resumeDockIdx,
                                          const RoutePlan &route)
        {
            if (route.points.empty())
                return;

            freeRoute_.active = true;
            freeRoute_.resumeMode = resumeMode;
            freeRoute_.resumeTarget = resumeTarget;
            freeRoute_.resumeTrackReverse = resumeTrackReverse;
            freeRoute_.resumeTrackSlow = resumeTrackSlow;
            freeRoute_.resumeMowIdx = resumeMowIdx;
            freeRoute_.resumeDockIdx = resumeDockIdx;

            localRoute_ = route;
            localRoute_.sourceMode = resumeMode;
            localRoute_.active = true;
            // Propagate resumeMode to individual points so currentTrackingSegment
            // reports the correct navigation mode (DOCK/MOW) while following this
            // free-route leg instead of the path-planner's initial default.
            for (auto &pt : localRoute_.points)
                pt.sourceMode = resumeMode;

            freePointsIdx_ = 0;
            wayMode_ = WayType::FREE;
            trackReverse_ = localRoute_.points.front().reverse;
            trackSlow_ = localRoute_.points.front().slow;
            targetPoint_ = localRoute_.points.front().p;
        }

        void RuntimeState::injectFreePath(const Map &map, const std::vector<Point> &waypoints)
        {
            if (waypoints.empty())
                return;

            RoutePlan route;
            route.sourceMode = freeRoute_.active ? freeRoute_.resumeMode : wayMode_;
            route.active = true;
            route.points.reserve(waypoints.size());
            for (const auto &waypoint : waypoints)
            {
                RoutePoint routePoint;
                routePoint.p = waypoint;
                routePoint.sourceMode = route.sourceMode;
                routePoint.clearance_m = routeClearanceForPoint(map, waypoint, route.sourceMode);
                routePoint.reverseAllowed = routeReverseAllowedForPoint(map, waypoint, route.sourceMode);
                route.points.push_back(routePoint);
            }

            const WayType resumeMode = freeRoute_.active ? freeRoute_.resumeMode : wayMode_;
            beginFreeRoute(resumeMode,
                           targetPoint_,
                           trackReverse_,
                           trackSlow_,
                           mowPointsIdx_,
                           dockPointsIdx_,
                           route);
        }

        bool RuntimeState::planPath(const Map &map, const Point &src, const Point &dst)
        {
            PlannerContext context;
            const RoutePoint *activePoint = activeRoutePointForPlanning(map, *this);
            context.robotPose = src;
            context.source = src;
            context.destination = dst;
            context.lastTarget = lastTargetPoint_;
            context.missionMode = activePoint ? activePoint->sourceMode : wayMode_;
            context.planningMode = (shouldDock_ || wayMode_ == WayType::DOCK) ? WayType::DOCK : WayType::MOW;
            context.clearance_m = activePoint ? activePoint->clearance_m : routeClearanceForPoint(map, dst, context.missionMode);
            context.reverseAllowed = activePoint ? activePoint->reverseAllowed : routeReverseAllowedForPoint(map, dst, context.missionMode);
            context.semantic = (context.missionMode == WayType::MOW)
                                   ? RouteSemantic::RECOVERY
                                   : (context.missionMode == WayType::DOCK ? RouteSemantic::DOCK_APPROACH
                                                                           : RouteSemantic::UNKNOWN);

            if (activePoint)
            {
                context.zoneId = activePoint->zoneId;
                context.componentId = activePoint->componentId;
                if (context.missionMode == WayType::MOW &&
                    context.planningMode != WayType::DOCK &&
                    !activePoint->zoneId.empty() &&
                    activePoint->semantic != RouteSemantic::TRANSIT_INTER_ZONE)
                {
                    const Zone *zone = findZoneById(map, activePoint->zoneId);
                    if (zone)
                        context.constraintZone = zone->polygon;
                }
            }
            if (lastTargetPoint_.distanceTo(src) > 1e-4f)
            {
                context.headingReferenceRad = pointsAngle(lastTargetPoint_.x, lastTargetPoint_.y, src.x, src.y);
                context.hasHeadingReference = true;
            }
            context.robotRadius_m = context.clearance_m + map.plannerSettings().obstacleInflation_m;

            localRoute_ = PathPlanner::planPath(map, context);
            return !localRoute_.points.empty();
        }

        bool RuntimeState::replanToCurrentTarget(const Map &map, float robotX, float robotY)
        {
            if (wayMode_ == WayType::FREE)
                return false;
            if (targetPoint_.distanceTo({robotX, robotY}) < 1e-4f)
                return false;

            const WayType resumeMode = wayMode_;
            const Point resumeTarget = targetPoint_;
            const bool resumeTrackReverse = trackReverse_;
            const bool resumeTrackSlow = trackSlow_;
            const int resumeMowIdx = mowPointsIdx_;
            const int resumeDockIdx = dockPointsIdx_;

            if (!planPath(map, {robotX, robotY}, targetPoint_) || localRoute_.points.empty())
                return false;

            setLastTargetPoint(robotX, robotY);
            beginFreeRoute(resumeMode,
                           resumeTarget,
                           resumeTrackReverse,
                           resumeTrackSlow,
                           resumeMowIdx,
                           resumeDockIdx,
                           localRoute_);
            return true;
        }

        bool RuntimeState::maybeReplanToCurrentTarget(const Map &map, unsigned long now_ms, float robotX, float robotY)
        {
            if (wayMode_ == WayType::FREE)
                return false;
            if (now_ms - lastReplanAttempt_ms_ < map.plannerSettings().replanPeriod_ms)
                return false;
            if (!currentSegmentNeedsReplan(map, robotX, robotY))
                return false;

            lastReplanAttempt_ms_ = now_ms;
            return replanToCurrentTarget(map, robotX, robotY);
        }

    } // namespace nav
} // namespace sunray