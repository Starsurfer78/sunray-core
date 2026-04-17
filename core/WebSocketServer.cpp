// WebSocketServer.cpp — Crow WebSocket + HTTP server implementation.
//
// Crow is included only here (pimpl) — not in WebSocketServer.h.
// This keeps Crow out of all headers that include WebSocketServer.h.

#include "core/WebSocketServer.h"

#include "crow.h"
#include "core/navigation/MissionCompiler.h"
#include "core/navigation/RouteValidator.h"
#include "core/navigation/Map.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <sstream>
#include <iomanip>

namespace sunray
{

    bool WebSocketServer::isHttpAuthorizedForToken(const std::string &apiToken,
                                                   const std::string &xApiToken,
                                                   const std::string &authorizationHeader)
    {
        if (apiToken.empty())
            return true;
        if (!xApiToken.empty() && xApiToken == apiToken)
            return true;
        return authorizationHeader == ("Bearer " + apiToken);
    }

    bool WebSocketServer::isWsCommandAuthorizedForToken(const std::string &apiToken,
                                                        const nlohmann::json &payload)
    {
        if (apiToken.empty())
            return true;
        return payload.value("token", std::string()) == apiToken;
    }

    // ── Pimpl ─────────────────────────────────────────────────────────────────────

    struct WebSocketServer::Impl
    {
        crow::SimpleApp app;

        std::unordered_set<crow::websocket::connection *> connections;
        std::mutex connMutex;
    };

    // ── Static helpers ─────────────────────────────────────────────────────────────

    /// Return MIME type for a file extension.
    static std::string mimeType(const std::string &ext)
    {
        static const std::unordered_map<std::string, std::string> map{
            {".html", "text/html; charset=utf-8"},
            {".js", "application/javascript"},
            {".css", "text/css"},
            {".svg", "image/svg+xml"},
            {".ico", "image/x-icon"},
            {".png", "image/png"},
            {".json", "application/json"},
        };
        auto it = map.find(ext);
        return it != map.end() ? it->second : "application/octet-stream";
    }

    /// Read a file from disk and return its contents; empty string on failure.
    static std::string readFile(const std::filesystem::path &p)
    {
        std::ifstream f(p, std::ios::binary);
        if (!f)
            return {};
        return {std::istreambuf_iterator<char>(f), {}};
    }

    static bool jsonPointToNavPoint(const nlohmann::json &value, sunray::nav::Point &out)
    {
        if (!value.is_array() || value.size() < 2)
            return false;
        if (!value[0].is_number() || !value[1].is_number())
            return false;
        out.x = value[0].get<float>();
        out.y = value[1].get<float>();
        return true;
    }

    static sunray::nav::WayType parseWayType(const std::string &raw, sunray::nav::WayType fallback)
    {
        if (raw == "dock")
            return sunray::nav::WayType::DOCK;
        if (raw == "mow")
            return sunray::nav::WayType::MOW;
        if (raw == "perimeter")
            return sunray::nav::WayType::PERIMETER;
        if (raw == "exclusion")
            return sunray::nav::WayType::EXCLUSION;
        if (raw == "free")
            return sunray::nav::WayType::FREE;
        return fallback;
    }

    static std::string encodeWayType(sunray::nav::WayType mode)
    {
        switch (mode)
        {
        case sunray::nav::WayType::PERIMETER:
            return "perimeter";
        case sunray::nav::WayType::EXCLUSION:
            return "exclusion";
        case sunray::nav::WayType::DOCK:
            return "dock";
        case sunray::nav::WayType::MOW:
            return "mow";
        case sunray::nav::WayType::FREE:
            return "free";
        }
        return "free";
    }

    static std::string encodeRouteSemantic(sunray::nav::RouteSemantic sem)
    {
        switch (sem)
        {
        case sunray::nav::RouteSemantic::COVERAGE_EDGE:
            return "coverage_edge";
        case sunray::nav::RouteSemantic::COVERAGE_INFILL:
            return "coverage_infill";
        case sunray::nav::RouteSemantic::TRANSIT_WITHIN_ZONE:
            return "transit_within_zone";
        case sunray::nav::RouteSemantic::TRANSIT_BETWEEN_COMPONENTS:
            return "transit_between_components";
        case sunray::nav::RouteSemantic::TRANSIT_INTER_ZONE:
            return "transit_inter_zone";
        case sunray::nav::RouteSemantic::DOCK_APPROACH:
            return "dock_approach";
        case sunray::nav::RouteSemantic::RECOVERY:
            return "recovery";
        case sunray::nav::RouteSemantic::UNKNOWN:
            return "unknown";
        }
        return "unknown";
    }

    static nlohmann::json encodeRoutePlanJson(const sunray::nav::RoutePlan &route)
    {
        nlohmann::json out;
        out["active"] = route.active;
        out["valid"] = route.valid;
        out["invalidReason"] = route.invalidReason;
        out["sourceMode"] = encodeWayType(route.sourceMode);
        out["points"] = nlohmann::json::array();
        for (const auto &point : route.points)
        {
            out["points"].push_back({
                {"p", {point.p.x, point.p.y}},
                {"reverse", point.reverse},
                {"slow", point.slow},
                {"reverseAllowed", point.reverseAllowed},
                {"clearance_m", point.clearance_m},
                {"sourceMode", encodeWayType(point.sourceMode)},
                {"semantic", encodeRouteSemantic(point.semantic)},
                {"zoneId", point.zoneId},
                {"componentId", point.componentId},
            });
        }
        return out;
    }

    static nlohmann::json encodeRouteDebugJson(const sunray::nav::RoutePlan &route)
    {
        nlohmann::json semanticCounts = nlohmann::json::object();
        nlohmann::json zoneOrder = nlohmann::json::array();
        nlohmann::json componentOrder = nlohmann::json::array();
        std::unordered_set<std::string> seenZones;
        std::unordered_set<std::string> seenComponents;

        for (const auto &point : route.points)
        {
            const std::string semantic = encodeRouteSemantic(point.semantic);
            semanticCounts[semantic] = semanticCounts.value(semantic, 0) + 1;

            if (!point.zoneId.empty() && seenZones.insert(point.zoneId).second)
                zoneOrder.push_back(point.zoneId);

            if (!point.componentId.empty() && seenComponents.insert(point.componentId).second)
            {
                componentOrder.push_back({
                    {"componentId", point.componentId},
                    {"zoneId", point.zoneId},
                    {"firstSemantic", semantic},
                });
            }
        }

        return {
            {"pointCount", static_cast<int>(route.points.size())},
            {"active", route.active},
            {"valid", route.valid},
            {"invalidReason", route.invalidReason},
            {"semanticCounts", semanticCounts},
            {"zoneOrder", zoneOrder},
            {"componentOrder", componentOrder},
        };
    }

    static nlohmann::json encodeMissionPlanJson(const sunray::nav::MissionPlan &plan)
    {
        nlohmann::json zonePlans = nlohmann::json::array();
        for (const auto &zonePlan : plan.zonePlans)
        {
            zonePlans.push_back({
                {"zoneId", zonePlan.zoneId},
                {"zoneName", zonePlan.zoneName},
                {"valid", zonePlan.valid},
                {"invalidReason", zonePlan.invalidReason},
            });
        }

        return {
            {"missionId", plan.missionId},
            {"zoneOrder", plan.zoneOrder},
            {"valid", plan.valid},
            {"invalidReason", plan.invalidReason},
            {"zonePlans", zonePlans},
            {"planRef", {
                            {"id", plan.missionId.empty() ? std::string("preview-mission") : plan.missionId},
                            {"revision", static_cast<int>(plan.route.points.size())},
                        }},
        };
    }

    static constexpr const char *kDefaultMapJson =
        R"({"perimeter":[],"dock":[],"exclusions":[],"zones":[]})";

    static constexpr const char *kDefaultMissionsJson = R"([])";

    static std::filesystem::path stmUploadDir()
    {
        return "/var/lib/sunray-core/stm-upload";
    }

    static std::filesystem::path stmUploadedBinPath()
    {
        return stmUploadDir() / "rm18-upload.bin";
    }

    static std::filesystem::path stmUploadedMetaPath()
    {
        return stmUploadDir() / "rm18-upload.json";
    }

