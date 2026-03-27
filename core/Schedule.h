#pragma once
// Schedule.h — Weekly mowing timetable (C.11).
//
// Each entry covers one day-of-week slot: {wday, start_hhmm, end_hhmm, enabled}.
// The schedule is persisted as JSON (schedule.json next to config.json).
// Robot::run() calls checkNow() every minute to fire timetable events.
//
// Time is compared against localtime() — the Pi system clock must be correct.

#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace sunray {

struct ScheduleEntry {
    int  wday       = 0;      ///< 0=Sun … 6=Sat (matches tm_wday)
    int  start_hhmm = 800;    ///< e.g. 800 = 08:00
    int  end_hhmm   = 1200;   ///< e.g. 1200 = 12:00
    bool enabled    = true;
    int  rain_delay_min = 0;  ///< extra delay after rain event (minutes)
};

class Schedule {
public:
    explicit Schedule() = default;

    /// Load from JSON file. Returns true on success; on failure, clears entries.
    bool load(const std::filesystem::path& path);

    /// Save to JSON file.
    bool save(const std::filesystem::path& path) const;

    /// Replace all entries from a JSON array.
    bool fromJson(const nlohmann::json& arr);

    /// Serialise to JSON array.
    nlohmann::json toJson() const;

    /// Returns true if mowing should be ACTIVE at the given local time.
    /// wday: 0=Sun…6=Sat, hhmm: e.g. 830 for 08:30.
    bool isActive(int wday, int hhmm) const;

    /// Convenience: check against current system localtime().
    bool isActiveNow() const;

    const std::vector<ScheduleEntry>& entries() const { return entries_; }
    void setEntries(std::vector<ScheduleEntry> e) { entries_ = std::move(e); }

private:
    std::vector<ScheduleEntry> entries_;
};

} // namespace sunray
