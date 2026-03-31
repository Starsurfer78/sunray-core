#pragma once
// Logger.h — Lightweight logging interface for Sunray Core.
//
// Injected into Robot (and other core classes) via constructor.
// The interface is intentionally minimal — no stream operators, no macros.
//
// Typical DI use:
//   auto logger = std::make_shared<StdoutLogger>(LogLevel::INFO);
//   auto robot  = Robot(std::move(hw), config, logger);

#include <string>
#include <cstdio>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>

namespace sunray {

enum class LogLevel : int {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3,
};

/// Abstract logging interface — implement for file, syslog, MQTT, etc.
class Logger {
public:
    virtual ~Logger() = default;

    /// Emit a log message at the given level.
    /// module: short tag, e.g. "Robot", "SerialRobotDriver"
    /// msg:    human-readable message (no trailing newline required)
    virtual void log(LogLevel level, const std::string& module, const std::string& msg) = 0;

    // ── Convenience wrappers ─────────────────────────────────────────────────

    void debug(const std::string& module, const std::string& msg) { log(LogLevel::DEBUG, module, msg); }
    void info (const std::string& module, const std::string& msg) { log(LogLevel::INFO,  module, msg); }
    void warn (const std::string& module, const std::string& msg) { log(LogLevel::WARN,  module, msg); }
    void error(const std::string& module, const std::string& msg) { log(LogLevel::ERROR, module, msg); }
};

// ── StdoutLogger ──────────────────────────────────────────────────────────────

/// Simple logger that writes to stdout.
/// Messages below minLevel are silently dropped.
class StdoutLogger : public Logger {
public:
    explicit StdoutLogger(LogLevel minLevel = LogLevel::INFO)
        : minLevel_(minLevel) {}

    void log(LogLevel level, const std::string& module, const std::string& msg) override {
        if (level < minLevel_) return;

        const char* tag = "INFO ";
        switch (level) {
            case LogLevel::DEBUG: tag = "DEBUG"; break;
            case LogLevel::INFO:  tag = "INFO "; break;
            case LogLevel::WARN:  tag = "WARN "; break;
            case LogLevel::ERROR: tag = "ERROR"; break;
        }
        std::printf("[%s] %-20s %s\n", tag, module.c_str(), msg.c_str());
        std::fflush(stdout);
    }

private:
    LogLevel minLevel_;
};

/// Null logger — discards all messages (useful in tests).
class NullLogger : public Logger {
public:
    void log(LogLevel, const std::string&, const std::string&) override {}
};

/// Fan-out logger: forwards every message to a primary logger and optionally to
/// a secondary callback (for example WebUI live log mirroring).
class FanoutLogger : public Logger {
public:
    using SinkCallback = std::function<void(LogLevel, const std::string&, const std::string&)>;

    explicit FanoutLogger(std::shared_ptr<Logger> primary, SinkCallback secondary = {})
        : primary_(std::move(primary)), secondary_(std::move(secondary)) {}

    void log(LogLevel level, const std::string& module, const std::string& msg) override {
        if (primary_) primary_->log(level, module, msg);
        if (secondary_) secondary_(level, module, msg);
    }

private:
    std::shared_ptr<Logger> primary_;
    SinkCallback secondary_;
};

} // namespace sunray
