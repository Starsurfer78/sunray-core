// Schedule.cpp — Weekly mowing timetable (C.11).

#include "core/Schedule.h"

#include <ctime>
#include <fstream>
#include <stdexcept>

namespace sunray {

bool Schedule::load(const std::filesystem::path& path) {
    entries_.clear();
    try {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        nlohmann::json j;
        f >> j;
        return fromJson(j);
    } catch (...) {
        entries_.clear();
        return false;
    }
}

bool Schedule::save(const std::filesystem::path& path) const {
    try {
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << toJson().dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool Schedule::fromJson(const nlohmann::json& arr) {
    entries_.clear();
    if (!arr.is_array()) return false;
    for (const auto& e : arr) {
        ScheduleEntry se;
        se.wday          = e.value("wday",          0);
        se.start_hhmm    = e.value("start_hhmm",    800);
        se.end_hhmm      = e.value("end_hhmm",      1200);
        se.enabled       = e.value("enabled",       true);
        se.rain_delay_min = e.value("rain_delay_min", 0);
        if (se.wday < 0 || se.wday > 6) return false;
        const int sh = se.start_hhmm / 100, sm = se.start_hhmm % 100;
        const int eh = se.end_hhmm   / 100, em = se.end_hhmm   % 100;
        if (sh < 0 || sh > 23 || eh < 0 || eh > 23 || sm < 0 || sm > 59 || em < 0 || em > 59) return false;
        if (se.start_hhmm >= se.end_hhmm) return false;
        if (se.rain_delay_min < 0) se.rain_delay_min = 0;
        entries_.push_back(se);
    }
    return true;
}

nlohmann::json Schedule::toJson() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : entries_) {
        arr.push_back({
            {"wday",           e.wday},
            {"start_hhmm",     e.start_hhmm},
            {"end_hhmm",       e.end_hhmm},
            {"enabled",        e.enabled},
            {"rain_delay_min", e.rain_delay_min},
        });
    }
    return arr;
}

bool Schedule::isActive(int wday, int hhmm) const {
    for (const auto& e : entries_) {
        if (!e.enabled)       continue;
        if (e.wday != wday)   continue;
        if (hhmm >= e.start_hhmm && hhmm < e.end_hhmm) return true;
    }
    return false;
}

bool Schedule::isActiveNow() const {
    std::time_t t = std::time(nullptr);
    std::tm*    lt = std::localtime(&t);
    if (!lt) return false;
    const int hhmm = lt->tm_hour * 100 + lt->tm_min;
    return isActive(lt->tm_wday, hhmm);
}

} // namespace sunray
