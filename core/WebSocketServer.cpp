// WebSocketServer.cpp — Crow WebSocket + HTTP server implementation.
//
// Crow is included only here (pimpl) — not in WebSocketServer.h.
// This keeps Crow out of all headers that include WebSocketServer.h.

#include "core/WebSocketServer.h"

#include "crow.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace sunray {

// ── Pimpl ─────────────────────────────────────────────────────────────────────

struct WebSocketServer::Impl {
    crow::SimpleApp app;

    std::unordered_set<crow::websocket::connection*> connections;
    std::mutex connMutex;
};

// ── Static helpers ─────────────────────────────────────────────────────────────

/// Return MIME type for a file extension.
static std::string mimeType(const std::string& ext) {
    static const std::unordered_map<std::string, std::string> map {
        { ".html", "text/html; charset=utf-8"  },
        { ".js",   "application/javascript"    },
        { ".css",  "text/css"                  },
        { ".svg",  "image/svg+xml"             },
        { ".ico",  "image/x-icon"              },
        { ".png",  "image/png"                 },
        { ".json", "application/json"          },
    };
    auto it = map.find(ext);
    return it != map.end() ? it->second : "application/octet-stream";
}

/// Read a file from disk and return its contents; empty string on failure.
static std::string readFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return { std::istreambuf_iterator<char>(f), {} };
}

