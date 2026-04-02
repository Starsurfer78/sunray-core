#include "core/storage/HistoryDatabase.h"

#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>

#ifdef SUNRAY_HAVE_SQLITE
#include <sqlite3.h>
#endif

namespace sunray {

namespace {

constexpr int kSchemaVersion = 3;
constexpr unsigned kMaxQueryLimit = 500;

unsigned clampLimit(unsigned limit) {
    if (limit == 0) return 100;
    return limit > kMaxQueryLimit ? kMaxQueryLimit : limit;
}

} // namespace

bool HistoryDatabase::init(const Config& config, Logger& logger) {
    std::lock_guard<std::mutex> lk(mutex_);
    enabled_ = config.get<bool>("history_db_enabled", true);
    path_ = config.get<std::string>("history_db_path", "/var/lib/sunray/history.db");
    maxEvents_ = config.get<unsigned>("history_db_max_events", 20000);
    maxSessions_ = config.get<unsigned>("history_db_max_sessions", 2000);
    exportEnabled_ = config.get<bool>("history_db_export_enabled", true);
    if (!enabled_) return initDisabled(logger);
    return initSqlite(config, logger);
}

bool HistoryDatabase::appendEvent(const EventRecord& event, Logger& logger) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!enabled_ || !available_) return false;

#ifndef SUNRAY_HAVE_SQLITE
    (void)event;
    (void)logger;
    return false;
#else
    void* dbHandle = nullptr;
    if (!openDatabase(&dbHandle, logger)) {
        return false;
    }
    auto* db = static_cast<sqlite3*>(dbHandle);

    constexpr const char* sql =
        "INSERT INTO events("
        "ts_ms, wall_ts_ms, level, module, event_type, state_phase, event_reason, error_code, message,"
        "battery_v, gps_sol, x, y"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    const int prepRc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (prepRc != SQLITE_OK) {
        logger.error("HistoryDatabase",
                     "failed to prepare event insert: " + std::string(sqlite3_errmsg(db)));
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(event.ts_ms));
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(event.wall_ts_ms));
    sqlite3_bind_text(stmt, 3, event.level.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, event.module.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, event.eventType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, event.statePhase.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, event.eventReason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, event.errorCode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, event.message.c_str(), -1, SQLITE_TRANSIENT);

    if (event.batteryV.has_value()) sqlite3_bind_double(stmt, 10, *event.batteryV);
    else sqlite3_bind_null(stmt, 10);

    if (event.gpsSol.has_value()) sqlite3_bind_int(stmt, 11, *event.gpsSol);
    else sqlite3_bind_null(stmt, 11);

    if (event.x.has_value()) sqlite3_bind_double(stmt, 12, *event.x);
    else sqlite3_bind_null(stmt, 12);

    if (event.y.has_value()) sqlite3_bind_double(stmt, 13, *event.y);
    else sqlite3_bind_null(stmt, 13);

    const int stepRc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (stepRc != SQLITE_DONE) {
        logger.error("HistoryDatabase",
                     "failed to insert event: " + std::string(sqlite3_errmsg(db)));
        sqlite3_close(db);
        return false;
    }

    if (!applyRetention(db, logger)) {
        sqlite3_close(db);
        return false;
    }
    sqlite3_close(db);
    return true;
#endif
}

bool HistoryDatabase::upsertSession(const SessionRecord& session, Logger& logger) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!enabled_ || !available_) return false;

#ifndef SUNRAY_HAVE_SQLITE
    (void)session;
    (void)logger;
    return false;