    static long long unixNowMs()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    static std::string sanitizeUploadName(const std::string &rawName)
    {
        if (rawName.empty())
            return "rm18-upload.bin";

        std::string cleaned;
        cleaned.reserve(rawName.size());
        for (char ch : rawName)
        {
            const bool safe =
                (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-';
            cleaned.push_back(safe ? ch : '_');
        }
        return cleaned;
    }

    static std::string slugifyMapId(const std::string &rawName)
    {
        std::string out;
        out.reserve(rawName.size());
        bool prevDash = false;
        for (char ch : rawName)
        {
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (std::isalnum(uch))
            {
                out.push_back(static_cast<char>(std::tolower(uch)));
                prevDash = false;
                continue;
            }
            if (!prevDash)
            {
                out.push_back('-');
                prevDash = true;
            }
        }
        while (!out.empty() && out.front() == '-')
            out.erase(out.begin());
        while (!out.empty() && out.back() == '-')
            out.pop_back();
        if (out.empty())
            out = "map";
        return out;
    }

    static nlohmann::json readStmUploadMeta()
    {
        const auto metaPath = stmUploadedMetaPath();
        if (!std::filesystem::exists(metaPath))
            return nlohmann::json::object();
        try
        {
            return nlohmann::json::parse(readFile(metaPath));
        }
        catch (...)
        {
            return nlohmann::json::object();
        }
    }

    static nlohmann::json buildStmUploadInfoJson()
    {
        const auto binPath = stmUploadedBinPath();
        nlohmann::json j = {
            {"ok", true},
            {"exists", std::filesystem::exists(binPath)},
        };
        if (!std::filesystem::exists(binPath))
            return j;

        const auto meta = readStmUploadMeta();
        j["size_bytes"] = static_cast<long long>(std::filesystem::file_size(binPath));
        j["stored_path"] = binPath.string();
        j["original_name"] = meta.value("original_name", std::string("rm18-upload.bin"));
        j["uploaded_at_ms"] = meta.value("uploaded_at_ms", 0LL);
        return j;
    }

    /// Serve a file from webRoot, guarding against path traversal.
    /// Returns 200+body on success, 404 on missing, 403 on traversal attempt.
    static crow::response serveStatic(const std::filesystem::path &webRoot,
                                      const std::string &relPath)
    {
        namespace fs = std::filesystem;
        const fs::path base = fs::weakly_canonical(webRoot);
        const fs::path target = fs::weakly_canonical(base / relPath);

        // Path traversal guard: resolved path must stay inside webRoot
        const auto [baseEnd, _] = std::mismatch(base.begin(), base.end(),
                                                target.begin(), target.end());
        if (baseEnd != base.end())
        {
            return crow::response(403);
        }

        std::string body = readFile(target);
        if (body.empty() && !fs::exists(target))
        {
            return crow::response(404);
        }

        const std::string ext = target.extension().string();
        crow::response res(200, body);
        res.set_header("Content-Type", mimeType(ext));
        return res;
    }

    // ── Construction / destruction ────────────────────────────────────────────────

    WebSocketServer::WebSocketServer(std::shared_ptr<Config> config,
                                     std::shared_ptr<Logger> logger)
        : config_(std::move(config)), logger_(std::move(logger)), impl_(std::make_unique<Impl>())
    {
    }

    WebSocketServer::~WebSocketServer()
    {
        stop();
    }

    // ── Configuration ─────────────────────────────────────────────────────────────

    void WebSocketServer::setWebRoot(const std::string &distDir)
    {
        webRoot_ = distDir;
    }

    // ── Command registration ───────────────────────────────────────────────────────

    void WebSocketServer::onCommand(CommandCallback cb)
    {
        std::lock_guard<std::mutex> lk(cmdMutex_);
        commandCallback_ = std::move(cb);
    }

    void WebSocketServer::onSimCommand(SimCommandCallback cb)
    {
        std::lock_guard<std::mutex> lk(simCmdMutex_);
        simCommandCallback_ = std::move(cb);
    }

    void WebSocketServer::onDiag(DiagCallback cb)
    {
        std::lock_guard<std::mutex> lk(diagCbMutex_);
        diagCallback_ = std::move(cb);
    }

    void WebSocketServer::onScheduleGet(ScheduleGetCallback cb)
    {
        std::lock_guard<std::mutex> lk(schedCbMutex_);
        schedGetCb_ = std::move(cb);
    }

    void WebSocketServer::onSchedulePut(SchedulePutCallback cb)
    {
        std::lock_guard<std::mutex> lk(schedCbMutex_);
        schedPutCb_ = std::move(cb);
    }

    void WebSocketServer::onHistoryEventsGet(HistoryGetCallback cb)
    {
        std::lock_guard<std::mutex> lk(historyCbMutex_);
        historyEventsGetCb_ = std::move(cb);
    }

    void WebSocketServer::onHistorySessionsGet(HistoryGetCallback cb)
    {
        std::lock_guard<std::mutex> lk(historyCbMutex_);
        historySessionsGetCb_ = std::move(cb);
    }

    void WebSocketServer::onHistoryClear(HistoryClearCallback cb)
    {
        std::lock_guard<std::mutex> lk(historyCbMutex_);
        historyClearCb_ = std::move(cb);
    }

    void WebSocketServer::onStatisticsSummaryGet(StatisticsGetCallback cb)
    {
        std::lock_guard<std::mutex> lk(historyCbMutex_);
        statisticsSummaryGetCb_ = std::move(cb);
    }

    void WebSocketServer::setMapPath(const std::string &mapPath)
    {
        mapPath_ = mapPath;
    }

    void WebSocketServer::onMapGet(MapGetCallback cb)
    {
        std::lock_guard<std::mutex> lk(mapReloadMutex_);
        mapGetCallback_ = std::move(cb);
    }

    void WebSocketServer::onMapReload(MapReloadCallback cb)
    {
        std::lock_guard<std::mutex> lk(mapReloadMutex_);
        mapReloadCallback_ = std::move(cb);
    }

    void WebSocketServer::onPlanPreview(PlanPreviewCallback cb)
    {
        std::lock_guard<std::mutex> lk(cmdMutex_);
        planPreviewCallback_ = std::move(cb);
    }

    void WebSocketServer::setMissionPath(const std::string &missionPath)
    {
        missionPath_ = missionPath;
    }

    void WebSocketServer::setOtaScriptPath(const std::string &path)
    {
        otaScriptPath_ = path;
    }

    void WebSocketServer::setStmFlashScriptPath(const std::string &path)
    {
        stmFlashScriptPath_ = path;
    }

    void WebSocketServer::onStmFlashStateChange(StmFlashStateCallback cb)
    {
        std::lock_guard<std::mutex> lk(stmFlashStateMutex_);
        stmFlashStateCb_ = std::move(cb);
    }

    std::filesystem::path WebSocketServer::mapsDir() const
    {
        if (mapPath_.empty())
            return {};
        return std::filesystem::path(mapPath_).parent_path() / "maps";
    }

    std::filesystem::path WebSocketServer::mapsCatalogPath() const
    {
        const auto dir = mapsDir();
        if (dir.empty())
            return {};
        return dir / "catalog.json";
    }

    nlohmann::json WebSocketServer::loadMapsCatalogLocked() const
    {
        const auto catalogPath = mapsCatalogPath();
        if (catalogPath.empty() || !std::filesystem::exists(catalogPath))
        {
            return nlohmann::json::object();
        }
        try
        {
            return nlohmann::json::parse(readFile(catalogPath));
        }
        catch (...)
        {
            return nlohmann::json::object();
        }
    }

    bool WebSocketServer::saveMapsCatalogLocked(const nlohmann::json &catalog, std::string *err) const
    {
        try
        {
            const auto catalogPath = mapsCatalogPath();
            if (catalogPath.empty())
            {
                if (err)
                    *err = "map path not configured";
                return false;
            }
            std::filesystem::create_directories(catalogPath.parent_path());
            std::ofstream out(catalogPath);
            if (!out.is_open())
            {
                if (err)
                    *err = "cannot write map catalog";
                return false;
            }
            out << catalog.dump(2);
            if (!out.good())
            {
                if (err)
                    *err = "failed to flush map catalog";
                return false;
            }
            return true;
        }
        catch (const std::exception &e)
        {
            if (err)
                *err = e.what();
            return false;
        }
    }

    bool WebSocketServer::ensureMapsCatalogLocked(std::string *err)
    {
        try
        {
            if (mapPath_.empty())
            {
                if (err)
                    *err = "map path not configured";
                return false;
            }

            auto catalog = loadMapsCatalogLocked();
            if (!catalog.is_object())
                catalog = nlohmann::json::object();
            if (!catalog.contains("maps") || !catalog["maps"].is_array())
            {
                catalog["maps"] = nlohmann::json::array();
            }

            auto &maps = catalog["maps"];
            if (maps.empty())
            {
                std::filesystem::create_directories(mapsDir());

                std::string raw = readFile(mapPath_);
                if (raw.empty())
                    raw = kDefaultMapJson;
                nlohmann::json parsed = nlohmann::json::parse(raw, nullptr, false);
                if (!parsed.is_object())
                    parsed = nlohmann::json::parse(kDefaultMapJson);

                const std::string id = "default";
                const std::string file = "default.json";
                const auto mapFilePath = mapsDir() / file;
                {
                    std::ofstream out(mapFilePath);
                    if (!out.is_open())
                    {
                        if (err)
                            *err = "cannot write initial map entry";
                        return false;
                    }
                    out << parsed.dump(2);
                }
                const long long now = unixNowMs();
                maps.push_back({
                    {"id", id},
                    {"name", "Standardkarte"},
                    {"file", file},
                    {"created_at_ms", now},
                    {"updated_at_ms", now},
                });
                catalog["active_id"] = id;
            }

            std::string activeId = catalog.value("active_id", std::string());
            if (activeId.empty())
            {
                activeId = maps.front().value("id", std::string("default"));
                catalog["active_id"] = activeId;
            }

            bool activeFound = false;
            for (const auto &entry : maps)
            {
                if (entry.value("id", std::string()) == activeId)
                {
                    activeFound = true;
                    break;
                }
            }
            if (!activeFound)
            {
                catalog["active_id"] = maps.front().value("id", std::string("default"));
            }

            return saveMapsCatalogLocked(catalog, err);
        }
        catch (const std::exception &e)
        {
            if (err)
                *err = e.what();
            return false;
        }
    }

    bool WebSocketServer::syncActiveMapSnapshotLocked(const nlohmann::json &mapDoc, std::string *err)
    {
        if (!ensureMapsCatalogLocked(err))
            return false;

        auto catalog = loadMapsCatalogLocked();
        auto &maps = catalog["maps"];
        const std::string activeId = catalog.value("active_id", std::string());
        for (auto &entry : maps)
        {
            if (entry.value("id", std::string()) != activeId)
                continue;
            const auto file = entry.value("file", std::string());
            if (file.empty())
                break;
            try
            {
                std::filesystem::create_directories(mapsDir());
                std::ofstream out(mapsDir() / file);
                if (!out.is_open())
                {
                    if (err)
                        *err = "cannot write active map snapshot";
                    return false;
                }
                out << mapDoc.dump(2);
                if (!out.good())
                {
                    if (err)
                        *err = "failed to flush active map snapshot";
                    return false;
                }
                entry["updated_at_ms"] = unixNowMs();
                return saveMapsCatalogLocked(catalog, err);
            }
            catch (const std::exception &e)
            {
                if (err)
                    *err = e.what();
                return false;
            }
        }
        if (err)
            *err = "active map entry not found";
        return false;
    }

    bool WebSocketServer::activateMapByIdLocked(const std::string &mapId, std::string *err)
    {
        if (!ensureMapsCatalogLocked(err))
            return false;

        auto catalog = loadMapsCatalogLocked();
        auto &maps = catalog["maps"];
        nlohmann::json *selected = nullptr;
        for (auto &entry : maps)
        {
            if (entry.value("id", std::string()) == mapId)
            {
                selected = &entry;
                break;
            }
        }
        if (!selected)
        {
            if (err)
                *err = "map not found";
            return false;
        }

        const std::string file = selected->value("file", std::string());
        if (file.empty())
        {
            if (err)
                *err = "map file missing";
            return false;
        }
        const auto srcPath = mapsDir() / file;
        const std::string raw = readFile(srcPath);
        if (raw.empty())
        {
            if (err)
                *err = "map file unreadable";
            return false;
        }

        {
            std::ofstream dst(mapPath_);
            if (!dst.is_open())
            {
                if (err)
                    *err = "cannot write active map file";
                return false;
            }
            dst << raw;
            if (!dst.good())
            {
                if (err)
                    *err = "failed to flush active map file";
                return false;
            }
        }

        catalog["active_id"] = mapId;
        if (!saveMapsCatalogLocked(catalog, err))
            return false;

        bool reloadOk = true;
        {
            std::lock_guard<std::mutex> lk(mapReloadMutex_);
            if (mapReloadCallback_)
                reloadOk = mapReloadCallback_();
        }
        if (!reloadOk)
        {
            if (err)
                *err = "reload failed";
            return false;
        }
        return true;
    }

    // ── HTTP route setup ──────────────────────────────────────────────────────────

    void WebSocketServer::setupHttpRoutes()
    {
        auto &app = impl_->app;
        const std::string apiToken = config_ ? config_->get<std::string>("api_token", "") : "";
        auto parseLimit = [](const crow::request &req, unsigned fallback) -> unsigned
        {
            const char *raw = req.url_params.get("limit");
            if (raw == nullptr || *raw == '\0')
                return fallback;
            try
            {
                const unsigned parsed = static_cast<unsigned>(std::stoul(raw));
                return parsed == 0 ? fallback : parsed;
            }
            catch (...)
            {
                return fallback;
            }
        };
        auto isAuthorized = [apiToken](const crow::request &req) -> bool
        {
            return isHttpAuthorizedForToken(apiToken,
                                            req.get_header_value("X-Api-Token"),
                                            req.get_header_value("Authorization"));
        };

        // ── Static file serving ────────────────────────────────────────────────────
        if (!webRoot_.empty())
        {
            const std::string root = webRoot_;

            // GET / → index.html
            CROW_ROUTE(app, "/")([root]()
                                 { return serveStatic(root, "index.html"); });

            // GET /assets/<path> → webRoot/assets/<path>
            CROW_ROUTE(app, "/assets/<path>")([root](const std::string &path)
                                              { return serveStatic(root, "assets/" + path); });
        }

        // ── GET /api/version ───────────────────────────────────────────────────────
        CROW_ROUTE(app, "/api/version")(
            [this]() -> crow::response
            {
                nlohmann::json j;
                j["pi"] = SUNRAY_VERSION;
                // Get latest MCU version from telemetry snapshot
                {
                    std::lock_guard<std::mutex> lk(telemetryMutex_);
                    j["mcu"] = latestTelemetry_.mcu_version;
                }
                const std::string body = j.dump();
                crow::response res(200, body);
                res.set_header("Content-Type", "application/json");
                res.set_header("Access-Control-Allow-Origin", "*");
                return res;
            });

        // ── GET /api/config ────────────────────────────────────────────────────────
        CROW_ROUTE(app, "/api/config")(
            [this, isAuthorized](const crow::request &req) -> crow::response
            {
                if (!isAuthorized(req))
                    return crow::response(401, R"({"error":"unauthorized"})");
                if (!config_)
                    return crow::response(503);
                try
                {
                    const std::string body = config_->dump();
                    crow::response res;
                    res.code = 200;
                    res.body = body;
                    res.set_header("Content-Type", "application/json");
                    return res;
                }
                catch (const std::exception &e)
                {
                    logger_->warn(TAG, std::string("GET /api/config error: ") + e.what());
                    return crow::response(500, R"({"error":"config dump failed"})");
                }
            });

        // ── PUT /api/config ────────────────────────────────────────────────────────
        CROW_ROUTE(app, "/api/config").methods(crow::HTTPMethod::PUT)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                      {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (!config_) return crow::response(503);
            try {
                auto body = nlohmann::json::parse(req.body);
                if (!body.is_object()) return crow::response(400);
                for (auto& [key, val] : body.items()) {
                    if (val.is_boolean())      config_->set(key, val.get<bool>());
                    else if (val.is_number_integer()) config_->set(key, val.get<int>());
                    else if (val.is_number())  config_->set(key, val.get<double>());
                    else if (val.is_string())  config_->set(key, val.get<std::string>());
                    // skip arrays/objects — not supported in flat config
                }
                config_->save();
                logger_->info(TAG, "Config updated via REST");
                return crow::response(200, R"({"ok":true})");
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("PUT /api/config error: ") + e.what());
                return crow::response(400);
            } });