/// Serve a file from webRoot, guarding against path traversal.
/// Returns 200+body on success, 404 on missing, 403 on traversal attempt.
static crow::response serveStatic(const std::filesystem::path& webRoot,
                                  const std::string&            relPath) {
    namespace fs = std::filesystem;
    const fs::path base    = fs::weakly_canonical(webRoot);
    const fs::path target  = fs::weakly_canonical(base / relPath);

    // Path traversal guard: resolved path must stay inside webRoot
    const auto [baseEnd, _] = std::mismatch(base.begin(),  base.end(),
                                             target.begin(), target.end());
    if (baseEnd != base.end()) {
        return crow::response(403);
    }

    std::string body = readFile(target);
    if (body.empty() && !fs::exists(target)) {
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
    : config_(std::move(config))
    , logger_(std::move(logger))
    , impl_  (std::make_unique<Impl>())
{}

WebSocketServer::~WebSocketServer() {
    stop();
}

// ── Configuration ─────────────────────────────────────────────────────────────

void WebSocketServer::setWebRoot(const std::string& distDir) {
    webRoot_ = distDir;
}

// ── Command registration ───────────────────────────────────────────────────────

void WebSocketServer::onCommand(CommandCallback cb) {
    std::lock_guard<std::mutex> lk(cmdMutex_);
    commandCallback_ = std::move(cb);
}

void WebSocketServer::onSimCommand(SimCommandCallback cb) {
    std::lock_guard<std::mutex> lk(simCmdMutex_);
    simCommandCallback_ = std::move(cb);
}

void WebSocketServer::setMapPath(const std::string& mapPath) {
    mapPath_ = mapPath;
}

void WebSocketServer::onMapReload(MapReloadCallback cb) {
    std::lock_guard<std::mutex> lk(mapReloadMutex_);
    mapReloadCallback_ = std::move(cb);
}

// ── HTTP route setup ──────────────────────────────────────────────────────────

void WebSocketServer::setupHttpRoutes() {
    auto& app = impl_->app;

    // ── Static file serving ────────────────────────────────────────────────────
    if (!webRoot_.empty()) {
        const std::string root = webRoot_;

        // GET / → index.html
        CROW_ROUTE(app, "/")([root]() {
            return serveStatic(root, "index.html");
        });

        // GET /assets/<path> → webRoot/assets/<path>
        CROW_ROUTE(app, "/assets/<path>")([root](const std::string& path) {
            return serveStatic(root, "assets/" + path);
        });
    }

    // ── GET /api/config ────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/config")(
        [this]() -> crow::response {
            if (!config_) return crow::response(503);
            crow::response res(200, config_->dump());
            res.set_header("Content-Type", "application/json");
            return res;
        }
    );

    // ── PUT /api/config ────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/config").methods(crow::HTTPMethod::PUT)(
        [this](const crow::request& req) -> crow::response {
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
            }
        }
    );

    // ── GET /api/map ───────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/map")(
        [this]() -> crow::response {
            if (mapPath_.empty()) return crow::response(404);
            std::string body = readFile(mapPath_);
            if (body.empty()) {
                // Return empty-but-valid map structure when file not yet created
                body = R"({"perimeter":[],"mow":[],"dock":[],"exclusions":[]})";
            }
            crow::response res(200, body);
            res.set_header("Content-Type", "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
            return res;
        }
    );

    // ── POST /api/map ──────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/map").methods(crow::HTTPMethod::POST)(
        [this](const crow::request& req) -> crow::response {
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
            }
        }
    );

    // ── GET /api/map/geojson — export current map as GeoJSON ──────────────────
    // Coordinates are WGS84 (EPSG:4326).  Origin stored in map.json under key
    // "origin": {"lat":…,"lon":…}.  If absent, lat0=lon0=0 (not georeferenced).
    CROW_ROUTE(app, "/api/map/geojson")(
        [this]() -> crow::response {
            if (mapPath_.empty()) return crow::response(503);
            const std::string raw = readFile(mapPath_);
            if (raw.empty()) return crow::response(404);
            try {
                const auto map = nlohmann::json::parse(raw);

                // ── origin ────────────────────────────────────────────────────
                double lat0 = 0.0, lon0 = 0.0;
                bool georef = false;
                if (map.contains("origin") && map["origin"].is_object()) {
                    lat0   = map["origin"].value("lat", 0.0);
                    lon0   = map["origin"].value("lon", 0.0);
                    georef = (lat0 != 0.0 || lon0 != 0.0);
                }

                // ── coordinate helpers ────────────────────────────────────────
                constexpr double R   = 6371000.0;
                constexpr double DEG = M_PI / 180.0;

                auto toWgs = [&](double x, double y)
                    -> std::pair<double,double>
                {
                    const double lat = lat0 + y / (R * DEG);
                    const double lon = lon0 + x / (R * std::cos(lat0 * DEG) * DEG);
                    return { lat, lon };
                };

                // local [[x,y],…] → closed GeoJSON ring [[lon,lat],…,[lon0,lat0]]
                auto toRing = [&](const nlohmann::json& pts) -> nlohmann::json {
                    auto ring = nlohmann::json::array();
                    for (const auto& p : pts) {
                        auto [lat, lon] = toWgs(p[0].get<double>(), p[1].get<double>());
                        ring.push_back({ lon, lat });
                    }
                    if (!ring.empty()) ring.push_back(ring[0]);  // close
                    return ring;
                };

                // ── build FeatureCollection ───────────────────────────────────
                nlohmann::json fc;
                fc["type"]         = "FeatureCollection";
                fc["georeferenced"] = georef;
                fc["origin"]       = { {"lat", lat0}, {"lon", lon0} };
                fc["features"]     = nlohmann::json::array();

                // perimeter
                if (map.contains("perimeter") && !map["perimeter"].empty()) {
                    fc["features"].push_back({
                        {"type", "Feature"},
                        {"properties", {{"type", "perimeter"}}},
                        {"geometry", {{"type","Polygon"},
                                      {"coordinates", nlohmann::json::array({ toRing(map["perimeter"]) })}}}
                    });
                }
                // exclusions
                if (map.contains("exclusions")) {
                    int idx = 0;
                    for (const auto& ex : map["exclusions"]) {
                        fc["features"].push_back({
                            {"type", "Feature"},
                            {"properties", {{"type","exclusion"},{"index",idx++}}},
                            {"geometry", {{"type","Polygon"},
                                          {"coordinates", nlohmann::json::array({ toRing(ex) })}}}
                        });
                    }
                }
                // dock (Point)
                if (map.contains("dock") && !map["dock"].empty()) {
                    const auto& d = map["dock"][0];
                    auto [lat, lon] = toWgs(d[0].get<double>(), d[1].get<double>());
                    fc["features"].push_back({
                        {"type", "Feature"},
                        {"properties", {{"type","dock"}}},
                        {"geometry", {{"type","Point"},{"coordinates",{lon,lat}}}}
                    });
                }
                // mow path (MultiPoint)
                if (map.contains("mow") && !map["mow"].empty()) {
                    auto coords = nlohmann::json::array();
                    for (const auto& p : map["mow"]) {
                        auto [lat, lon] = toWgs(p[0].get<double>(), p[1].get<double>());
                        coords.push_back({ lon, lat });
                    }
                    fc["features"].push_back({
                        {"type", "Feature"},
                        {"properties", {{"type","mow"}}},
                        {"geometry", {{"type","MultiPoint"},{"coordinates",coords}}}
                    });
                }

                crow::response res(200, fc.dump(2));
                res.set_header("Content-Type", "application/geo+json");
                res.set_header("Content-Disposition",
                               "attachment; filename=\"sunray-map.geojson\"");
                return res;
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("GET /api/map/geojson: ") + e.what());
                return crow::response(500);
            }
        }
    );

    // ── POST /api/map/geojson — import GeoJSON, convert to internal format ─────
    // Accepts a GeoJSON FeatureCollection.
    // Origin: taken from top-level "origin" field if present, otherwise computed
    // as the centroid of the "perimeter" feature.
    CROW_ROUTE(app, "/api/map/geojson").methods(crow::HTTPMethod::POST)(
        [this](const crow::request& req) -> crow::response {
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
                outMap["mow"]        = nlohmann::json::array();
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
                    } else if (ftype == "mow" && gtype == "MultiPoint") {
                        for (const auto& c : feat["geometry"]["coordinates"]) {
                            auto [x, y] = toLocal(c[0].get<double>(), c[1].get<double>());
                            outMap["mow"].push_back({ x, y });
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
                return crow::response(400,
                    std::string(R"({"error":")") + e.what() + "\"}");
            }
        }
    );

    // ── POST /api/sim/<action> ─────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/sim/<string>").methods(crow::HTTPMethod::POST)(
        [this](const crow::request& req, const std::string& action) -> crow::response {
            nlohmann::json params;
            if (!req.body.empty()) {
                try { params = nlohmann::json::parse(req.body); }
                catch (...) { return crow::response(400); }
            }
            {
                std::lock_guard<std::mutex> lk(simCmdMutex_);
                if (simCommandCallback_) simCommandCallback_(action, params);
            }
            return crow::response(200, R"({"ok":true})");
        }
    );
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void WebSocketServer::start() {
    if (running_.load()) return;

    const int port = config_ ? config_->get<int>("ws_port", 8765) : 8765;

    // Register HTTP routes first
    setupHttpRoutes();

    // Configure WebSocket route
    CROW_WEBSOCKET_ROUTE(impl_->app, "/ws/telemetry")
        .onopen([this](crow::websocket::connection& conn) {
            {
                std::lock_guard<std::mutex> lk(impl_->connMutex);
                impl_->connections.insert(&conn);
            }
            logger_->info(TAG, "WS client connected");
        })
        .onclose([this](crow::websocket::connection& conn, const std::string&) {
            {
                std::lock_guard<std::mutex> lk(impl_->connMutex);
                impl_->connections.erase(&conn);
            }
            logger_->info(TAG, "WS client disconnected");
        })
        .onmessage([this](crow::websocket::connection&,
                          const std::string& data, bool /*is_binary*/) {
            try {
                auto j          = nlohmann::json::parse(data);
                std::string cmd = j.value("cmd", "");
                if (!cmd.empty()) {
                    std::lock_guard<std::mutex> lk(cmdMutex_);
                    if (commandCallback_) commandCallback_(cmd, j);
                }
            } catch (const std::exception& e) {
                logger_->warn(TAG, std::string("bad WS message: ") + e.what());
            }
        });

    // Silence Crow's stdout chatter
    impl_->app.loglevel(crow::LogLevel::Warning);

    // Start Crow async (returns immediately; runs in its own thread pool)
    impl_->app.port(port).run_async();

    running_.store(true);
    logger_->info(TAG, "WebSocket server started on port " + std::to_string(port)
                       + (webRoot_.empty() ? "" : " (serving " + webRoot_ + ")"));

    // Push thread: every 100 ms broadcast latest telemetry to all clients
    serverThread_ = std::thread([this]() {
        while (running_.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(PUSH_INTERVAL_MS));

            // Build message under lock
            std::string msg;
            {
                std::lock_guard<std::mutex> lk(telemetryMutex_);
                if (hasNewTelemetry_) {
                    msg = buildTelemetryJson(latestTelemetry_);
                    hasNewTelemetry_ = false;
                } else {
                    msg = R"({"type":"ping"})";
                }
            }

            // Snapshot connection set under lock, then send without lock
            std::vector<crow::websocket::connection*> snap;
            {
                std::lock_guard<std::mutex> lk(impl_->connMutex);
                snap.assign(impl_->connections.begin(),
                            impl_->connections.end());
            }
            for (auto* c : snap) {
                try { c->send_text(msg); }
                catch (...) { /* connection may have closed between snapshot and send */ }
            }
        }

        impl_->app.stop();
    });
}