#else
    void* dbHandle = nullptr;
    if (!openDatabase(&dbHandle, logger)) {
        return false;
    }
    auto* db = static_cast<sqlite3*>(dbHandle);

    constexpr const char* sql =
        "INSERT INTO sessions("
        "id, started_at_ms, ended_at_ms, duration_ms, distance_m, battery_start_v, battery_end_v,"
        "end_reason, mean_gps_accuracy_m, max_gps_accuracy_m, metadata_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        " ON CONFLICT(id) DO UPDATE SET "
        "ended_at_ms=excluded.ended_at_ms, "
        "duration_ms=excluded.duration_ms, "
        "distance_m=excluded.distance_m, "
        "battery_end_v=excluded.battery_end_v, "
        "end_reason=excluded.end_reason, "
        "mean_gps_accuracy_m=excluded.mean_gps_accuracy_m, "
        "max_gps_accuracy_m=excluded.max_gps_accuracy_m, "
        "metadata_json=excluded.metadata_json;";

    sqlite3_stmt* stmt = nullptr;
    const int prepRc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (prepRc != SQLITE_OK) {
        logger.error("HistoryDatabase",
                     "failed to prepare session upsert: " + std::string(sqlite3_errmsg(db)));
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, session.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(session.startedAtMs));
    if (session.endedAtMs > 0) sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(session.endedAtMs));
    else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(session.durationMs));
    sqlite3_bind_double(stmt, 5, session.distanceM);
    sqlite3_bind_double(stmt, 6, session.batteryStartV);
    sqlite3_bind_double(stmt, 7, session.batteryEndV);
    sqlite3_bind_text(stmt, 8, session.endReason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 9, session.meanGpsAccuracyM);
    sqlite3_bind_double(stmt, 10, session.maxGpsAccuracyM);
    sqlite3_bind_text(stmt, 11, session.metadataJson.c_str(), -1, SQLITE_TRANSIENT);

    const int stepRc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (stepRc != SQLITE_DONE) {
        logger.error("HistoryDatabase",
                     "failed to upsert session: " + std::string(sqlite3_errmsg(db)));
        sqlite3_close(db);
        return false;
    }

    if (!applyRetention(db, logger)) {
        sqlite3_close(db);
        return false;
    }
    sqlite3_close(db);
    return true;
#endif
}

nlohmann::json HistoryDatabase::listEvents(unsigned limit, Logger& logger) const {
    std::lock_guard<std::mutex> lk(mutex_);

    nlohmann::json result = {
        {"ok", true},
        {"backend_ready", enabled_ && available_},
        {"items", nlohmann::json::array()},
        {"limit", clampLimit(limit)},
    };

    if (!enabled_ || !available_) {
        return result;
    }

#ifndef SUNRAY_HAVE_SQLITE
    (void)logger;
    return result;
#else
    void* dbHandle = nullptr;
    if (!openDatabase(&dbHandle, logger)) {
        return {{"ok", false}, {"backend_ready", false}, {"items", nlohmann::json::array()}};
    }
    auto* db = static_cast<sqlite3*>(dbHandle);

    constexpr const char* sql =
        "SELECT ts_ms, wall_ts_ms, level, module, event_type, state_phase, event_reason, error_code, message,"
        " battery_v, gps_sol, x, y"
        " FROM events"
        " ORDER BY ts_ms DESC"
        " LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    const int prepRc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (prepRc != SQLITE_OK) {
        logger.error("HistoryDatabase",
                     "failed to prepare event query: " + std::string(sqlite3_errmsg(db)));
        sqlite3_close(db);
        return {{"ok", false}, {"backend_ready", true}, {"items", nlohmann::json::array()}};
    }

    sqlite3_bind_int(stmt, 1, static_cast<int>(clampLimit(limit)));
    auto& items = result["items"];
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        nlohmann::json row = {
            {"ts_ms", static_cast<long long>(sqlite3_column_int64(stmt, 0))},
            {"wall_ts_ms", static_cast<long long>(sqlite3_column_int64(stmt, 1))},
            {"level", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2) ?: reinterpret_cast<const unsigned char*>(""))},
            {"module", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3) ?: reinterpret_cast<const unsigned char*>(""))},
            {"event_type", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4) ?: reinterpret_cast<const unsigned char*>(""))},
            {"state_phase", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5) ?: reinterpret_cast<const unsigned char*>(""))},
            {"event_reason", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6) ?: reinterpret_cast<const unsigned char*>(""))},
            {"error_code", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7) ?: reinterpret_cast<const unsigned char*>(""))},
            {"message", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8) ?: reinterpret_cast<const unsigned char*>(""))},
        };
        if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) row["battery_v"] = sqlite3_column_double(stmt, 9);
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) row["gps_sol"] = sqlite3_column_int(stmt, 10);
        if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) row["x"] = sqlite3_column_double(stmt, 11);
        if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) row["y"] = sqlite3_column_double(stmt, 12);
        items.push_back(std::move(row));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