        // ── GET /api/map ───────────────────────────────────────────────────────────
        CROW_ROUTE(app, "/api/map")(
            [this, isAuthorized](const crow::request &req) -> crow::response
            {
                try
                {
                    if (!isAuthorized(req))
                        return crow::response(401, R"({"error":"unauthorized"})");
                    nlohmann::json mapJson;
                    const std::string raw = mapPath_.empty() ? std::string() : readFile(mapPath_);
                    if (!raw.empty())
                    {
                        mapJson = nlohmann::json::parse(raw, nullptr, false);
                    }
                    if (mapJson.is_discarded() || mapJson.is_null() || !mapJson.is_object())
                    {
                        std::lock_guard<std::mutex> lk(mapReloadMutex_);
                        if (mapGetCallback_)
                        {
                            mapJson = mapGetCallback_();
                        }
                    }
                    if (mapJson.is_null() || mapJson.is_discarded() || !mapJson.is_object())
                    {
                        mapJson = nlohmann::json::parse(kDefaultMapJson);
                    }
                    crow::response res(200, mapJson.dump());
                    res.set_header("Content-Type", "application/json");
                    res.set_header("Access-Control-Allow-Origin", "*");
                    return res;
                }
                catch (const std::exception &e)
                {
                    logger_->error(TAG, std::string("GET /api/map failed for path '") + mapPath_ + "': " + e.what());
                    return crow::response(500, R"({"error":"map read failed"})");
                }
                catch (...)
                {
                    logger_->error(TAG, std::string("GET /api/map failed for path '") + mapPath_ + "': unknown error");
                    return crow::response(500, R"({"error":"map read failed"})");
                }
            });

        // ── GET /api/missions ──────────────────────────────────────────────────────
        CROW_ROUTE(app, "/api/missions")(
            [this, isAuthorized](const crow::request &req) -> crow::response
            {
                if (!isAuthorized(req))
                    return crow::response(401, R"({"error":"unauthorized"})");
                try
                {
                    const std::string raw = missionPath_.empty() ? std::string() : readFile(missionPath_);
                    const std::string body = raw.empty() ? std::string(kDefaultMissionsJson) : raw;
                    auto parsed = nlohmann::json::parse(body);
                    if (!parsed.is_array())
                        parsed = nlohmann::json::array();
                    crow::response res(200, parsed.dump());
                    res.set_header("Content-Type", "application/json");
                    res.set_header("Access-Control-Allow-Origin", "*");
                    return res;
                }
                catch (const std::exception &e)
                {
                    logger_->error(TAG, std::string("GET /api/missions failed for path '") + missionPath_ + "': " + e.what());
                    return crow::response(500, R"({"error":"missions read failed"})");
                }
                catch (...)
                {
                    logger_->error(TAG, std::string("GET /api/missions failed for path '") + missionPath_ + "': unknown error");
                    return crow::response(500, R"({"error":"missions read failed"})");
                }
            });

