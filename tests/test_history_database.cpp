#include "core/Config.h"
#include "core/Logger.h"
#include "core/storage/EventRecord.h"
#include "core/storage/HistoryDatabase.h"
#include "core/storage/SessionRecord.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace fs = std::filesystem;

namespace {

fs::path uniqueTestPath(const char* stem, const char* ext) {
    const auto base = fs::temp_directory_path()
        / (std::string("sunray-history-") + stem + ext);
    std::error_code ec;
    fs::remove(base, ec);
    fs::remove(base.string() + "-wal", ec);
    fs::remove(base.string() + "-shm", ec);
    return base;
}

#ifdef SUNRAY_HAVE_SQLITE
sunray::EventRecord makeEvent(long long ts, const std::string& type, const std::string& reason) {
    sunray::EventRecord event;
    event.ts_ms = ts;
    event.level = "info";
    event.module = "test";
    event.eventType = type;
    event.statePhase = "mowing";
    event.eventReason = reason;
    event.message = "message-" + reason;
    event.batteryV = 26.4f;
    event.gpsSol = 4;
    event.x = 1.0f;
    event.y = 2.0f;
    return event;
}

sunray::SessionRecord makeSession(const std::string& id, long long started, long long ended) {
    sunray::SessionRecord session;
    session.id = id;
    session.startedAtMs = started;
    session.endedAtMs = ended;
    session.durationMs = ended - started;
    session.distanceM = 12.5f;
    session.batteryStartV = 28.1f;
    session.batteryEndV = 26.7f;
    session.endReason = "battery_low";
    session.meanGpsAccuracyM = 0.04f;
    session.maxGpsAccuracyM = 0.08f;
    session.metadataJson = R"({"source":"test","state_phase":"mowing"})";
    return session;
}
#endif

} // namespace

TEST_CASE("HistoryDatabase: list and summary APIs expose backend data", "[history]") {
    const fs::path configPath = uniqueTestPath("config-basic", ".json");
    const fs::path dbPath = uniqueTestPath("basic", ".db");

    sunray::Config config(configPath);
    config.set("history_db_enabled", true);
    config.set("history_db_path", dbPath.string());
    config.set("history_db_max_events", 10);
    config.set("history_db_max_sessions", 10);

    sunray::NullLogger logger;
    sunray::HistoryDatabase db;

    REQUIRE(db.init(config, logger));

#ifdef SUNRAY_HAVE_SQLITE
    REQUIRE(db.available());
    REQUIRE(db.appendEvent(makeEvent(1000, "state_transition", "mission_active"), logger));
    REQUIRE(db.upsertSession(makeSession("session-1", 2000, 8000), logger));

    const auto events = db.listEvents(20, logger);
    REQUIRE(events["ok"] == true);
    REQUIRE(events["backend_ready"] == true);
    REQUIRE(events["items"].size() == 1);
    REQUIRE(events["items"][0]["event_type"] == "state_transition");

    const auto sessions = db.listSessions(20, logger);
    REQUIRE(sessions["ok"] == true);
    REQUIRE(sessions["backend_ready"] == true);
    REQUIRE(sessions["items"].size() == 1);
    REQUIRE(sessions["items"][0]["id"] == "session-1");
    REQUIRE(sessions["items"][0]["metadata"]["source"] == "test");

    const auto summary = db.buildSummary(logger);
    REQUIRE(summary["ok"] == true);
    REQUIRE(summary["backend_ready"] == true);
    REQUIRE(summary["events_total"] == 1);
    REQUIRE(summary["sessions_total"] == 1);
    REQUIRE(summary["sessions_completed"] == 1);
    REQUIRE(summary["retention"]["max_events"] == 10);
    REQUIRE(summary["retention"]["max_sessions"] == 10);
    REQUIRE(summary["database_path"] == dbPath.string());
#else
    REQUIRE_FALSE(db.available());
    const auto events = db.listEvents(20, logger);
    const auto sessions = db.listSessions(20, logger);
    const auto summary = db.buildSummary(logger);
    REQUIRE(events["backend_ready"] == false);
    REQUIRE(sessions["backend_ready"] == false);
    REQUIRE(summary["backend_ready"] == false);
    REQUIRE(events["items"].empty());
    REQUIRE(sessions["items"].empty());
#endif

    std::error_code ec;
    fs::remove(configPath, ec);
    fs::remove(dbPath, ec);
    fs::remove(dbPath.string() + "-wal", ec);
    fs::remove(dbPath.string() + "-shm", ec);
}

TEST_CASE("HistoryDatabase: retention limits trim events and sessions", "[history]") {
    const fs::path configPath = uniqueTestPath("config-retention", ".json");
    const fs::path dbPath = uniqueTestPath("retention", ".db");

    sunray::Config config(configPath);
    config.set("history_db_enabled", true);
    config.set("history_db_path", dbPath.string());
    config.set("history_db_max_events", 2);
    config.set("history_db_max_sessions", 1);

    sunray::NullLogger logger;
    sunray::HistoryDatabase db;

    REQUIRE(db.init(config, logger));

#ifdef SUNRAY_HAVE_SQLITE
    REQUIRE(db.appendEvent(makeEvent(1000, "state_transition", "first"), logger));
    REQUIRE(db.appendEvent(makeEvent(2000, "state_transition", "second"), logger));
    REQUIRE(db.appendEvent(makeEvent(3000, "state_transition", "third"), logger));

    REQUIRE(db.upsertSession(makeSession("session-a", 1000, 5000), logger));
    REQUIRE(db.upsertSession(makeSession("session-b", 6000, 12000), logger));

    const auto events = db.listEvents(50, logger);
    REQUIRE(events["items"].size() == 2);
    REQUIRE(events["items"][0]["event_reason"] == "third");
    REQUIRE(events["items"][1]["event_reason"] == "second");

    const auto sessions = db.listSessions(50, logger);
    REQUIRE(sessions["items"].size() == 1);
    REQUIRE(sessions["items"][0]["id"] == "session-b");

    const auto summary = db.buildSummary(logger);
    REQUIRE(summary["events_total"] == 2);
    REQUIRE(summary["sessions_total"] == 1);
#else
    REQUIRE(db.listEvents(50, logger)["items"].empty());
    REQUIRE(db.listSessions(50, logger)["items"].empty());
#endif

    std::error_code ec;
    fs::remove(configPath, ec);
    fs::remove(dbPath, ec);
    fs::remove(dbPath.string() + "-wal", ec);
    fs::remove(dbPath.string() + "-shm", ec);
}