#endif
}

nlohmann::json HistoryDatabase::listSessions(unsigned limit, Logger& logger) const {
    std::lock_guard<std::mutex> lk(mutex_);

    nlohmann::json result = {
        {"ok", true},
        {"backend_ready", enabled_ && available_},
        {"items", nlohmann::json::array()},
        {"limit", clampLimit(limit)},
    };

    if (!enabled_ || !available_) {
        return result;
    }

#ifndef SUNRAY_HAVE_SQLITE
    (void)logger;
    return result;
#else
    void* dbHandle = nullptr;
    if (!openDatabase(&dbHandle, logger)) {
        return {{"ok", false}, {"backend_ready", false}, {"items", nlohmann::json::array()}};
    }
    auto* db = static_cast<sqlite3*>(dbHandle);

    constexpr const char* sql =
        "SELECT id, started_at_ms, ended_at_ms, duration_ms, distance_m, battery_start_v, battery_end_v,"
        " end_reason, mean_gps_accuracy_m, max_gps_accuracy_m, metadata_json"
        " FROM sessions"
        " ORDER BY started_at_ms DESC"
        " LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    const int prepRc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (prepRc != SQLITE_OK) {
        logger.error("HistoryDatabase",
                     "failed to prepare session query: " + std::string(sqlite3_errmsg(db)));
        sqlite3_close(db);
        return {{"ok", false}, {"backend_ready", true}, {"items", nlohmann::json::array()}};
    }

    sqlite3_bind_int(stmt, 1, static_cast<int>(clampLimit(limit)));
    auto& items = result["items"];
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        nlohmann::json row = {
            {"id", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0) ?: reinterpret_cast<const unsigned char*>(""))},
            {"started_at_ms", static_cast<long long>(sqlite3_column_int64(stmt, 1))},
            {"duration_ms", static_cast<long long>(sqlite3_column_int64(stmt, 3))},
            {"distance_m", sqlite3_column_double(stmt, 4)},
            {"battery_start_v", sqlite3_column_double(stmt, 5)},
            {"battery_end_v", sqlite3_column_double(stmt, 6)},
            {"end_reason", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7) ?: reinterpret_cast<const unsigned char*>(""))},
        };
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) row["ended_at_ms"] = static_cast<long long>(sqlite3_column_int64(stmt, 2));
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) row["mean_gps_accuracy_m"] = sqlite3_column_double(stmt, 8);
        if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) row["max_gps_accuracy_m"] = sqlite3_column_double(stmt, 9);
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            const auto* txt = sqlite3_column_text(stmt, 10);
            try {
                row["metadata"] = txt ? nlohmann::json::parse(reinterpret_cast<const char*>(txt)) : nlohmann::json::object();
            } catch (...) {
                row["metadata"] = nlohmann::json{{"raw", txt ? reinterpret_cast<const char*>(txt) : ""}};
            }
        }
        items.push_back(std::move(row));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
#endif
}