        // ── POST /api/missions ─────────────────────────────────────────────────────
        CROW_ROUTE(app, "/api/missions").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                         {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (missionPath_.empty()) return crow::response(503);
            try {
                auto incoming = nlohmann::json::parse(req.body);
                if (!incoming.is_object()) return crow::response(400, R"({"error":"expected mission object"})");

                nlohmann::json missions = nlohmann::json::array();
                const std::string raw = readFile(missionPath_);
                if (!raw.empty()) {
                    missions = nlohmann::json::parse(raw);
                    if (!missions.is_array()) missions = nlohmann::json::array();
                }

                const std::string missionId = incoming.value("id", std::string());
                if (missionId.empty()) return crow::response(400, R"({"error":"mission id required"})");
                for (const auto& mission : missions) {
                    if (mission.value("id", std::string()) == missionId) {
                        return crow::response(409, R"({"error":"mission already exists"})");
                    }
                }
                missions.push_back(incoming);

                std::filesystem::create_directories(std::filesystem::path(missionPath_).parent_path());
                std::ofstream f(missionPath_);
                if (!f.is_open()) return crow::response(500, R"({"error":"cannot write missions"})");
                f << missions.dump(2);
                logger_->info(TAG, "Mission created via REST: " + missionId);
                return crow::response(200, R"({"ok":true})");
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("POST /api/missions error: ") + e.what());
                return crow::response(400, R"({"error":"bad mission payload"})");
            } });

        // ── PUT /api/missions/<id> ────────────────────────────────────────────────
        CROW_ROUTE(app, "/api/missions/<string>").methods(crow::HTTPMethod::PUT)([this, isAuthorized](const crow::request &req, const std::string &missionId) -> crow::response
                                                                                 {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (missionPath_.empty()) return crow::response(503);
            try {
                auto incoming = nlohmann::json::parse(req.body);
                if (!incoming.is_object()) return crow::response(400, R"({"error":"expected mission object"})");
                incoming["id"] = missionId;

                nlohmann::json missions = nlohmann::json::array();
                const std::string raw = readFile(missionPath_);
                if (!raw.empty()) {
                    missions = nlohmann::json::parse(raw);
                    if (!missions.is_array()) missions = nlohmann::json::array();
                }

                bool updated = false;
                for (auto& mission : missions) {
                    if (mission.value("id", std::string()) == missionId) {
                        mission = incoming;
                        updated = true;
                        break;
                    }
                }
                if (!updated) missions.push_back(incoming);

                std::filesystem::create_directories(std::filesystem::path(missionPath_).parent_path());
                std::ofstream f(missionPath_);
                if (!f.is_open()) return crow::response(500, R"({"error":"cannot write missions"})");
                f << missions.dump(2);
                logger_->info(TAG, "Mission updated via REST: " + missionId);
                return crow::response(200, R"({"ok":true})");
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("PUT /api/missions error: ") + e.what());
                return crow::response(400, R"({"error":"bad mission payload"})");
            } });

        // ── DELETE /api/missions/<id> ─────────────────────────────────────────────
        CROW_ROUTE(app, "/api/missions/<string>").methods(crow::HTTPMethod::DELETE)([this, isAuthorized](const crow::request &req, const std::string &missionId) -> crow::response
                                                                                    {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (missionPath_.empty()) return crow::response(503);
            try {
                nlohmann::json missions = nlohmann::json::array();
                const std::string raw = readFile(missionPath_);
                if (!raw.empty()) {
                    missions = nlohmann::json::parse(raw);
                    if (!missions.is_array()) missions = nlohmann::json::array();
                }

                const auto originalSize = missions.size();
                missions.erase(
                    std::remove_if(missions.begin(), missions.end(),
                        [&missionId](const nlohmann::json& mission) {
                            return mission.value("id", std::string()) == missionId;
                        }),
                    missions.end());

                std::filesystem::create_directories(std::filesystem::path(missionPath_).parent_path());
                std::ofstream f(missionPath_);
                if (!f.is_open()) return crow::response(500, R"({"error":"cannot write missions"})");
                f << missions.dump(2);
                logger_->info(TAG, "Mission deleted via REST: " + missionId);
                if (missions.size() == originalSize) {
                    return crow::response(200, R"({"ok":false,"error":"mission not found"})");
                }
                return crow::response(200, R"({"ok":true})");
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("DELETE /api/missions error: ") + e.what());
                return crow::response(400, R"({"error":"mission delete failed"})");
            } });

        // ── GET /api/map/live — obstacles + active detour overlay (N5.1 + N5.2) ──
        CROW_ROUTE(app, "/api/map/live").methods(crow::HTTPMethod::GET)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                        {
            if (!isAuthorized(req))
                return crow::response(401, R"({\"error\":\"unauthorized\"})");
            nlohmann::json resp;
            {
                std::lock_guard<std::mutex> lk(liveOverlayMutex_);
                resp = liveOverlayJson_.is_object() && !liveOverlayJson_.empty()
                    ? liveOverlayJson_
                    : nlohmann::json({{"obstacles", nlohmann::json::array()}, {"detour", {{"active", false}, {"waypoints", nlohmann::json::array()}}}});
            }
            crow::response r(200, resp.dump());
            r.add_header("Content-Type", "application/json");
            return r; });

        // ── GET /api/mission/active-plan — return currently active mission plan JSON ──
        CROW_ROUTE(app, "/api/mission/active-plan").methods(crow::HTTPMethod::GET)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                                   {
            if (!isAuthorized(req))
                return crow::response(401, R"({"error":"unauthorized"})");
            nlohmann::json resp;
            {
                std::lock_guard<std::mutex> lkPlan(activePlanMutex_);
                resp = (activePlanJson_.is_object() && !activePlanJson_.empty())
                    ? activePlanJson_
                    : nlohmann::json({{"missionId", ""},
                                      {"waypoints", nlohmann::json::array()},
                                      {"zoneOrder", nlohmann::json::array()},
                                      {"route",
                                       {{"active", false},
                                        {"valid", true},
                                        {"invalidReason", ""},
                                        {"sourceMode", "free"},
                                        {"points", nlohmann::json::array()}}}});
            }
            {
                std::lock_guard<std::mutex> lkTel(telemetryMutex_);
                resp["waypointIndex"] = latestTelemetry_.waypoint_index;
                resp["waypointTotal"] = latestTelemetry_.waypoint_total;
                resp["activePointIndex"] = latestTelemetry_.waypoint_index;
            }
            crow::response r(200, resp.dump());
            r.add_header("Content-Type", "application/json");
            return r; });

        // ── POST /api/planner/preview — compute planner routes for preview ───────
        CROW_ROUTE(app, "/api/planner/preview").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                                {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            try {
                const auto body = nlohmann::json::parse(req.body.empty() ? "{}" : req.body);
                if (!body.is_object()) return crow::response(400, R"({"error":"expected object payload"})");

                nlohmann::json mapJson = body.value("map", nlohmann::json::object());
                if (!mapJson.is_object()) {
                    const std::string raw = mapPath_.empty() ? std::string() : readFile(mapPath_);
                    if (!raw.empty()) {
                        mapJson = nlohmann::json::parse(raw, nullptr, false);
                    }
                }
                if (!mapJson.is_object()) {
                    mapJson = nlohmann::json::parse(kDefaultMapJson);
                }

                nav::Map previewMap(config_);
                if (!previewMap.loadJson(mapJson)) {
                    return crow::response(400, R"({"error":"invalid map snapshot"})");
                }

                nlohmann::json routes = nlohmann::json::array();

                if (body.contains("mission") && body["mission"].is_object()) {
                    const nav::MissionPlan missionPlan = nav::buildMissionPlanPreview(previewMap, body["mission"]);
                    const nav::RoutePlan& route = missionPlan.route;
                    const nav::RouteValidationResult vr = nav::RouteValidator::validate(previewMap, route);
                    const nlohmann::json encodedPlan = encodeMissionPlanJson(missionPlan);

                    nlohmann::json validationErrors = nlohmann::json::array();
                    for (const auto& err : vr.errors) {
                        validationErrors.push_back({
                            {"pointIndex", err.pointIndex},
                            {"code",       static_cast<int>(err.code)},
                            {"message",    err.message},
                            {"zoneId",     err.zoneId},
                        });
                    }

                    std::string errorMsg;
                    if (route.points.empty()) {
                        errorMsg = "planner returned no route";
                    } else if (!vr.valid) {
                        errorMsg = vr.errors.empty() ? "route validation failed"
                                                     : vr.errors.front().message;
                    }

                    // N3.3: encode flat waypoint sequence for the WebUI preview.
                    nlohmann::json waypointsJson = nlohmann::json::array();
                    for (const auto &wp : missionPlan.waypoints)
                        waypointsJson.push_back({{"x", wp.x}, {"y", wp.y}, {"mowOn", wp.mowOn}});

                    routes.push_back({
                        {"index",            0},
                        {"ok",               !route.points.empty()},
                        {"valid",            vr.valid},
                        {"error",            errorMsg},
                        {"plan",             encodedPlan},
                        {"planRef",          encodedPlan["planRef"]},
                        {"validationErrors", validationErrors},
                        {"debug",            encodeRouteDebugJson(route)},
                        {"route",            encodeRoutePlanJson(route)},
                        {"waypoints",        waypointsJson},
                    });

                    // N6.1: notify Robot so it can cache this plan for the next start.
                    if (!route.points.empty()) {
                        PlanPreviewCallback cb;
                        {
                            std::lock_guard<std::mutex> lk(cmdMutex_);
                            cb = planPreviewCallback_;
                        }
                        if (cb)
                            cb(missionPlan);
                    }
                } else {
                    const auto jobs = body.value("jobs", nlohmann::json::array());
                    if (!jobs.is_array()) {
                        return crow::response(400, R"({"error":"expected jobs array"})");
                    }
                    for (size_t i = 0; i < jobs.size(); ++i) {
                        const auto& job = jobs[i];
                        nav::Point src;
                        nav::Point dst;
                        if (!jsonPointToNavPoint(job.value("source", nlohmann::json::array()), src)
                            || !jsonPointToNavPoint(job.value("destination", nlohmann::json::array()), dst)) {
                            routes.push_back({
                                {"index", i},
                                {"ok", false},
                                {"error", "invalid source/destination"},
                            });
                            continue;
                        }

                        const nav::WayType missionMode = parseWayType(job.value("missionMode", std::string("free")), nav::WayType::FREE);
                        const nav::WayType planningMode = parseWayType(job.value("planningMode", std::string("free")), nav::WayType::FREE);
                        const float headingReferenceRad = job.value("headingReferenceRad", 0.0f);
                        const bool hasHeadingReference = job.value("hasHeadingReference", false);
                        const bool reverseAllowed = job.value("reverseAllowed", false);
                        const float clearance_m = job.value("clearance_m", 0.25f);
                        const float robotRadius_m = job.value("robotRadius_m", 0.0f);

                        const nav::RoutePlan route = previewMap.previewPath(
                            src,
                            dst,
                            missionMode,
                            planningMode,
                            headingReferenceRad,
                            hasHeadingReference,
                            reverseAllowed,
                            clearance_m,
                            robotRadius_m);

                        if (route.points.empty()) {
                            routes.push_back({
                                {"index", i},
                                {"ok", false},
                                {"error", "planner returned no route"},
                            });
                            continue;
                        }

                        routes.push_back({
                            {"index", i},
                            {"ok", true},
                            {"route", encodeRoutePlanJson(route)},
                        });
                    }
                }

                nlohmann::json out = {
                    {"ok", true},
                    {"routes", routes},
                };
                crow::response res(200, out.dump());
                res.set_header("Content-Type", "application/json");
                res.set_header("Access-Control-Allow-Origin", "*");
                return res;
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("POST /api/planner/preview error: ") + e.what());
                return crow::response(400, R"({"error":"bad planner preview payload"})");
            } });

        // ── GET /api/maps — list stored maps + active selection ──────────────────
        CROW_ROUTE(app, "/api/maps").methods(crow::HTTPMethod::GET)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                    {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (mapPath_.empty()) return crow::response(503, R"({"error":"map path not configured"})");
            std::lock_guard<std::mutex> lk(mapCatalogMutex_);
            std::string err;
            if (!ensureMapsCatalogLocked(&err)) {
                logger_->warn(TAG, "GET /api/maps: " + err);
                return crow::response(500, R"({"error":"map catalog unavailable"})");
            }
            const auto catalog = loadMapsCatalogLocked();
            nlohmann::json out = nlohmann::json::object();
            out["active_id"] = catalog.value("active_id", std::string());
            out["maps"] = catalog.value("maps", nlohmann::json::array());
            return crow::response(200, out.dump()); });

        // ── POST /api/maps — create a new named map entry ────────────────────────
        CROW_ROUTE(app, "/api/maps").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                     {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (mapPath_.empty()) return crow::response(503, R"({"error":"map path not configured"})");
            try {
                const auto body = nlohmann::json::parse(req.body.empty() ? "{}" : req.body);
                const std::string requestedName = body.value("name", std::string("Neue Karte"));
                const bool activate = body.value("activate", true);

                nlohmann::json mapDoc;
                if (body.contains("map")) mapDoc = body["map"];
                if (!mapDoc.is_object()) {
                    const std::string raw = readFile(mapPath_);
                    mapDoc = nlohmann::json::parse(raw.empty() ? kDefaultMapJson : raw, nullptr, false);
                }
                if (!mapDoc.is_object()) {
                    return crow::response(400, R"({"error":"invalid map payload"})");
                }

                std::lock_guard<std::mutex> lk(mapCatalogMutex_);
                std::string err;
                if (!ensureMapsCatalogLocked(&err)) {
                    logger_->warn(TAG, "POST /api/maps ensure catalog failed: " + err);
                    return crow::response(500, R"({"error":"map catalog unavailable"})");
                }
                auto catalog = loadMapsCatalogLocked();
                auto maps = catalog.value("maps", nlohmann::json::array());

                std::string baseId = slugifyMapId(requestedName);
                std::unordered_set<std::string> usedIds;
                for (const auto& entry : maps) usedIds.insert(entry.value("id", std::string()));
                std::string id = baseId;
                int suffix = 2;
                while (usedIds.count(id) > 0) {
                    id = baseId + "-" + std::to_string(suffix++);
                }

                const std::string file = id + ".json";
                const auto mapFilePath = mapsDir() / file;
                std::filesystem::create_directories(mapFilePath.parent_path());
                {
                    std::ofstream out(mapFilePath);
                    if (!out.is_open()) return crow::response(500, R"({"error":"cannot write map file"})");
                    out << mapDoc.dump(2);
                }

                const long long now = unixNowMs();
                maps.push_back({
                    {"id", id},
                    {"name", requestedName.empty() ? id : requestedName},
                    {"file", file},
                    {"created_at_ms", now},
                    {"updated_at_ms", now},
                });
                catalog["maps"] = maps;
                if (catalog.value("active_id", std::string()).empty()) {
                    catalog["active_id"] = id;
                }
                if (!saveMapsCatalogLocked(catalog, &err)) {
                    logger_->warn(TAG, "POST /api/maps save catalog failed: " + err);
                    return crow::response(500, R"({"error":"cannot update map catalog"})");
                }

                if (activate) {
                    if (!activateMapByIdLocked(id, &err)) {
                        logger_->warn(TAG, "POST /api/maps activate failed: " + err);
                        return crow::response(200, std::string(R"({"ok":false,"error":")") + err + "\"}");
                    }
                }

                nlohmann::json out = {{"ok", true}, {"id", id}, {"active", activate}};
                return crow::response(200, out.dump());
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("POST /api/maps error: ") + e.what());
                return crow::response(400, R"({"error":"bad map payload"})");
            } });

        // ── POST /api/maps/<id>/activate — switch active map ─────────────────────
        CROW_ROUTE(app, "/api/maps/<string>/activate").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req, const std::string &mapId) -> crow::response
                                                                                       {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (mapPath_.empty()) return crow::response(503, R"({"error":"map path not configured"})");
            std::lock_guard<std::mutex> lk(mapCatalogMutex_);
            std::string err;
            const bool ok = activateMapByIdLocked(mapId, &err);
            if (!ok) {
                logger_->warn(TAG, "POST /api/maps/<id>/activate failed: " + err);
                return crow::response(200, std::string(R"({"ok":false,"error":")") + err + "\"}");
            }
            return crow::response(200, R"({"ok":true})"); });

        // ── DELETE /api/maps/<id> — delete stored map (except active) ────────────
        CROW_ROUTE(app, "/api/maps/<string>").methods(crow::HTTPMethod::DELETE)([this, isAuthorized](const crow::request &req, const std::string &mapId) -> crow::response
                                                                                {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (mapPath_.empty()) return crow::response(503, R"({"error":"map path not configured"})");
            std::lock_guard<std::mutex> lk(mapCatalogMutex_);
            std::string err;
            if (!ensureMapsCatalogLocked(&err)) {
                logger_->warn(TAG, "DELETE /api/maps ensure catalog failed: " + err);
                return crow::response(500, R"({"error":"map catalog unavailable"})");
            }
            auto catalog = loadMapsCatalogLocked();
            auto maps = catalog.value("maps", nlohmann::json::array());
            const std::string activeId = catalog.value("active_id", std::string());
            if (mapId == activeId) {
                return crow::response(200, R"({"ok":false,"error":"cannot delete active map"})");
            }

            bool removed = false;
            std::string fileToDelete;
            nlohmann::json kept = nlohmann::json::array();
            for (const auto& entry : maps) {
                if (entry.value("id", std::string()) == mapId) {
                    removed = true;
                    fileToDelete = entry.value("file", std::string());
                    continue;
                }
                kept.push_back(entry);
            }
            if (!removed) return crow::response(200, R"({"ok":false,"error":"map not found"})");
            if (kept.empty()) return crow::response(200, R"({"ok":false,"error":"at least one map required"})");

            catalog["maps"] = kept;
            if (!saveMapsCatalogLocked(catalog, &err)) {
                logger_->warn(TAG, "DELETE /api/maps save catalog failed: " + err);
                return crow::response(500, R"({"error":"cannot update map catalog"})");
            }
            if (!fileToDelete.empty()) {
                std::error_code ec;
                std::filesystem::remove(mapsDir() / fileToDelete, ec);
            }
            return crow::response(200, R"({"ok":true})"); });

        // ── Zone Profile CRUD ──────────────────────────────────────────────────────
        // Helper: read map JSON from file, apply a mutation, write back, reload robot.
        auto mutatMapZoneProfile = [this, isAuthorized](
                                       const crow::request &req,
                                       const std::string &zoneId,
                                       const std::function<crow::response(nlohmann::json & mapJson, nlohmann::json & jz)> &mutate)
            -> crow::response
        {
            if (!isAuthorized(req))
                return crow::response(401, R"({"error":"unauthorized"})");
            if (mapPath_.empty())
                return crow::response(503, R"({"error":"map path not configured"})");
            try
            {
                // Read current map
                nlohmann::json mapJson;
                {
                    const std::string raw = readFile(mapPath_);
                    if (raw.empty())
                        return crow::response(404, R"({"error":"map not found"})");
                    mapJson = nlohmann::json::parse(raw, nullptr, false);
                    if (mapJson.is_discarded() || !mapJson.is_object())
                        return crow::response(500, R"({"error":"map parse error"})");
                }
                // Find the zone
                if (!mapJson.contains("zones") || !mapJson["zones"].is_array())
                    return crow::response(404, R"({"error":"zone not found"})");
                nlohmann::json *jzPtr = nullptr;
                for (auto &jz : mapJson["zones"])
                {
                    if (jz.value("id", "") == zoneId)
                    {
                        jzPtr = &jz;
                        break;
                    }
                }
                if (!jzPtr)
                    return crow::response(404, R"({"error":"zone not found"})");
                // Ensure profiles object exists
                if (!jzPtr->contains("profiles") || !(*jzPtr)["profiles"].is_object())
                    (*jzPtr)["profiles"] = nlohmann::json::object();

                // Apply the specific mutation
                crow::response result = mutate(mapJson, *jzPtr);

                // Write & reload only on success (2xx)
                if (result.code >= 200 && result.code < 300)
                {
                    {
                        std::ofstream f(mapPath_);
                        if (!f.is_open())
                            return crow::response(500, R"({"error":"cannot write map"})");
                        f << mapJson.dump(2);
                    }
                    {
                        std::lock_guard<std::mutex> mapsLk(mapCatalogMutex_);
                        std::string mapErr;
                        syncActiveMapSnapshotLocked(mapJson, &mapErr);
                    }
                    {
                        std::lock_guard<std::mutex> lk(mapReloadMutex_);
                        if (mapReloadCallback_)
                            mapReloadCallback_();
                    }
                }
                return result;
            }
            catch (const std::exception &e)
            {
                return crow::response(400, std::string(R"({"error":")") + e.what() + "\"}");
            }
        };

        // GET /api/zones/<zoneId>/profiles
        CROW_ROUTE(app, "/api/zones/<string>/profiles")
            .methods(crow::HTTPMethod::GET)([this, isAuthorized](const crow::request &req, const std::string &zoneId) -> crow::response
                                            {
            if (!isAuthorized(req))
                return crow::response(401, R"({"error":"unauthorized"})");
            if (mapPath_.empty())
                return crow::response(503, R"({"error":"map path not configured"})");
            try {
                const std::string raw = readFile(mapPath_);
                if (raw.empty()) return crow::response(404, R"({"error":"map not found"})");
                auto mapJson = nlohmann::json::parse(raw, nullptr, false);
                if (mapJson.is_discarded() || !mapJson.is_object())
                    return crow::response(500, R"({"error":"map parse error"})");
                if (!mapJson.contains("zones")) return crow::response(404, R"({"error":"zone not found"})");
                for (const auto &jz : mapJson["zones"]) {
                    if (jz.value("id","") != zoneId) continue;
                    nlohmann::json resp;
                    resp["zone_id"] = zoneId;
                    resp["active_profile"] = jz.value("activeProfile", "");
                    resp["profiles"] = nlohmann::json::array();
                    const auto &profs = jz.contains("profiles") && jz["profiles"].is_object()
                                        ? jz["profiles"] : nlohmann::json::object();
                    for (auto it = profs.begin(); it != profs.end(); ++it) {
                        nlohmann::json entry = it.value();
                        entry["id"] = it.key();
                        resp["profiles"].push_back(entry);
                    }
                    return crow::response(200, resp.dump());
                }
                return crow::response(404, R"({"error":"zone not found"})");
            } catch (const std::exception &e) {
                return crow::response(400, std::string(R"({"error":")") + e.what() + "\"}");
            } });

        // POST /api/zones/<zoneId>/profiles  — create a new profile
        CROW_ROUTE(app, "/api/zones/<string>/profiles")
            .methods(crow::HTTPMethod::POST)([this, isAuthorized, mutatMapZoneProfile](const crow::request &req, const std::string &zoneId) -> crow::response
                                             { return mutatMapZoneProfile(req, zoneId, [&](nlohmann::json &, nlohmann::json &jz) -> crow::response
                                                                          {
                auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded() || !body.is_object())
                    return crow::response(400, R"({"error":"invalid JSON body"})");
                const std::string profileName = body.value("name", "");
                if (profileName.empty())
                    return crow::response(400, R"({"error":"name required"})");
                // Derive id from name (lowercase, spaces to underscores)
                std::string profileId = profileName;
                std::transform(profileId.begin(), profileId.end(), profileId.begin(), ::tolower);
                std::replace(profileId.begin(), profileId.end(), ' ', '_');
                // Ensure uniqueness by appending suffix if needed
                const std::string baseId = profileId;
                int suffix = 2;
                while (jz["profiles"].contains(profileId))
                    profileId = baseId + "_" + std::to_string(suffix++);
                const uint64_t now = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                nlohmann::json profile;
                profile["name"] = profileName;
                profile["settings"] = body.value("settings", nlohmann::json::object());
                profile["created"] = now;
                profile["lastUsed"] = 0;
                jz["profiles"][profileId] = profile;
                nlohmann::json resp;
                resp["profile_id"] = profileId;
                resp["status"] = "created";
                resp["message"] = "Profile saved and will be persisted to map.json";
                return crow::response(201, resp.dump()); }); });

        // PUT /api/zones/<zoneId>/profiles/<profileId>  — activate a profile or update it
        CROW_ROUTE(app, "/api/zones/<string>/profiles/<string>")
            .methods(crow::HTTPMethod::PUT)([this, isAuthorized, mutatMapZoneProfile](const crow::request &req, const std::string &zoneId, const std::string &profileId) -> crow::response
                                            { return mutatMapZoneProfile(req, zoneId, [&](nlohmann::json &, nlohmann::json &jz) -> crow::response
                                                                         {
                if (!jz["profiles"].contains(profileId))
                    return crow::response(404, R"({"error":"profile not found"})");
                auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded()) body = nlohmann::json::object();
                const std::string action = body.value("action", "");
                if (action == "activate" || action.empty()) {
                    const uint64_t now = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                    jz["activeProfile"] = profileId;
                    jz["profiles"][profileId]["lastUsed"] = now;
                    nlohmann::json resp;
                    resp["active_profile"] = profileId;
                    resp["status"] = "ok";
                    resp["zone_id"] = zoneId;
                    return crow::response(200, resp.dump());
                }
                // Update settings
                if (body.contains("name"))
                    jz["profiles"][profileId]["name"] = body["name"];
                if (body.contains("settings") && body["settings"].is_object())
                    jz["profiles"][profileId]["settings"] = body["settings"];
                nlohmann::json resp;
                resp["profile_id"] = profileId;
                resp["status"] = "updated";
                return crow::response(200, resp.dump()); }); });

        // DELETE /api/zones/<zoneId>/profiles/<profileId>
        CROW_ROUTE(app, "/api/zones/<string>/profiles/<string>")
            .methods(crow::HTTPMethod::DELETE)([this, isAuthorized, mutatMapZoneProfile](const crow::request &req, const std::string &zoneId, const std::string &profileId) -> crow::response
                                               { return mutatMapZoneProfile(req, zoneId, [&](nlohmann::json &, nlohmann::json &jz) -> crow::response
                                                                            {
                if (!jz["profiles"].contains(profileId))
                    return crow::response(404, R"({"error":"profile not found"})");
                jz["profiles"].erase(profileId);
                if (jz.value("activeProfile", "") == profileId)
                    jz["activeProfile"] = "";
                nlohmann::json resp;
                resp["deleted_profile"] = profileId;
                resp["status"] = "ok";
                return crow::response(200, resp.dump()); }); });

        // ── POST /api/map ──────────────────────────────────────────────────────────
        CROW_ROUTE(app, "/api/map").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                    {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (mapPath_.empty()) return crow::response(503);
            try {
                auto j = nlohmann::json::parse(req.body);
                if (!j.is_object()) return crow::response(400);

                // Write map file
                {
                    std::ofstream f(mapPath_);
                    if (!f.is_open()) {
                        logger_->warn(TAG, "POST /api/map: cannot write " + mapPath_);
                        return crow::response(500);
                    }
                    f << j.dump(2);
                }
                {
                    std::lock_guard<std::mutex> mapsLk(mapCatalogMutex_);
                    std::string mapErr;
                    if (!syncActiveMapSnapshotLocked(j, &mapErr)) {
                        logger_->warn(TAG, "POST /api/map: active snapshot sync failed: " + mapErr);
                    }
                }

                // Notify robot to reload
                bool ok = true;
                {
                    std::lock_guard<std::mutex> lk(mapReloadMutex_);
                    if (mapReloadCallback_) ok = mapReloadCallback_();
                }
                logger_->info(TAG, "Map updated via REST"
                              + std::string(ok ? "" : " (reload failed)"));
                if (ok) return crow::response(200, R"({"ok":true})");
                return crow::response(200, R"({"ok":false,"error":"reload failed"})");
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("POST /api/map error: ") + e.what());
                return crow::response(400);
            } });

        // ── GET /api/map/geojson — export current map as GeoJSON ──────────────────
        // Coordinates are WGS84 (EPSG:4326).  Origin stored in map.json under key
        // "origin": {"lat":…,"lon":…}.  If absent, lat0=lon0=0 (not georeferenced).
        CROW_ROUTE(app, "/api/map/geojson")(
            [this, isAuthorized](const crow::request &req) -> crow::response
            {
                if (!isAuthorized(req))
                    return crow::response(401, R"({"error":"unauthorized"})");
                if (mapPath_.empty())
                    return crow::response(503);
                const std::string raw = readFile(mapPath_);
                if (raw.empty())
                    return crow::response(404);
                try
                {
                    const auto map = nlohmann::json::parse(raw);

                    // ── origin ────────────────────────────────────────────────────
                    double lat0 = 0.0, lon0 = 0.0;
                    bool georef = false;
                    if (map.contains("origin") && map["origin"].is_object())
                    {
                        lat0 = map["origin"].value("lat", 0.0);
                        lon0 = map["origin"].value("lon", 0.0);
                        georef = (lat0 != 0.0 || lon0 != 0.0);
                    }

                    // ── coordinate helpers ────────────────────────────────────────
                    constexpr double R = 6371000.0;
                    constexpr double DEG = M_PI / 180.0;

                    auto toWgs = [&](double x, double y)
                        -> std::pair<double, double>
                    {
                        const double lat = lat0 + y / (R * DEG);
                        const double lon = lon0 + x / (R * std::cos(lat0 * DEG) * DEG);
                        return {lat, lon};
                    };

                    // local [[x,y],…] → closed GeoJSON ring [[lon,lat],…,[lon0,lat0]]
                    auto toRing = [&](const nlohmann::json &pts) -> nlohmann::json
                    {
                        auto ring = nlohmann::json::array();
                        for (const auto &p : pts)
                        {
                            auto [lat, lon] = toWgs(p[0].get<double>(), p[1].get<double>());
                            ring.push_back({lon, lat});
                        }
                        if (!ring.empty())
                            ring.push_back(ring[0]); // close
                        return ring;
                    };

                    // ── build FeatureCollection ───────────────────────────────────
                    nlohmann::json fc;
                    fc["type"] = "FeatureCollection";
                    fc["georeferenced"] = georef;
                    fc["origin"] = {{"lat", lat0}, {"lon", lon0}};
                    fc["features"] = nlohmann::json::array();

                    // perimeter
                    if (map.contains("perimeter") && !map["perimeter"].empty())
                    {
                        fc["features"].push_back({{"type", "Feature"},
                                                  {"properties", {{"type", "perimeter"}}},
                                                  {"geometry", {{"type", "Polygon"}, {"coordinates", nlohmann::json::array({toRing(map["perimeter"])})}}}});
                    }
                    // exclusions
                    if (map.contains("exclusions"))
                    {
                        int idx = 0;
                        for (const auto &ex : map["exclusions"])
                        {
                            fc["features"].push_back({{"type", "Feature"},
                                                      {"properties", {{"type", "exclusion"}, {"index", idx++}}},
                                                      {"geometry", {{"type", "Polygon"}, {"coordinates", nlohmann::json::array({toRing(ex)})}}}});
                        }
                    }
                    // dock (Point)
                    if (map.contains("dock") && !map["dock"].empty())
                    {
                        const auto &d = map["dock"][0];
                        auto [lat, lon] = toWgs(d[0].get<double>(), d[1].get<double>());
                        fc["features"].push_back({{"type", "Feature"},
                                                  {"properties", {{"type", "dock"}}},
                                                  {"geometry", {{"type", "Point"}, {"coordinates", {lon, lat}}}}});
                    }
                    const std::string body = fc.dump(2);
                    crow::response res(200, body);
                    res.set_header("Content-Type", "application/geo+json");
                    res.set_header("Content-Disposition",
                                   "attachment; filename=\"sunray-map.geojson\"");
                    return res;
                }
                catch (const std::exception &e)
                {
                    logger_->warn(TAG, std::string("GET /api/map/geojson: ") + e.what());
                    return crow::response(500);
                }
            });

        // ── POST /api/map/geojson — import GeoJSON, convert to internal format ─────
        // Accepts a GeoJSON FeatureCollection.
        // Origin: taken from top-level "origin" field if present, otherwise computed
        // as the centroid of the "perimeter" feature.
        CROW_ROUTE(app, "/api/map/geojson").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                            {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (mapPath_.empty()) return crow::response(503);
            try {
                const auto fc = nlohmann::json::parse(req.body);
                if (!fc.is_object() ||
                    fc.value("type","") != "FeatureCollection")
                    return crow::response(400, R"({"error":"not a FeatureCollection"})");

                // ── origin ────────────────────────────────────────────────────
                double lat0 = 0.0, lon0 = 0.0;
                bool hasOrigin = false;

                if (fc.contains("origin") && fc["origin"].is_object()) {
                    lat0      = fc["origin"].value("lat", 0.0);
                    lon0      = fc["origin"].value("lon", 0.0);
                    hasOrigin = true;
                }

                // No explicit origin → centroid of perimeter ring
                if (!hasOrigin) {
                    for (const auto& feat :
                         fc.value("features", nlohmann::json::array()))
                    {
                        if (feat["properties"].value("type","") == "perimeter"
                            && feat["geometry"].value("type","") == "Polygon")
                        {
                            const auto& ring = feat["geometry"]["coordinates"][0];
                            double sumLat = 0, sumLon = 0;
                            int    n      = 0;
                            for (const auto& c : ring) {
                                sumLon += c[0].get<double>();
                                sumLat += c[1].get<double>();
                                ++n;
                            }
                            if (n > 0) {
                                lon0 = sumLon / n;
                                lat0 = sumLat / n;
                                hasOrigin = true;
                            }
                            break;
                        }
                    }
                }

                if (!hasOrigin)
                    return crow::response(400,
                        R"({"error":"cannot determine origin — no 'origin' field and no perimeter feature"})");

                // ── coordinate helpers ────────────────────────────────────────
                constexpr double R   = 6371000.0;
                constexpr double DEG = M_PI / 180.0;

                // [lon, lat] → local [x, y]
                auto toLocal = [&](double lon, double lat)
                    -> std::pair<double,double>
                {
                    const double x = (lon - lon0) * R * std::cos(lat0 * DEG) * DEG;
                    const double y = (lat - lat0) * R * DEG;
                    return { x, y };
                };

                // GeoJSON ring → local [[x,y],…] (closing point stripped)
                auto ringToLocal = [&](const nlohmann::json& ring)
                    -> nlohmann::json
                {
                    auto pts = nlohmann::json::array();
                    for (const auto& c : ring) {
                        auto [x, y] = toLocal(c[0].get<double>(), c[1].get<double>());
                        pts.push_back({ x, y });
                    }
                    // GeoJSON polygons repeat the first vertex — remove it
                    if (pts.size() > 1 && pts.front() == pts.back())
                        pts.erase(pts.end() - 1);
                    return pts;
                };

                // ── build internal map ────────────────────────────────────────
                nlohmann::json outMap;
                outMap["origin"]     = { {"lat", lat0}, {"lon", lon0} };
                outMap["perimeter"]  = nlohmann::json::array();
                outMap["exclusions"] = nlohmann::json::array();
                outMap["dock"]       = nlohmann::json::array();
                outMap["obstacles"]  = nlohmann::json::array();

                for (const auto& feat :
                     fc.value("features", nlohmann::json::array()))
                {
                    const std::string ftype = feat["properties"].value("type","");
                    const std::string gtype = feat["geometry"].value("type","");

                    if (ftype == "perimeter" && gtype == "Polygon") {
                        outMap["perimeter"] =
                            ringToLocal(feat["geometry"]["coordinates"][0]);

                    } else if (ftype == "exclusion" && gtype == "Polygon") {
                        outMap["exclusions"].push_back(
                            ringToLocal(feat["geometry"]["coordinates"][0]));

                    } else if (ftype == "dock") {
                        if (gtype == "Point") {
                            auto [x, y] = toLocal(
                                feat["geometry"]["coordinates"][0].get<double>(),
                                feat["geometry"]["coordinates"][1].get<double>());
                            outMap["dock"].push_back({ x, y });
                        } else if (gtype == "Polygon") {
                            // dock stored as polygon: use centroid
                            const auto& ring = feat["geometry"]["coordinates"][0];
                            double sx = 0, sy = 0; int n = 0;
                            for (const auto& c : ring) {
                                auto [x,y] = toLocal(c[0].get<double>(), c[1].get<double>());
                                sx += x; sy += y; ++n;
                            }
                            if (n > 0) outMap["dock"].push_back({ sx/n, sy/n });
                        }
                    }
                    // obstacles: not in standard GeoJSON → ignored on import
                }

                // ── write & reload ────────────────────────────────────────────
                {
                    std::ofstream f(mapPath_);
                    if (!f.is_open()) return crow::response(500);
                    f << outMap.dump(2);
                }
                {
                    std::lock_guard<std::mutex> mapsLk(mapCatalogMutex_);
                    std::string mapErr;
                    if (!syncActiveMapSnapshotLocked(outMap, &mapErr)) {
                        logger_->warn(TAG, "POST /api/map/geojson: active snapshot sync failed: " + mapErr);
                    }
                }
                bool ok = true;
                {
                    std::lock_guard<std::mutex> lk(mapReloadMutex_);
                    if (mapReloadCallback_) ok = mapReloadCallback_();
                }
                logger_->info(TAG, "Map imported via GeoJSON (origin "
                              + std::to_string(lat0) + "," + std::to_string(lon0) + ")");
                if (ok) return crow::response(200, R"({"ok":true})");
                return crow::response(200,
                    R"({"ok":false,"error":"reload failed"})");

            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("POST /api/map/geojson: ") + e.what());
                nlohmann::json err; err["error"] = e.what();  // BUG-007 fix
                return crow::response(400, err.dump());
            } });

        // ── GET /api/schedule — C.11 ──────────────────────────────────────────────
        CROW_ROUTE(app, "/api/schedule")(
            [this, isAuthorized](const crow::request &req) -> crow::response
            {
                if (!isAuthorized(req))
                    return crow::response(401, R"({"error":"unauthorized"})");
                nlohmann::json result;
                {
                    std::lock_guard<std::mutex> lk(schedCbMutex_);
                    result = schedGetCb_ ? schedGetCb_() : nlohmann::json::array();
                }
                const std::string body = result.dump();
                crow::response res(200, body);
                res.set_header("Content-Type", "application/json");
                return res;
            });

        // ── PUT /api/schedule — C.11 ───────────────────────────────────────────────
        CROW_ROUTE(app, "/api/schedule").methods(crow::HTTPMethod::PUT)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                        {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            try {
                auto arr = nlohmann::json::parse(req.body);
                nlohmann::json result;
                {
                    std::lock_guard<std::mutex> lk(schedCbMutex_);
                    result = schedPutCb_ ? schedPutCb_(arr) : nlohmann::json{{"ok",false}};
                }
                const std::string body = result.dump();
                crow::response res(200, body);
                res.set_header("Content-Type", "application/json");
                return res;
            } catch (...) {
                return crow::response(400, R"({"error":"bad JSON"})");
            } });

        // ── GET /api/history/events ───────────────────────────────────────────────
        CROW_ROUTE(app, "/api/history/events")(
            [this, isAuthorized, parseLimit](const crow::request &req) -> crow::response
            {
                if (!isAuthorized(req))
                    return crow::response(401, R"({"error":"unauthorized"})");
                const unsigned limit = parseLimit(req, 100);
                nlohmann::json result;
                {
                    std::lock_guard<std::mutex> lk(historyCbMutex_);
                    result = historyEventsGetCb_
                                 ? historyEventsGetCb_(limit)
                                 : nlohmann::json{{"ok", false}, {"error", "history events callback not set"}};
                }
                crow::response res(result.value("ok", true) ? 200 : 503, result.dump());
                res.set_header("Content-Type", "application/json");
                return res;
            });

        // ── GET /api/history/sessions ─────────────────────────────────────────────
        CROW_ROUTE(app, "/api/history/sessions")(
            [this, isAuthorized, parseLimit](const crow::request &req) -> crow::response
            {
                if (!isAuthorized(req))
                    return crow::response(401, R"({"error":"unauthorized"})");
                const unsigned limit = parseLimit(req, 100);
                nlohmann::json result;
                {
                    std::lock_guard<std::mutex> lk(historyCbMutex_);
                    result = historySessionsGetCb_
                                 ? historySessionsGetCb_(limit)
                                 : nlohmann::json{{"ok", false}, {"error", "history sessions callback not set"}};
                }
                crow::response res(result.value("ok", true) ? 200 : 503, result.dump());
                res.set_header("Content-Type", "application/json");
                return res;
            });

        // ── GET /api/history/export ───────────────────────────────────────────────
        CROW_ROUTE(app, "/api/history/export")(
            [this, isAuthorized](const crow::request &req) -> crow::response
            {
                if (!isAuthorized(req))
                    return crow::response(401, R"({"error":"unauthorized"})");
                if (!config_)
                    return crow::response(503, R"({"error":"config unavailable"})");

                const bool enabled = config_->get<bool>("history_db_enabled", true);
                const bool exportEnabled = config_->get<bool>("history_db_export_enabled", true);
                const std::filesystem::path historyPath =
                    config_->get<std::string>("history_db_path", "/var/lib/sunray/history.db");

                if (!enabled)
                    return crow::response(503, R"({"error":"history database disabled"})");
                if (!exportEnabled)
                    return crow::response(403, R"({"error":"history export disabled"})");
                if (!std::filesystem::exists(historyPath))
                    return crow::response(404, R"({"error":"history database not found"})");

                const std::string body = readFile(historyPath);
                crow::response res(200, body);
                res.set_header("Content-Type", "application/octet-stream");
                res.set_header("Content-Disposition", "attachment; filename=\"sunray-history.db\"");
                return res;
            });

        // ── POST /api/history/clear ───────────────────────────────────────────────
        CROW_ROUTE(app, "/api/history/clear").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                              {
                if (!isAuthorized(req))
                    return crow::response(401, R"({"error":"unauthorized"})");
                nlohmann::json result;
                {
                    std::lock_guard<std::mutex> lk(historyCbMutex_);
                    result = historyClearCb_
                        ? historyClearCb_()
                        : nlohmann::json{{"ok", false}, {"error", "history clear callback not set"}};
                }
                crow::response res(result.value("ok", true) ? 200 : 503, result.dump());
                res.set_header("Content-Type", "application/json");
                return res; });

        // ── GET /api/statistics/summary ───────────────────────────────────────────
        CROW_ROUTE(app, "/api/statistics/summary")(
            [this, isAuthorized](const crow::request &req) -> crow::response
            {
                if (!isAuthorized(req))
                    return crow::response(401, R"({"error":"unauthorized"})");
                nlohmann::json result;
                {
                    std::lock_guard<std::mutex> lk(historyCbMutex_);
                    result = statisticsSummaryGetCb_
                                 ? statisticsSummaryGetCb_()
                                 : nlohmann::json{{"ok", false}, {"error", "statistics summary callback not set"}};
                }
                crow::response res(result.value("ok", true) ? 200 : 503, result.dump());
                res.set_header("Content-Type", "application/json");
                return res;
            });

        // ── POST /api/diag/<action> ───────────────────────────────────────────────
        // Safety-first implementation: run the diagnostic synchronously and return
        // the JSON result only after completion. A previous detached-thread version
        // captured crow::response by reference across request lifetime, which could
        // corrupt memory and crash the process after motor/tick diagnostics.
        CROW_ROUTE(app, "/api/diag/<string>").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req, const std::string &action) -> crow::response
                                                                              {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            nlohmann::json params;
            if (!req.body.empty()) {
                try { params = nlohmann::json::parse(req.body); }
                catch (...) { return crow::response(400, R"({"error":"bad JSON"})"); }
            }
            DiagCallback diagCb;
            {
                std::lock_guard<std::mutex> lk(diagCbMutex_);
                diagCb = diagCallback_;
            }
            if (!diagCb) return crow::response(501, R"({"error":"diag callback not set"})");

            try {
                crow::response res(200, diagCb(action, params).dump());
                res.set_header("Content-Type", "application/json");
                return res;
            } catch (const std::exception& e) {
                logger_->error(TAG, std::string("POST /api/diag/") + action + " failed: " + e.what());
                return crow::response(
                    500, std::string(R"({"ok":false,"error":")") + e.what() + "\"}");
            } catch (...) {
                logger_->error(TAG, std::string("POST /api/diag/") + action + " failed: unknown error");
                return crow::response(500, R"({"ok":false,"error":"unknown error"})");
            } });

        // ── POST /api/sim/<action> ─────────────────────────────────────────────────
        CROW_ROUTE(app, "/api/sim/<string>").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req, const std::string &action) -> crow::response
                                                                             {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            nlohmann::json params;
            if (!req.body.empty()) {
                try { params = nlohmann::json::parse(req.body); }
                catch (...) { return crow::response(400); }
            }
            {
                std::lock_guard<std::mutex> lk(simCmdMutex_);
                if (simCommandCallback_) simCommandCallback_(action, params);
            }
            return crow::response(200, R"({"ok":true})"); });

        // ── POST /api/ota/check — check whether a newer git commit is available ────
        CROW_ROUTE(app, "/api/ota/check").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                          {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");

            const std::string scriptPath = otaScriptPath_;
            if (scriptPath.empty() || !std::filesystem::exists(scriptPath)) {
                return crow::response(503, R"({"error":"ota_script_not_found"})");
            }

            const std::string cmd = scriptPath + " --check-only 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return crow::response(500, R"({"error":"popen_failed"})");

            char buf[256];
            std::string output;
            while (fgets(buf, sizeof(buf), pipe)) output += buf;
            pclose(pipe);

            // Trim trailing newline
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }

            nlohmann::json j;
            if (output == "already_up_to_date") {
                j["status"] = "up_to_date";
            } else if (output.rfind("update_available:", 0) == 0) {
                j["status"] = "update_available";
                j["hash"]   = output.substr(std::string("update_available:").size());
            } else if (output.rfind("error:", 0) == 0) {
                j["status"] = "error";
                j["detail"] = output.substr(6);
            } else {
                j["status"] = "unknown";
                j["detail"] = output;
            }

            crow::response res(200, j.dump());
            res.set_header("Content-Type", "application/json");
            return res; });

        // ── POST /api/ota/update — pull, build, restart (async, runs in background) ─
        CROW_ROUTE(app, "/api/ota/update").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                           {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");

            const std::string scriptPath = otaScriptPath_;
            if (scriptPath.empty() || !std::filesystem::exists(scriptPath)) {
                return crow::response(503, R"({"error":"ota_script_not_found"})");
            }

            if (otaRunning_.exchange(true)) {
                return crow::response(409, R"({"error":"update_already_running"})");
            }

            logger_->info(TAG, "OTA update triggered via WebUI");
            std::thread([this, scriptPath]() {
                const int rc = std::system(scriptPath.c_str());
                if (rc != 0) {
                    logger_->warn(TAG, "OTA update script exited with code " + std::to_string(rc));
                }
                otaRunning_.store(false);
            }).detach();

            crow::response res(200, R"({"status":"update_started"})");
            res.set_header("Content-Type", "application/json");
            return res; });

        // ── POST /api/restart — restart sunray-core.service immediately ─────────
        CROW_ROUTE(app, "/api/restart").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                        {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");

            logger_->info(TAG, "Service restart triggered via WebUI");
            std::thread([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::system("sudo /bin/systemctl restart sunray-core.service >/dev/null 2>&1");
            }).detach();

            crow::response res(200, R"({"status":"restarting"})");
            res.set_header("Content-Type", "application/json");
            return res; });

        // ── POST /api/stm/probe — verify STM32 SWD flash path availability ──────
        CROW_ROUTE(app, "/api/stm/probe").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                          {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");

            const std::string scriptPath = stmFlashScriptPath_;
            if (scriptPath.empty() || !std::filesystem::exists(scriptPath)) {
                return crow::response(503, R"({"error":"stm_flash_script_not_found"})");
            }

            const std::string cmd = "sudo /bin/bash \"" + scriptPath + "\" probe 2>&1";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return crow::response(500, R"({"error":"popen_failed"})");

            char buf[256];
            std::string output;
            while (fgets(buf, sizeof(buf), pipe)) output += buf;
            const int rc = pclose(pipe);

            while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }

            nlohmann::json j;
            j["ok"] = (rc == 0);
            if (rc == 0) {
                j["status"] = "probe_ok";
            } else if (output.find("openocd: command not found") != std::string::npos) {
                j["status"] = "probe_tool_missing";
            } else {
                j["status"] = "probe_failed";
            }
            j["detail"] = output.empty()
                ? ((rc == 0) ? "SWD probe completed" : "STM probe failed")
                : output;

            crow::response res(200, j.dump());
            res.set_header("Content-Type", "application/json");
            return res; });

        // ── GET /api/stm/uploaded — metadata for the currently uploaded STM .bin ─
        CROW_ROUTE(app, "/api/stm/uploaded").methods(crow::HTTPMethod::GET)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                            {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");

            crow::response res(200, buildStmUploadInfoJson().dump());
            res.set_header("Content-Type", "application/json");
            return res; });

        // ── POST /api/stm/upload — store uploaded STM firmware binary on Alfred ──
        CROW_ROUTE(app, "/api/stm/upload").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                           {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");
            if (req.body.empty()) return crow::response(400, R"({"error":"stm_upload_empty"})");

            const auto uploadDir = stmUploadDir();
            const auto binPath = stmUploadedBinPath();
            const auto metaPath = stmUploadedMetaPath();

            try {
                std::filesystem::create_directories(uploadDir);
                {
                    std::ofstream out(binPath, std::ios::binary | std::ios::trunc);
                    if (!out) return crow::response(500, R"({"error":"stm_upload_open_failed"})");
                    out.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));
                    if (!out.good()) return crow::response(500, R"({"error":"stm_upload_write_failed"})");
                }

                nlohmann::json meta = {
                    {"original_name", sanitizeUploadName(req.get_header_value("X-File-Name"))},
                    {"uploaded_at_ms", unixNowMs()},
                };
                std::ofstream metaOut(metaPath, std::ios::binary | std::ios::trunc);
                if (!metaOut) return crow::response(500, R"({"error":"stm_upload_meta_open_failed"})");
                metaOut << meta.dump(2);
                metaOut.flush();
                metaOut.close();
                if (!metaOut.good()) return crow::response(500, R"({"error":"stm_upload_meta_write_failed"})");

                crow::response res(200, buildStmUploadInfoJson().dump());
                res.set_header("Content-Type", "application/json");
                return res;
            } catch (const std::exception& e) {
                logger_->error(TAG, std::string("POST /api/stm/upload failed: ") + e.what());
                return crow::response(500, R"({"error":"stm_upload_failed"})");
            } });

        // ── POST /api/stm/flash — flash the last uploaded STM firmware binary ────
        CROW_ROUTE(app, "/api/stm/flash").methods(crow::HTTPMethod::POST)([this, isAuthorized](const crow::request &req) -> crow::response
                                                                          {
            if (!isAuthorized(req)) return crow::response(401, R"({"error":"unauthorized"})");

            const std::string scriptPath = stmFlashScriptPath_;
            if (scriptPath.empty() || !std::filesystem::exists(scriptPath)) {
                return crow::response(503, R"({"error":"stm_flash_script_not_found"})");
            }

            const auto uploadInfo = buildStmUploadInfoJson();
            if (!uploadInfo.value("exists", false)) {
                return crow::response(409, R"({"error":"stm_upload_missing"})");
            }

            TelemetryData latest;
            {
                std::lock_guard<std::mutex> lk(telemetryMutex_);
                latest = latestTelemetry_;
            }
            if (!(latest.op == "Idle" || latest.op == "Charge")) {
                return crow::response(409, R"({"error":"stm_flash_requires_idle_or_charge"})");
            }

            if (stmFlashRunning_.exchange(true)) {
                return crow::response(409, R"({"error":"stm_flash_already_running"})");
            }
            {
                std::lock_guard<std::mutex> lk(stmFlashStateMutex_);
                if (stmFlashStateCb_) stmFlashStateCb_(true, 0);
            }

            const std::filesystem::path wrapperPath =
                std::filesystem::path(scriptPath).parent_path() / "flash_uploaded_stm.sh";
            if (!std::filesystem::exists(wrapperPath)) {
                {
                    std::lock_guard<std::mutex> lk(stmFlashStateMutex_);
                    if (stmFlashStateCb_) stmFlashStateCb_(false, 0);
                }
                stmFlashRunning_.store(false);
                return crow::response(503, R"({"error":"stm_flash_wrapper_not_found"})");
            }

            logger_->info(TAG, "STM upload flash triggered via WebUI");
            const std::string cmd = "sudo /bin/bash \"" + wrapperPath.string() + "\" 2>&1";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                {
                    std::lock_guard<std::mutex> lk(stmFlashStateMutex_);
                    if (stmFlashStateCb_) stmFlashStateCb_(false, 0);
                }
                stmFlashRunning_.store(false);
                return crow::response(500, R"({"error":"popen_failed"})");
            }

            char buf[256];
            std::string output;
            while (fgets(buf, sizeof(buf), pipe)) output += buf;
            const int rc = pclose(pipe);
            stmFlashRunning_.store(false);
            {
                std::lock_guard<std::mutex> lk(stmFlashStateMutex_);
                if (stmFlashStateCb_) stmFlashStateCb_(false, rc == 0 ? 15000 : 0);
            }

            while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }

            nlohmann::json j;
            j["ok"] = (rc == 0);
            j["status"] = (rc == 0) ? "flash_ok" : "flash_failed";
            j["detail"] = output.empty()
                ? ((rc == 0) ? "STM flash completed" : "STM flash failed")
                : output;

            if (rc == 0) {
                auto logger = logger_;
                std::thread([logger]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
                    if (logger) {
                        logger->info("WsServer",
                                     "STM flash successful — restarting sunray-core.service");
                    }
                    const int restartRc = std::system(
                        "sudo /bin/systemctl restart sunray-core.service >/dev/null 2>&1");
                    if (restartRc != 0 && logger) {
                        logger->warn("WsServer",
                                     "Automatic restart after STM flash failed");
                    }
                }).detach();
            }

            crow::response res(200, j.dump());
            res.set_header("Content-Type", "application/json");
            return res; });
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────────

    void WebSocketServer::start()
    {
        if (running_.load())
            return;

        const int port = config_ ? config_->get<int>("ws_port", 8765) : 8765;

        // Register HTTP routes first
        setupHttpRoutes();

        // Configure WebSocket route
        CROW_WEBSOCKET_ROUTE(impl_->app, "/ws/telemetry")
            .onopen([this](crow::websocket::connection &conn)
                    {
            {
                std::lock_guard<std::mutex> lk(impl_->connMutex);
                impl_->connections.insert(&conn);
            }
            logger_->info(TAG, "WS client connected"); })
            .onclose([this](crow::websocket::connection &conn, const std::string &)
                     {
            {
                std::lock_guard<std::mutex> lk(impl_->connMutex);
                impl_->connections.erase(&conn);
            }
            logger_->info(TAG, "WS client disconnected"); })
            .onmessage([this](crow::websocket::connection &,
                              const std::string &data, bool /*is_binary*/)
                       {
            try {
                auto j          = nlohmann::json::parse(data);
                std::string cmd = j.value("cmd", "");
                if (!cmd.empty()) {
                    if (config_) {
                        const std::string token = config_->get<std::string>("api_token", "");
                        if (!isWsCommandAuthorizedForToken(token, j)) {
                            logger_->warn(TAG, "WS command rejected: unauthorized");
                            return;
                        }
                    }
                    std::lock_guard<std::mutex> lk(cmdMutex_);
                    if (commandCallback_) commandCallback_(cmd, j);
                }
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("bad WS message: ") + e.what());
            } });

        // Silence Crow's stdout chatter
        impl_->app.loglevel(crow::LogLevel::Warning);

        // Start Crow on its own async task and keep the returned handle alive for
        // the full server lifetime. Prefer the simple single-threaded run-path
        // here; on Alfred's Pi this has proven more robust than Crow's
        // multithreaded websocket runtime.
        crowFuture_ = std::async(std::launch::async, [this, port]()
                                 { impl_->app.bindaddr("0.0.0.0").port(static_cast<uint16_t>(port)).run(); });

        running_.store(true);
        logger_->info(TAG, "WebSocket server started on port " + std::to_string(port) + (webRoot_.empty() ? "" : " (serving " + webRoot_ + ")"));

        // Push thread: every 100 ms broadcast NMEA queue + latest telemetry.
        // BUG-005 fix: this is the ONLY thread that calls send_text() on connections.
        serverThread_ = std::thread([this]()
                                    {
        while (running_.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(PUSH_INTERVAL_MS));

            // Build telemetry message under lock
            std::string telMsg;
            {
                std::lock_guard<std::mutex> lk(telemetryMutex_);
                if (hasNewTelemetry_) {
                    telMsg = buildTelemetryJson(latestTelemetry_);
                    hasNewTelemetry_ = false;
                } else {
                    telMsg = R"({"type":"ping"})";
                }
            }

            // Drain NMEA queue
            std::vector<std::string> nmeaMsgs;
            {
                std::lock_guard<std::mutex> lk(nmeaMutex_);
                while (!nmeaQueue_.empty()) {
                    nmeaMsgs.push_back(std::move(nmeaQueue_.front()));
                    nmeaQueue_.pop();
                }
            }

            std::vector<std::string> logMsgs;
            {
                std::lock_guard<std::mutex> lk(logMutex_);
                while (!logQueue_.empty()) {
                    logMsgs.push_back(std::move(logQueue_.front()));
                    logQueue_.pop();
                }
            }

            // Single connection snapshot, then send everything without lock
            std::vector<crow::websocket::connection*> snap;
            {
                std::lock_guard<std::mutex> lk(impl_->connMutex);
                snap.assign(impl_->connections.begin(),
                            impl_->connections.end());
            }
            for (auto* c : snap) {
                for (const auto& nm : nmeaMsgs)
                    try { c->send_text(nm); } catch (...) {}
                for (const auto& lm : logMsgs)
                    try { c->send_text(lm); } catch (...) {}
                try { c->send_text(telMsg); }
                catch (...) { /* connection may have closed between snapshot and send */ }
            }
        }

        impl_->app.stop(); });
    }

    void WebSocketServer::stop()
    {
        if (!running_.load())
            return;
        running_.store(false);
        if (serverThread_.joinable())
            serverThread_.join();
        if (crowFuture_.valid())
            crowFuture_.wait();
    }

    // ── Data feed ─────────────────────────────────────────────────────────────────

    void WebSocketServer::pushTelemetry(const TelemetryData &data)
    {
        std::lock_guard<std::mutex> lk(telemetryMutex_);
        latestTelemetry_ = data;
        hasNewTelemetry_ = true;
    }

    void WebSocketServer::pushActivePlan(nlohmann::json planJson)
    {
        std::lock_guard<std::mutex> lk(activePlanMutex_);
        activePlanJson_ = std::move(planJson);
    }

    void WebSocketServer::pushLiveOverlay(nlohmann::json overlayJson)
    {
        std::lock_guard<std::mutex> lk(liveOverlayMutex_);
        liveOverlayJson_ = std::move(overlayJson);
    }

    void WebSocketServer::broadcastNmea(const std::string &line)
    {
        if (!running_.load())
            return;

        // BUG-005 fix: never call send_text() from the Robot thread.
        // Enqueue only — serverThread_ is the sole thread calling send_text().
        nlohmann::json j;
        j["type"] = "nmea";
        j["line"] = line;
        std::lock_guard<std::mutex> lk(nmeaMutex_);
        nmeaQueue_.push(j.dump());
    }

    void WebSocketServer::broadcastLog(const std::string &text)
    {
        if (!running_.load())
            return;

        nlohmann::json j;
        j["type"] = "log";
        j["text"] = text;
        std::lock_guard<std::mutex> lk(logMutex_);
        logQueue_.push(j.dump());
    }

    // ── JSON serialization ────────────────────────────────────────────────────────
    //
    // Format frozen to match sunray/mission_api.cpp:254-274.
    // Field order and key names must not change — the Python Mission Service and
    // index.html parse them directly.

    std::string WebSocketServer::buildTelemetryJson(const TelemetryData &d)
    {
        auto esc = [](const std::string &in) -> std::string
        {
            std::string out;
            out.reserve(in.size() + 8);
            for (unsigned char c : in)
            {
                switch (c)
                {
                case '\"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\b':
                    out += "\\b";
                    break;
                case '\f':
                    out += "\\f";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    if (c < 0x20)
                    {
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                        out += buf;
                    }
                    else
                    {
                        out += static_cast<char>(c);
                    }
                }
            }
            return out;
        };
        auto flt = [](float v, int prec = 4) -> std::string
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(prec) << v;
            return ss.str();
        };
        auto dbl = [](double v, int prec = 8) -> std::string
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(prec) << v;
            return ss.str();
        };
        auto bl = [](bool v) -> const char *
        { return v ? "true" : "false"; };

        std::string s;
        s.reserve(384);
        s += "{\"type\":\"state\"";
        s += ",\"op\":\"";
        s += esc(d.op);
        s += "\"";
        s += ",\"x\":";
        s += flt(d.x);
        s += ",\"y\":";
        s += flt(d.y);
        s += ",\"heading\":";
        s += flt(d.heading);
        s += ",\"battery_v\":";
        s += flt(d.battery_v, 2);
        s += ",\"charge_v\":";
        s += flt(d.charge_v, 2);
        s += ",\"charge_a\":";
        s += flt(d.charge_a, 2);
        s += ",\"charger_connected\":";
        s += bl(d.charger_connected);
        s += ",\"gps_sol\":";
        s += std::to_string(d.gps_sol);
        s += ",\"gps_text\":\"";
        s += esc(d.gps_text);
        s += "\"";
        s += ",\"gps_acc\":";
        s += flt(d.gps_acc, 3);
        s += ",\"gps_num_sv\":";
        s += std::to_string(d.gps_num_sv);
        s += ",\"gps_num_corr_signals\":";
        s += std::to_string(d.gps_num_corr_signals);
        s += ",\"gps_dgps_age_ms\":";
        s += std::to_string(d.gps_dgps_age_ms);
        s += ",\"gps_lat\":";
        s += dbl(d.gps_lat);
        s += ",\"gps_lon\":";
        s += dbl(d.gps_lon);
        s += ",\"bumper_l\":";
        s += bl(d.bumper_l);
        s += ",\"bumper_r\":";
        s += bl(d.bumper_r);
        s += ",\"stop_button\":";
        s += bl(d.stop_button);
        s += ",\"lift\":";
        s += bl(d.lift);
        s += ",\"motor_err\":";
        s += bl(d.motor_err);
        s += ",\"mow_fault_pin\":";
        s += bl(d.mow_fault_pin);
        s += ",\"mow_overload\":";
        s += bl(d.mow_overload);
        s += ",\"mow_permanent_fault\":";
        s += bl(d.mow_permanent_fault);
        s += ",\"mow_ov_check\":";
        s += bl(d.mow_ov_check);
        s += ",\"uptime_s\":";
        s += std::to_string(d.uptime_s);
        s += ",\"mcu_v\":\"";
        s += esc(d.mcu_version);
        s += "\"";
        s += ",\"pi_v\":\"";
        s += SUNRAY_VERSION;
        s += "\"";
        s += ",\"imu_h\":";
        s += flt(d.imu_heading, 1);
        s += ",\"imu_r\":";
        s += flt(d.imu_roll, 1);
        s += ",\"imu_p\":";
        s += flt(d.imu_pitch, 1);
        s += ",\"diag_active\":";
        s += bl(d.diag_active);
        s += ",\"diag_ticks\":";
        s += std::to_string(d.diag_ticks);
        s += ",\"ekf_health\":\"";
        s += esc(d.ekf_health);
        s += "\"";
        s += ",\"runtime_health\":\"";
        s += esc(d.runtime_health);
        s += "\"";
        s += ",\"mcu_connected\":";
        s += bl(d.mcu_connected);
        s += ",\"mcu_comm_loss\":";
        s += bl(d.mcu_comm_loss);
        s += ",\"gps_signal_lost\":";
        s += bl(d.gps_signal_lost);
        s += ",\"gps_fix_timeout\":";
        s += bl(d.gps_fix_timeout);
        s += ",\"battery_low\":";
        s += bl(d.battery_low);
        s += ",\"battery_critical\":";
        s += bl(d.battery_critical);
        s += ",\"recovery_active\":";
        s += bl(d.recovery_active);
        s += ",\"watchdog_event_active\":";
        s += bl(d.watchdog_event_active);
        s += ",\"ts_ms\":";
        s += std::to_string(d.ts_ms);
        s += ",\"state_since_ms\":";
        s += std::to_string(d.state_since_ms);
        s += ",\"state_phase\":\"";
        s += esc(d.state_phase);
        s += "\"";
        s += ",\"resume_target\":\"";
        s += esc(d.resume_target);
        s += "\"";
        s += ",\"event_reason\":\"";
        s += esc(d.event_reason);
        s += "\"";
        s += ",\"error_code\":\"";
        s += esc(d.error_code);
        s += "\"";
        s += ",\"ui_message\":\"";
        s += esc(d.ui_message);
        s += "\"";
        s += ",\"ui_severity\":\"";
        s += esc(d.ui_severity);
        s += "\"";
        s += ",\"history_backend_ready\":";
        s += bl(d.history_backend_ready);
        s += ",\"session_id\":\"";
        s += esc(d.session_id);
        s += "\"";
        s += ",\"session_started_at_ms\":";
        s += std::to_string(d.session_started_at_ms);
        s += ",\"mission_id\":\"";
        s += esc(d.mission_id);
        s += "\"";
        s += ",\"mission_zone_index\":";
        s += std::to_string(d.mission_zone_index);
        s += ",\"mission_zone_count\":";
        s += std::to_string(d.mission_zone_count);
        s += ",\"waypoint_index\":";
        s += std::to_string(d.waypoint_index);
        s += ",\"waypoint_total\":";
        s += std::to_string(d.waypoint_total);
        s += ",\"target_x\":";
        s += flt(d.target_x);
        s += ",\"target_y\":";
        s += flt(d.target_y);
        s += ",\"has_target\":";
        s += bl(d.has_target);
        s += ",\"has_interrupted_mission\":";
        s += bl(d.has_interrupted_mission);
        s += "}";
        return s;
    }

} // namespace sunray
