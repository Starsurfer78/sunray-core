// Map.cpp — Waypoint management + polygon operations.

#include "core/navigation/Map.h"
#include "core/navigation/MissionCompiler.h"
#include "core/navigation/GridMap.h"
#include "core/navigation/PathPlanner.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace sunray
{
    namespace nav
    {
        static bool lineCircleIntersects(const Point &src, const Point &dst,
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

        // ── Construction ───────────────────────────────────────────────────────────────

        Map::Map(std::shared_ptr<Config> config)
            : config_(std::move(config))
        {
            applyConfigDefaults();
        }

        // ── Persistence ───────────────────────────────────────────────────────────────

        static PolygonPoints parsePoints(const nlohmann::json &arr)
        {
            PolygonPoints pts;
            pts.reserve(arr.size());
            for (auto &p : arr)
            {
                pts.push_back({p.at(0).get<float>(), p.at(1).get<float>()});
            }
            return pts;
        }

        static nlohmann::json encodePoints(const PolygonPoints &pts)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (auto &p : pts)
                arr.push_back({p.x, p.y});
            return arr;
        }

        static ExclusionType parseExclusionType(const std::string &value)
        {
            return value == "soft" ? ExclusionType::SOFT : ExclusionType::HARD;
        }

        static const char *encodeExclusionType(ExclusionType value)
        {
            return value == ExclusionType::SOFT ? "soft" : "hard";
        }

        static DockApproachMode parseDockApproachMode(const std::string &value)
        {
            return value == "reverse_allowed" ? DockApproachMode::REVERSE_ALLOWED
                                              : DockApproachMode::FORWARD_ONLY;
        }

        static const char *encodeDockApproachMode(DockApproachMode value)
        {
            return value == DockApproachMode::REVERSE_ALLOWED ? "reverse_allowed" : "forward_only";
        }

        static PolygonPoints exportPolygonPoints(const RoutePlan &route)
        {
            PolygonPoints points;
            points.reserve(route.points.size());
            for (const auto &routePoint : route.points)
            {
                points.push_back(routePoint.p);
            }
            return points;
        }

        static PlannerSettings parsePlannerSettings(const nlohmann::json &jp)
        {
            PlannerSettings settings;
            settings.defaultClearance_m = jp.value("defaultClearance", settings.defaultClearance_m);
            settings.perimeterSoftMargin_m = jp.value("perimeterSoftMargin", settings.perimeterSoftMargin_m);
            settings.perimeterHardMargin_m = jp.value("perimeterHardMargin", settings.perimeterHardMargin_m);
            settings.obstacleInflation_m = jp.value("obstacleInflation", settings.obstacleInflation_m);
            settings.softNoGoCostScale = jp.value("softNoGoCostScale", settings.softNoGoCostScale);
            settings.replanPeriod_ms = jp.value("replanPeriodMs", settings.replanPeriod_ms);
            settings.gridCellSize_m = jp.value("gridCellSize", settings.gridCellSize_m);
            return settings;
        }

        static nlohmann::json encodePlannerSettings(const PlannerSettings &settings)
        {
            return {
                {"defaultClearance", settings.defaultClearance_m},
                {"perimeterSoftMargin", settings.perimeterSoftMargin_m},
                {"perimeterHardMargin", settings.perimeterHardMargin_m},
                {"obstacleInflation", settings.obstacleInflation_m},
                {"softNoGoCostScale", settings.softNoGoCostScale},
                {"replanPeriodMs", settings.replanPeriod_ms},
                {"gridCellSize", settings.gridCellSize_m},
            };
        }

        static std::vector<ExclusionMeta> parseExclusionMeta(const nlohmann::json &arr)
        {
            std::vector<ExclusionMeta> meta;
            meta.reserve(arr.size());
            for (const auto &item : arr)
            {
                ExclusionMeta entry;
                entry.type = parseExclusionType(item.value("type", "hard"));
                entry.clearance_m = item.value("clearance", entry.clearance_m);
                entry.costScale = item.value("costScale", entry.costScale);
                meta.push_back(entry);
            }
            return meta;
        }

        static nlohmann::json encodeExclusionMeta(const std::vector<ExclusionMeta> &meta)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto &entry : meta)
            {
                arr.push_back({
                    {"type", encodeExclusionType(entry.type)},
                    {"clearance", entry.clearance_m},
                    {"costScale", entry.costScale},
                });
            }
            return arr;
        }

        static DockMeta parseDockMeta(const nlohmann::json &jd)
        {
            DockMeta meta;
            meta.approachMode = parseDockApproachMode(jd.value("approachMode", "forward_only"));
            if (jd.contains("corridor"))
                meta.corridor = parsePoints(jd["corridor"]);
            if (jd.contains("finalAlignHeadingDeg") && !jd["finalAlignHeadingDeg"].is_null())
            {
                meta.finalAlignHeading_deg = jd["finalAlignHeadingDeg"].get<float>();
                meta.hasFinalAlignHeading = true;
            }
            meta.slowZoneRadius_m = jd.value("slowZoneRadius", meta.slowZoneRadius_m);
            return meta;
        }

        static nlohmann::json encodeDockMeta(const DockMeta &meta)
        {
            nlohmann::json jd;
            jd["approachMode"] = encodeDockApproachMode(meta.approachMode);
            jd["corridor"] = encodePoints(meta.corridor);
            jd["finalAlignHeadingDeg"] = meta.hasFinalAlignHeading
                                             ? nlohmann::json(meta.finalAlignHeading_deg)
                                             : nlohmann::json(nullptr);
            jd["slowZoneRadius"] = meta.slowZoneRadius_m;
            return jd;
        }

        static ZoneSettings parseZoneSettings(const nlohmann::json &js)
        {
            ZoneSettings s;
            s.name = js.value("name", s.name);
            s.stripWidth = js.value("stripWidth", s.stripWidth);
            s.angle = js.value("angle", s.angle);
            s.edgeMowing = js.value("edgeMowing", s.edgeMowing);
            s.edgeRounds = js.value("edgeRounds", s.edgeRounds);
            s.speed = js.value("speed", s.speed);
            const std::string pat = js.value("pattern", std::string("stripe"));
            s.pattern = (pat == "spiral") ? ZonePattern::SPIRAL : ZonePattern::STRIPE;
            s.reverseAllowed = js.value("reverseAllowed", s.reverseAllowed);
            s.clearance = js.value("clearance", s.clearance);
            return s;
        }

        static nlohmann::json encodeZoneSettings(const ZoneSettings &s)
        {
            return {
                {"name", s.name},
                {"stripWidth", s.stripWidth},
                {"angle", s.angle},
                {"edgeMowing", s.edgeMowing},
                {"edgeRounds", s.edgeRounds},
                {"speed", s.speed},
                {"pattern", s.pattern == ZonePattern::SPIRAL ? "spiral" : "stripe"},
                {"reverseAllowed", s.reverseAllowed},
                {"clearance", s.clearance},
            };
        }

        static ZoneProfile parseZoneProfile(const nlohmann::json &jp)
        {
            ZoneProfile p;
            p.name = jp.value("name", p.name);
            p.created = jp.value("created", p.created);
            p.lastUsed = jp.value("lastUsed", p.lastUsed);
            if (jp.contains("settings") && jp["settings"].is_object())
                p.settings = parseZoneSettings(jp["settings"]);
            return p;
        }

        static nlohmann::json encodeZoneProfile(const ZoneProfile &p)
        {
            return {
                {"name", p.name},
                {"settings", encodeZoneSettings(p.settings)},
                {"created", p.created},
                {"lastUsed", p.lastUsed},
            };
        }

        bool Map::load(const std::filesystem::path &path)
        {
            try
            {
                std::ifstream f(path);
                if (!f.is_open())
                    return false;
                nlohmann::json j;
                f >> j;
                return loadJson(j);
            }
            catch (...)
            {
                clear();
                return false;
            }
        }

        bool Map::loadJson(const nlohmann::json &j)
        {
            clear();
            try
            {

                if (j.contains("perimeter"))
                    perimeterPoints_ = parsePoints(j["perimeter"]);
                PolygonPoints parsedDockPoints;
                if (j.contains("dock"))
                    parsedDockPoints = parsePoints(j["dock"]);
                if (j.contains("exclusions"))
                {
                    for (auto &ex : j["exclusions"])
                        exclusions_.push_back(parsePoints(ex));
                }
                if (j.contains("zones"))
                {
                    for (const auto &jz : j["zones"])
                    {
                        Zone z;
                        z.id = jz.value("id", "");
                        z.order = jz.value("order", 1);
                        if (jz.contains("polygon"))
                            z.polygon = parsePoints(jz["polygon"]);
                        // Read name: top-level first, fall back to legacy settings.name for backward compat
                        if (jz.contains("name"))
                            z.name = jz["name"].get<std::string>();
                        else if (jz.contains("settings") && jz["settings"].contains("name"))
                            z.name = jz["settings"]["name"].get<std::string>();
                        else
                            z.name = "Zone";
                        z.activeProfileId = jz.value("activeProfile", std::string{});
                        if (jz.contains("profiles") && jz["profiles"].is_object())
                        {
                            for (auto it = jz["profiles"].begin(); it != jz["profiles"].end(); ++it)
                            {
                                if (it.value().is_object())
                                    z.profiles[it.key()] = parseZoneProfile(it.value());
                            }
                        }
                        zones_.push_back(std::move(z));
                    }
                    // Sort by order
                    std::sort(zones_.begin(), zones_.end(), [](const Zone &a, const Zone &b)
                              { return a.order < b.order; });
                }
                if (j.contains("planner") && j["planner"].is_object())
                {
                    planner_ = parsePlannerSettings(j["planner"]);
                }
                if (j.contains("dockMeta") && j["dockMeta"].is_object())
                {
                    dockMeta_ = parseDockMeta(j["dockMeta"]);
                }
                if (j.contains("exclusionMeta") && j["exclusionMeta"].is_array())
                {
                    exclusionMeta_ = parseExclusionMeta(j["exclusionMeta"]);
                }
                while (exclusionMeta_.size() < exclusions_.size())
                {
                    exclusionMeta_.push_back(ExclusionMeta{});
                }
                if (j.contains("captureMeta") && j["captureMeta"].is_object())
                {
                    captureMeta_ = j["captureMeta"];
                }
                dockRoute_ = buildDockRoutePlan(parsedDockPoints);
                for (auto &routePoint : dockRoute_.points)
                {
                    routePoint.clearance_m = routeClearanceForPoint(routePoint.p, WayType::DOCK);
                    routePoint.reverseAllowed = routeReverseAllowedForPoint(routePoint.p, WayType::DOCK);
                }
                mapCRC_ = calcCRC();
                return true;
            }
            catch (...)
            {
                clear();
                return false;
            }
        }

        bool Map::save(const std::filesystem::path &path) const
        {
            try
            {
                const nlohmann::json j = exportMapJson();

                std::ofstream f(path);
                if (!f.is_open())
                    return false;
                f << j.dump(2);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        nlohmann::json Map::exportMapJson() const
        {
            nlohmann::json j;
            j["perimeter"] = encodePoints(perimeterPoints_);
            j["dock"] = encodePoints(exportPolygonPoints(dockRoute_));
            j["exclusions"] = nlohmann::json::array();
            for (const auto &ex : exclusions_)
                j["exclusions"].push_back(encodePoints(ex));

            j["zones"] = nlohmann::json::array();
            for (const auto &z : zones_)
            {
                nlohmann::json jz;
                jz["id"] = z.id;
                jz["name"] = z.name;
                jz["order"] = z.order;
                jz["polygon"] = encodePoints(z.polygon);
                jz["activeProfile"] = z.activeProfileId;
                jz["profiles"] = nlohmann::json::object();
                for (const auto &[profileId, profile] : z.profiles)
                    jz["profiles"][profileId] = encodeZoneProfile(profile);
                j["zones"].push_back(jz);
            }

            j["planner"] = encodePlannerSettings(planner_);
            j["dockMeta"] = encodeDockMeta(dockMeta_);
            if (!exclusionMeta_.empty())
            {
                j["exclusionMeta"] = encodeExclusionMeta(exclusionMeta_);
            }
            if (!captureMeta_.empty())
            {
                j["captureMeta"] = captureMeta_;
            }
            return j;
        }

        void Map::clear()
        {
            perimeterPoints_.clear();
            dockRoute_ = RoutePlan{};
            exclusions_.clear();
            exclusionMeta_.clear();
            obstacles_.clear();
            zones_.clear();
            applyConfigDefaults();
            captureMeta_ = nlohmann::json::object();
            mapCRC_ = 0;
        }

        RoutePlan Map::buildDockRoutePlan(const PolygonPoints &points)
        {
            RoutePlan route;
            route.sourceMode = WayType::DOCK;
            route.active = !points.empty();
            route.points.reserve(points.size());
            for (size_t i = 0; i < points.size(); ++i)
            {
                RoutePoint routePoint;
                routePoint.p = points[i];
                routePoint.reverse = false;
                routePoint.slow = i >= (points.size() > 2 ? points.size() - 2 : 0);
                routePoint.reverseAllowed = false;
                routePoint.clearance_m = 0.25f;
                routePoint.sourceMode = WayType::DOCK;
                route.points.push_back(routePoint);
            }
            return route;
        }

        void Map::applyConfigDefaults()
        {
            planner_ = PlannerSettings{};
            dockMeta_ = DockMeta{};

            if (!config_)
                return;

            planner_.defaultClearance_m = config_->get<float>("planner_default_clearance_m", planner_.defaultClearance_m);
            planner_.perimeterSoftMargin_m = config_->get<float>("planner_perimeter_soft_margin_m", planner_.perimeterSoftMargin_m);
            planner_.perimeterHardMargin_m = config_->get<float>("planner_perimeter_hard_margin_m", planner_.perimeterHardMargin_m);
            planner_.obstacleInflation_m = config_->get<float>("planner_obstacle_inflation_m", planner_.obstacleInflation_m);
            planner_.softNoGoCostScale = config_->get<float>("planner_soft_nogo_cost_scale", planner_.softNoGoCostScale);
            planner_.replanPeriod_ms = config_->get<unsigned long>("planner_replan_period_ms", planner_.replanPeriod_ms);
            planner_.gridCellSize_m = config_->get<float>("planner_grid_cell_size_m", planner_.gridCellSize_m);

            dockMeta_.approachMode = parseDockApproachMode(
                config_->get<std::string>("dock_approach_mode", encodeDockApproachMode(dockMeta_.approachMode)));
            dockMeta_.slowZoneRadius_m = config_->get<float>("dock_slow_zone_radius_m", dockMeta_.slowZoneRadius_m);
            dockMeta_.hasFinalAlignHeading = config_->get<bool>("dock_final_align_heading_enabled", false);
            if (dockMeta_.hasFinalAlignHeading)
            {
                dockMeta_.finalAlignHeading_deg = config_->get<float>(
                    "dock_final_align_heading_deg",
                    dockMeta_.finalAlignHeading_deg);
            }
        }

        long Map::calcCRC() const
        {
            long crc = 0;
            auto accum = [&](const PolygonPoints &pts)
            {
                for (auto &p : pts)
                {
                    crc ^= static_cast<long>(p.x * 1000) ^ static_cast<long>(p.y * 1000);
                }
            };
            accum(perimeterPoints_);
            accum(exportPolygonPoints(dockRoute_));
            for (auto &ex : exclusions_)
                accum(ex);
            return crc;
        }

        bool Map::getDockingPos(float &x, float &y, float &delta, int idx) const
        {
            if (dockRoute_.points.empty())
                return false;
            int i = (idx < 0) ? static_cast<int>(dockRoute_.points.size()) - 1 : idx;
            i = std::clamp(i, 0, static_cast<int>(dockRoute_.points.size()) - 1);

            x = dockRoute_.points[i].p.x;
            y = dockRoute_.points[i].p.y;

            // Heading: angle from previous dock point to this one.
            if (i > 0)
            {
                delta = pointsAngle(dockRoute_.points[i - 1].p.x, dockRoute_.points[i - 1].p.y, x, y);
            }
            else
            {
                delta = 0.0f;
            }
            return true;
        }

        // ── Obstacle management ────────────────────────────────────────────────────────

        bool Map::addObstacle(float x, float y, unsigned now_ms, bool persistent)
        {
            // If an existing obstacle is within merge_radius, just increment its hitCount.
            constexpr float mergeRadius = 0.5f;
            for (auto &obs : obstacles_)
            {
                if (obs.center.distanceTo({x, y}) < mergeRadius)
                {
                    obs.hitCount++;
                    obs.detectedAt_ms = now_ms; // refresh timestamp
                    return true;
                }
            }
            float radius_m = 0.3f;
            if (config_)
                radius_m = config_->get<float>("obstacle_diameter_m", 0.6f) * 0.5f;
            obstacles_.push_back({{x, y}, radius_m, now_ms, 1, persistent});
            return true;
        }

        void Map::clearObstacles()
        {
            obstacles_.clear();
        }

        void Map::clearAutoObstacles()
        {
            obstacles_.erase(
                std::remove_if(obstacles_.begin(), obstacles_.end(),
                               [](const OnTheFlyObstacle &o)
                               { return !o.persistent; }),
                obstacles_.end());
        }

        void Map::cleanupExpiredObstacles(unsigned now_ms, unsigned expiry_ms)
        {
            obstacles_.erase(
                std::remove_if(obstacles_.begin(), obstacles_.end(),
                               [&](const OnTheFlyObstacle &o)
                               {
                                   return !o.persistent && (now_ms - o.detectedAt_ms) >= expiry_ms;
                               }),
                obstacles_.end());
        }

        // ── Boundary queries ───────────────────────────────────────────────────────────

        bool Map::isInsideAllowedArea(float x, float y) const
        {
            if (perimeterPoints_.empty())
                return true; // no perimeter loaded → allow all
            if (!pointInPolygon(perimeterPoints_, x, y))
                return false;
            for (std::size_t exclusionIndex = 0; exclusionIndex < exclusions_.size(); ++exclusionIndex)
            {
                const bool isHard = exclusionIndex >= exclusionMeta_.size() ||
                                    exclusionMeta_[exclusionIndex].type == ExclusionType::HARD;
                if (isHard && pointInPolygon(exclusions_[exclusionIndex], x, y))
                    return false;
            }
            return true;
        }

        float Map::routeClearanceForPoint(const Point &point, WayType mode) const
        {
            (void)point;
            (void)mode;
            return std::max(0.05f, planner_.defaultClearance_m);
        }

        bool Map::routeReverseAllowedForPoint(const Point &point, WayType mode) const
        {
            (void)point;
            if (mode == WayType::DOCK)
            {
                return dockMeta_.approachMode == DockApproachMode::REVERSE_ALLOWED;
            }
            return false;
        }

        std::string Map::zoneIdForPoint(float x, float y,
                                        const std::vector<std::string> &preferredOrder) const
        {
            auto findPreferred = [this, x, y](const std::string &zoneId) -> std::string
            {
                for (const auto &zone : zones_)
                {
                    if (zone.id == zoneId && pointInPolygon(zone.polygon, x, y))
                    {
                        return zone.id;
                    }
                }
                return {};
            };

            for (const auto &zoneId : preferredOrder)
            {
                const std::string match = findPreferred(zoneId);
                if (!match.empty())
                    return match;
            }

            for (const auto &zone : zones_)
            {
                if (pointInPolygon(zone.polygon, x, y))
                    return zone.id;
            }
            return {};
        }

        // ── Geometry helpers ───────────────────────────────────────────────────────────

        float Map::scalePI(float v)
        {
            float res = std::fmod(v, 2.0f * static_cast<float>(M_PI));
            if (res > static_cast<float>(M_PI))
                res -= 2.0f * static_cast<float>(M_PI);
            if (res < -static_cast<float>(M_PI))
                res += 2.0f * static_cast<float>(M_PI);
            return res;
        }

        float Map::distancePI(float x, float w)
        {
            float d = scalePI(x - w);
            if (d < -static_cast<float>(M_PI))
                d += 2.0f * static_cast<float>(M_PI);
            if (d > static_cast<float>(M_PI))
                d -= 2.0f * static_cast<float>(M_PI);
            return d;
        }

        float Map::scalePIangles(float setAngle, float currAngle)
        {
            return scalePI(setAngle) + std::round((currAngle - scalePI(setAngle)) / (2.0f * static_cast<float>(M_PI))) * 2.0f * static_cast<float>(M_PI);
        }

        float Map::pointsAngle(float x1, float y1, float x2, float y2)
        {
            return std::atan2(y2 - y1, x2 - x1);
        }

        bool Map::pointInPolygon(const PolygonPoints &poly, float px, float py)
        {
            // Ray-casting algorithm
            if (poly.size() < 3)
                return false;
            bool inside = false;
            std::size_t n = poly.size();
            for (std::size_t i = 0, j = n - 1; i < n; j = i++)
            {
                float xi = poly[i].x, yi = poly[i].y;
                float xj = poly[j].x, yj = poly[j].y;
                bool intersect = ((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
                if (intersect)
                    inside = !inside;
            }
            return inside;
        }

        float Map::polygonArea(const PolygonPoints &poly)
        {
            float area = 0.0f;
            std::size_t n = poly.size();
            for (std::size_t i = 0, j = n - 1; i < n; j = i++)
            {
                area += (poly[j].x + poly[i].x) * (poly[j].y - poly[i].y);
            }
            return std::fabs(area) * 0.5f;
        }

        bool Map::lineIntersects(const Point &p0, const Point &p1,
                                 const Point &p2, const Point &p3) const
        {
            float d1x = p1.x - p0.x, d1y = p1.y - p0.y;
            float d2x = p3.x - p2.x, d2y = p3.y - p2.y;
            float cross = d1x * d2y - d1y * d2x;
            if (std::fabs(cross) < 1e-10f)
                return false;
            float t = ((p2.x - p0.x) * d2y - (p2.y - p0.y) * d2x) / cross;
            float u = ((p2.x - p0.x) * d1y - (p2.y - p0.y) * d1x) / cross;
            return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
        }

        bool Map::linePolygonIntersection(const Point &src, const Point &dst,
                                          const PolygonPoints &poly) const
        {
            std::size_t n = poly.size();
            for (std::size_t i = 0, j = n - 1; i < n; j = i++)
            {
                if (lineIntersects(src, dst, poly[i], poly[j]))
                    return true;
            }
            return false;
        }

        bool Map::segmentAllowed(const Point &src, const Point &dst) const
        {
            if (!isInsideAllowedArea(src.x, src.y) || !isInsideAllowedArea(dst.x, dst.y))
                return false;

            if (!perimeterPoints_.empty() && linePolygonIntersection(src, dst, perimeterPoints_))
            {
                const Point mid{(src.x + dst.x) * 0.5f, (src.y + dst.y) * 0.5f};
                if (!isInsideAllowedArea(mid.x, mid.y))
                    return false;
            }

            for (std::size_t exclusionIndex = 0; exclusionIndex < exclusions_.size(); ++exclusionIndex)
            {
                const bool isHard = exclusionIndex >= exclusionMeta_.size() ||
                                    exclusionMeta_[exclusionIndex].type == ExclusionType::HARD;
                if (isHard && linePolygonIntersection(src, dst, exclusions_[exclusionIndex]))
                    return false;
            }

            for (const auto &obs : obstacles_)
            {
                const float radius = obs.radius_m + planner_.obstacleInflation_m;
                if (lineCircleIntersects(src, dst, obs.center, radius))
                    return false;
            }

            return true;
        }

        bool Map::pathAllowed(const std::vector<Point> &path) const
        {
            if (path.size() < 2)
                return false;
            for (size_t i = 0; i + 1 < path.size(); ++i)
            {
                if (!segmentAllowed(path[i], path[i + 1]))
                    return false;
            }
            return true;
        }

        Point Map::dockApproachTarget() const
        {
            if (dockMeta_.corridor.empty())
                return dockRoute_.points.front().p;

            Point center;
            for (const auto &p : dockMeta_.corridor)
            {
                center.x += p.x;
                center.y += p.y;
            }
            const float denom = static_cast<float>(dockMeta_.corridor.size());
            center.x /= denom;
            center.y /= denom;
            return center;
        }

        RoutePlan Map::previewPath(const Point &src,
                                   const Point &dst,
                                   WayType missionMode,
                                   WayType planningMode,
                                   float headingReferenceRad,
                                   bool hasHeadingReference,
                                   bool reverseAllowed,
                                   float clearance_m,
                                   float robotRadius_m,
                                   const PolygonPoints &constraintZone) const
        {
            PlannerContext context;
            context.robotPose = src;
            context.source = src;
            context.destination = dst;
            context.missionMode = missionMode;
            context.planningMode = planningMode;
            context.headingReferenceRad = headingReferenceRad;
            context.hasHeadingReference = hasHeadingReference;
            context.reverseAllowed = reverseAllowed;
            context.clearance_m = clearance_m;
            context.robotRadius_m = (robotRadius_m > 0.0f) ? robotRadius_m : (clearance_m + planner_.obstacleInflation_m);
            context.constraintZone = constraintZone;
            return PathPlanner::planPath(*this, context);
        }

    } // namespace nav
} // namespace sunray