nlohmann::json HistoryDatabase::buildSummary(Logger& logger) const {
    std::lock_guard<std::mutex> lk(mutex_);

    nlohmann::json result = {
        {"ok", true},
        {"backend_ready", enabled_ && available_},
        {"events_total", 0},
        {"event_reason_counts", nlohmann::json::object()},
        {"event_type_counts", nlohmann::json::object()},
        {"event_level_counts", nlohmann::json::object()},
        {"sessions_total", 0},
        {"sessions_completed", 0},
        {"mowing_duration_ms_total", 0},
        {"mowing_distance_m_total", 0.0},
        {"last_event_ts_ms", 0},
        {"last_event_wall_ts_ms", 0},
        {"last_session_started_at_ms", 0},
        {"retention", {
            {"max_events", maxEvents_},
            {"max_sessions", maxSessions_},
        }},
        {"export_enabled", exportEnabled_},
        {"database_path", path_.string()},
        {"database_exists", std::filesystem::exists(path_)},
    };

    if (!enabled_ || !available_) {
        return result;
    }

#ifndef SUNRAY_HAVE_SQLITE
    (void)logger;
    return result;
#else
    void* dbHandle = nullptr;
    if (!openDatabase(&dbHandle, logger)) {
        result["ok"] = false;
        result["backend_ready"] = false;
        return result;
    }
    auto* db = static_cast<sqlite3*>(dbHandle);

    auto queryScalar = [&](const char* sql, const char* key, bool integer = true) {
        sqlite3_stmt* stmt = nullptr;
        const int prepRc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (prepRc != SQLITE_OK) {
            logger.error("HistoryDatabase",
                         "failed to prepare summary query: " + std::string(sqlite3_errmsg(db)));
            return;
        }
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            if (integer) result[key] = static_cast<long long>(sqlite3_column_int64(stmt, 0));
            else result[key] = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    };

    queryScalar("SELECT COUNT(*) FROM events;", "events_total");
    queryScalar("SELECT COUNT(*) FROM sessions;", "sessions_total");
    queryScalar("SELECT COUNT(*) FROM sessions WHERE ended_at_ms IS NOT NULL;", "sessions_completed");
    queryScalar("SELECT COALESCE(SUM(duration_ms), 0) FROM sessions;", "mowing_duration_ms_total");
    queryScalar("SELECT COALESCE(SUM(distance_m), 0) FROM sessions;", "mowing_distance_m_total", false);
    queryScalar("SELECT COALESCE(MAX(ts_ms), 0) FROM events;", "last_event_ts_ms");
    queryScalar("SELECT COALESCE(MAX(wall_ts_ms), 0) FROM events;", "last_event_wall_ts_ms");
    queryScalar("SELECT COALESCE(MAX(started_at_ms), 0) FROM sessions;", "last_session_started_at_ms");

    auto queryGroupedCounts = [&](const char* sql, const char* key) {
        sqlite3_stmt* stmt = nullptr;
        const int prepRc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (prepRc != SQLITE_OK) {
            logger.error("HistoryDatabase",
                         "failed to prepare grouped summary query: " + std::string(sqlite3_errmsg(db)));
            return;
        }
        auto counts = nlohmann::json::object();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto* rawKey = sqlite3_column_text(stmt, 0);
            if (!rawKey) continue;
            counts[reinterpret_cast<const char*>(rawKey)] =
                static_cast<long long>(sqlite3_column_int64(stmt, 1));
        }
        sqlite3_finalize(stmt);
        result[key] = std::move(counts);
    };

    queryGroupedCounts(
        "SELECT event_reason, COUNT(*) FROM events "
        "WHERE event_reason IS NOT NULL AND event_reason != '' "
        "GROUP BY event_reason;",
        "event_reason_counts");
    queryGroupedCounts(
        "SELECT event_type, COUNT(*) FROM events "
        "WHERE event_type IS NOT NULL AND event_type != '' "
        "GROUP BY event_type;",
        "event_type_counts");
    queryGroupedCounts(
        "SELECT level, COUNT(*) FROM events "
        "WHERE level IS NOT NULL AND level != '' "
        "GROUP BY level;",
        "event_level_counts");

    sqlite3_close(db);
    return result;
#endif
}

bool HistoryDatabase::initDisabled(Logger& logger) {
    available_ = false;
    logger.info("HistoryDatabase", "history database disabled by config");
    return true;
}

