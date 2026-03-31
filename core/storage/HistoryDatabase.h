#pragma once

#include "core/Config.h"
#include "core/Logger.h"
#include "core/storage/EventRecord.h"
#include "core/storage/SessionRecord.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace sunray {

class HistoryDatabase {
public:
    bool init(const Config& config, Logger& logger);
    bool appendEvent(const EventRecord& event, Logger& logger);
    bool upsertSession(const SessionRecord& session, Logger& logger);
    nlohmann::json listEvents(unsigned limit, Logger& logger) const;
    nlohmann::json listSessions(unsigned limit, Logger& logger) const;
    nlohmann::json buildSummary(Logger& logger) const;

    bool enabled() const { return enabled_; }
    bool available() const { return available_; }
    const std::filesystem::path& path() const { return path_; }
    unsigned maxEvents() const { return maxEvents_; }
    unsigned maxSessions() const { return maxSessions_; }
    bool exportEnabled() const { return exportEnabled_; }

private:
    bool initDisabled(Logger& logger);
    bool initSqlite(const Config& config, Logger& logger);
    bool openDatabase(void** dbHandle, Logger& logger) const;
    bool bootstrapSchema(void* dbHandle, Logger& logger);
    bool applyRetention(void* dbHandle, Logger& logger) const;
    std::optional<int> readSchemaVersion(void* dbHandle, Logger& logger) const;
    bool backupExistingDatabase(Logger& logger) const;

    bool                  enabled_   = false;
    bool                  available_ = false;
    bool                  exportEnabled_ = true;
    unsigned              maxEvents_ = 20000;
    unsigned              maxSessions_ = 2000;
    std::filesystem::path path_;
    mutable std::mutex    mutex_;
};

} // namespace sunray