void WebSocketServer::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (serverThread_.joinable()) serverThread_.join();
}

// ── Data feed ─────────────────────────────────────────────────────────────────

void WebSocketServer::pushTelemetry(const TelemetryData& data) {
    std::lock_guard<std::mutex> lk(telemetryMutex_);
    latestTelemetry_  = data;
    hasNewTelemetry_  = true;
}

void WebSocketServer::broadcastNmea(const std::string& line) {
    if (!running_.load()) return;

    // Build JSON: {"type":"nmea","line":"<sentence>"}
    nlohmann::json j;
    j["type"] = "nmea";
    j["line"] = line;
    const std::string msg = j.dump();

    // Snapshot connections and broadcast
    std::vector<crow::websocket::connection*> snap;
    {
        std::lock_guard<std::mutex> lk(impl_->connMutex);
        snap.assign(impl_->connections.begin(), impl_->connections.end());
    }
    for (auto* c : snap) {
        try { c->send_text(msg); }
        catch (...) {}
    }
}

// ── JSON serialization ────────────────────────────────────────────────────────
//
// Format frozen to match sunray/mission_api.cpp:254-274.
// Field order and key names must not change — the Python Mission Service and
// index.html parse them directly.

std::string WebSocketServer::buildTelemetryJson(const TelemetryData& d) {
    auto flt = [](float v, int prec = 4) -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(prec) << v;
        return ss.str();
    };
    auto dbl = [](double v, int prec = 8) -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(prec) << v;
        return ss.str();
    };
    auto bl = [](bool v) -> const char* { return v ? "true" : "false"; };

    std::string s;
    s.reserve(256);
    s += "{\"type\":\"state\"";
    s += ",\"op\":\"";        s += d.op;        s += "\"";
    s += ",\"x\":";           s += flt(d.x);
    s += ",\"y\":";           s += flt(d.y);
    s += ",\"heading\":";     s += flt(d.heading);
    s += ",\"battery_v\":";   s += flt(d.battery_v, 2);
    s += ",\"charge_v\":";    s += flt(d.charge_v,  2);
    s += ",\"gps_sol\":";     s += std::to_string(d.gps_sol);
    s += ",\"gps_text\":\"";  s += d.gps_text;  s += "\"";
    s += ",\"gps_lat\":";     s += dbl(d.gps_lat);
    s += ",\"gps_lon\":";     s += dbl(d.gps_lon);
    s += ",\"bumper_l\":";    s += bl(d.bumper_l);
    s += ",\"bumper_r\":";    s += bl(d.bumper_r);
    s += ",\"motor_err\":";   s += bl(d.motor_err);
    s += ",\"uptime_s\":";    s += std::to_string(d.uptime_s);
    s += "}";
    return s;
}

} // namespace sunray