bool HistoryDatabase::initSqlite(const Config&, Logger& logger) {
#ifndef SUNRAY_HAVE_SQLITE
    available_ = false;
    logger.warn("HistoryDatabase",
                "SQLite support not built in; history database unavailable");
    return true;
#else
    std::error_code ec;
    if (const auto parent = path_.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            logger.error("HistoryDatabase",
                         "failed to create directory '" + parent.string() + "': " + ec.message());
            return false;
        }
    }

    sqlite3* db = nullptr;
    const int rc = sqlite3_open(path_.c_str(), &db);
    if (rc != SQLITE_OK || db == nullptr) {
        std::string msg = db ? sqlite3_errmsg(db) : "sqlite3_open failed";
        if (db) sqlite3_close(db);
        logger.error("HistoryDatabase", "failed to open '" + path_.string() + "': " + msg);
        return false;
    }

    const auto schemaVersion = readSchemaVersion(db, logger);
    if (schemaVersion.has_value() && *schemaVersion != kSchemaVersion) {
        if (!backupExistingDatabase(logger)) {
            sqlite3_close(db);
            return false;
        }
    }

    if (!schemaVersion.has_value() && std::filesystem::exists(path_)
        && std::filesystem::file_size(path_, ec) > 0) {
        if (!backupExistingDatabase(logger)) {
            sqlite3_close(db);
            return false;
        }
    }

    if (!bootstrapSchema(db, logger)) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_close(db);
    available_ = true;

    std::ostringstream msg;
    msg << "history database ready at " << path_.string()
        << " (schema_version=" << kSchemaVersion << ")";
    logger.info("HistoryDatabase", msg.str());
    return true;
#endif
}

bool HistoryDatabase::openDatabase(void** dbHandle, Logger& logger) const {
#ifndef SUNRAY_HAVE_SQLITE
    (void)dbHandle;
    (void)logger;
    return false;
#else
    sqlite3* db = nullptr;
    const int rc = sqlite3_open(path_.c_str(), &db);
    if (rc != SQLITE_OK || db == nullptr) {
        std::string msg = db ? sqlite3_errmsg(db) : "sqlite3_open failed";
        if (db) sqlite3_close(db);
        logger.error("HistoryDatabase", "failed to open history DB: " + msg);
        return false;
    }
    *dbHandle = db;
    return true;
#endif
}

bool HistoryDatabase::applyRetention(void* dbHandle, Logger& logger) const {
#ifndef SUNRAY_HAVE_SQLITE
    (void)dbHandle;
    (void)logger;
    return false;
#else
    auto* db = static_cast<sqlite3*>(dbHandle);
    auto runDelete = [&](const std::string& sql, const std::string& label) -> bool {
        char* err = nullptr;
        const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            const std::string msg = err ? err : "sqlite3_exec failed";
            if (err) sqlite3_free(err);
            logger.error("HistoryDatabase", "failed to prune " + label + ": " + msg);
            return false;
        }
        return true;
    };

    if (maxEvents_ > 0) {
        const std::string sql =
            "DELETE FROM events WHERE id NOT IN ("
            "SELECT id FROM events ORDER BY ts_ms DESC LIMIT " + std::to_string(maxEvents_) +
            ");";
        if (!runDelete(sql, "events")) return false;
    }

    if (maxSessions_ > 0) {
        const std::string sql =
            "DELETE FROM sessions WHERE id NOT IN ("
            "SELECT id FROM sessions ORDER BY started_at_ms DESC LIMIT " + std::to_string(maxSessions_) +
            ");";
        if (!runDelete(sql, "sessions")) return false;
    }
    return true;
#endif
}

bool HistoryDatabase::bootstrapSchema(void* dbHandle, Logger& logger) {
#ifdef SUNRAY_HAVE_SQLITE
    auto* db = static_cast<sqlite3*>(dbHandle);
    std::ostringstream sql;
    sql
        << "PRAGMA journal_mode=WAL;"
        << "PRAGMA foreign_keys=ON;"
        << "CREATE TABLE IF NOT EXISTS schema_meta ("
        << "  key TEXT PRIMARY KEY,"
        << "  value TEXT NOT NULL"
        << ");"
        << "CREATE TABLE IF NOT EXISTS events ("
        << "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        << "  ts_ms INTEGER NOT NULL,"
        << "  wall_ts_ms INTEGER NOT NULL DEFAULT 0,"
        << "  level TEXT NOT NULL DEFAULT '',"
        << "  module TEXT NOT NULL DEFAULT '',"
        << "  event_type TEXT NOT NULL,"
        << "  state_phase TEXT NOT NULL DEFAULT '',"
        << "  event_reason TEXT NOT NULL DEFAULT '',"
        << "  error_code TEXT NOT NULL DEFAULT '',"
        << "  message TEXT NOT NULL DEFAULT '',"
        << "  battery_v REAL,"
        << "  gps_sol INTEGER,"
        << "  x REAL,"
        << "  y REAL"
        << ");"
        << "CREATE INDEX IF NOT EXISTS idx_events_ts_ms ON events(ts_ms DESC);"
        << "CREATE INDEX IF NOT EXISTS idx_events_event_type ON events(event_type);"
        << "CREATE INDEX IF NOT EXISTS idx_events_state_phase ON events(state_phase);"
        << "CREATE TABLE IF NOT EXISTS sessions ("
        << "  id TEXT PRIMARY KEY,"
        << "  started_at_ms INTEGER NOT NULL,"
        << "  ended_at_ms INTEGER,"
        << "  duration_ms INTEGER NOT NULL DEFAULT 0,"
        << "  distance_m REAL NOT NULL DEFAULT 0,"
        << "  battery_start_v REAL NOT NULL DEFAULT 0,"
        << "  battery_end_v REAL NOT NULL DEFAULT 0,"
        << "  end_reason TEXT NOT NULL DEFAULT '',"
        << "  mean_gps_accuracy_m REAL,"
        << "  max_gps_accuracy_m REAL,"
        << "  metadata_json TEXT NOT NULL DEFAULT '{}'"
        << ");"
        << "CREATE INDEX IF NOT EXISTS idx_sessions_started_at_ms "
        << "  ON sessions(started_at_ms DESC);"
        << "INSERT INTO schema_meta(key, value) VALUES('schema_version', '"
        << kSchemaVersion
        << "') ON CONFLICT(key) DO UPDATE SET value=excluded.value;";

    char* err = nullptr;
    const int sqlRc = sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, &err);
    if (sqlRc != SQLITE_OK) {
        std::string msg = err ? err : "sqlite3_exec failed";
        if (err) sqlite3_free(err);
        logger.error("HistoryDatabase", "failed to bootstrap schema: " + msg);
        return false;
    }
    return true;
#else
    (void)dbHandle;
    (void)logger;
    return false;
#endif
}

std::optional<int> HistoryDatabase::readSchemaVersion(void* dbHandle, Logger& logger) const {
#ifdef SUNRAY_HAVE_SQLITE
    auto* db = static_cast<sqlite3*>(dbHandle);
    const char* sql =
        "SELECT value FROM schema_meta WHERE key='schema_version' LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    const int prepRc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (prepRc != SQLITE_OK) {
        const std::string msg = sqlite3_errmsg(db);
        if (msg.find("no such table") != std::string::npos) {
            return std::nullopt;
        }
        logger.warn("HistoryDatabase",
                    "could not read schema version: " + msg);
        return std::nullopt;
    }

    std::optional<int> result;
    const int stepRc = sqlite3_step(stmt);
    if (stepRc == SQLITE_ROW) {
        const unsigned char* txt = sqlite3_column_text(stmt, 0);
        if (txt != nullptr) {
            result = std::stoi(reinterpret_cast<const char*>(txt));
        }
    }
    sqlite3_finalize(stmt);
    return result;
#else
    (void)dbHandle;
    (void)logger;
    return std::nullopt;
#endif
}

bool HistoryDatabase::backupExistingDatabase(Logger& logger) const {
    if (!std::filesystem::exists(path_)) return true;

    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &nowTime);
#else
    localtime_r(&nowTime, &tm);
#endif

    std::ostringstream suffix;
    suffix << std::put_time(&tm, "%Y%m%d-%H%M%S");
    const auto backupPath = path_.string() + ".bak-" + suffix.str();

    std::error_code ec;
    std::filesystem::copy_file(
        path_, backupPath,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        logger.error("HistoryDatabase",
                     "failed to create DB backup '" + backupPath + "': " + ec.message());
        return false;
    }

    logger.warn("HistoryDatabase",
                "created DB backup before schema update: " + backupPath);
    return true;
}

} // namespace sunray
