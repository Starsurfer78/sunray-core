// sunray-core — entry point
// Wires together Config, HardwareInterface implementation, Robot, and
// WebSocketServer via DI.
// Platform: Linux (Raspberry Pi 4B)
//
// Usage:
//   sunray-core [config.json]       — Alfred hardware (SerialRobotDriver)
//   sunray-core --sim [config.json] — Software simulation (SimulationDriver)

#include "core/Config.h"
#include "core/Logger.h"
#include "core/MqttClient.h"
#include "core/Robot.h"
#include "core/WebSocketServer.h"
#include "hal/GpsDriver/UbloxGpsDriver.h"
#include "hal/SerialRobotDriver/SerialRobotDriver.h"
#include "hal/SimulationDriver/SimulationDriver.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <pthread.h>
#include <sstream>
#include <thread>
#include <vector>

static std::thread startSignalWaitThread(sunray::Robot &robot,
                                         const std::shared_ptr<sunray::Logger> &logger)
{
    sigset_t signalSet;
    sigemptyset(&signalSet);
    sigaddset(&signalSet, SIGINT);
    sigaddset(&signalSet, SIGTERM);

    const int maskRc = pthread_sigmask(SIG_BLOCK, &signalSet, nullptr);
    if (maskRc != 0)
    {
        throw std::runtime_error("pthread_sigmask failed");
    }

    return std::thread([signalSet, &robot, logger]() mutable
                       {
        int sig = 0;
        while (true) {
            const int rc = sigwait(&signalSet, &sig);
            if (rc != 0) {
                if (logger) logger->warn("main", "sigwait failed");
                return;
            }
            if (sig == SIGINT || sig == SIGTERM) {
                if (logger) {
                    logger->info("main", std::string("Signal received: ")
                                             + (sig == SIGTERM ? "SIGTERM" : "SIGINT")
                                             + " — stopping robot loop");
                }
                robot.stop();
                return;
            }
        } });
}

static std::string defaultConfigPath()
{
    if (const char *env = std::getenv("CONFIG_PATH"); env && *env)
    {
        return env;
    }

    const std::filesystem::path corePath = "/etc/sunray-core/config.json";
    if (std::filesystem::exists(corePath))
    {
        return corePath.string();
    }

    return "/etc/sunray/config.json";
}

static std::string resolveWebRoot()
{
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;
    candidates.emplace_back(fs::current_path() / "webui-svelte" / "dist");

    const fs::path procExe = "/proc/self/exe";
    std::error_code ec;
    if (fs::exists(procExe, ec))
    {
        const fs::path exePath = fs::read_symlink(procExe, ec);
        if (!ec && !exePath.empty())
        {
            const fs::path exeDir = exePath.parent_path();
            candidates.emplace_back(exeDir / "webui-svelte" / "dist");
            candidates.emplace_back(exeDir.parent_path() / "webui-svelte" / "dist");
        }
    }

    for (const auto &candidate : candidates)
    {
        if (fs::exists(candidate / "index.html"))
        {
            return fs::weakly_canonical(candidate).string();
        }
    }

    // Fall back to the historical relative path if nothing was found yet.
    return "webui-svelte/dist";
}

static std::string resolveOtaScriptPath()
{
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;
    candidates.emplace_back(fs::current_path() / "scripts" / "ota_update.sh");

    const fs::path procExe = "/proc/self/exe";
    std::error_code ec;
    if (fs::exists(procExe, ec))
    {
        const fs::path exePath = fs::read_symlink(procExe, ec);
        if (!ec && !exePath.empty())
        {
            const fs::path exeDir = exePath.parent_path();
            candidates.emplace_back(exeDir / "scripts" / "ota_update.sh");
            candidates.emplace_back(exeDir.parent_path() / "scripts" / "ota_update.sh");
        }
    }

    for (const auto &candidate : candidates)
    {
        if (fs::exists(candidate) && fs::is_regular_file(candidate))
        {
            return fs::weakly_canonical(candidate).string();
        }
    }

    return {};
}

static std::string resolveFlashScriptPath()
{
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;
    candidates.emplace_back(fs::current_path() / "scripts" / "flash_alfred.sh");

    const fs::path procExe = "/proc/self/exe";
    std::error_code ec;
    if (fs::exists(procExe, ec))
    {
        const fs::path exePath = fs::read_symlink(procExe, ec);
        if (!ec && !exePath.empty())
        {
            const fs::path exeDir = exePath.parent_path();
            candidates.emplace_back(exeDir / "scripts" / "flash_alfred.sh");
            candidates.emplace_back(exeDir.parent_path() / "scripts" / "flash_alfred.sh");
        }
    }

    for (const auto &candidate : candidates)
    {
        if (fs::exists(candidate) && fs::is_regular_file(candidate))
        {
            return fs::weakly_canonical(candidate).string();
        }
    }

    return {};
}

static std::vector<std::string> loadMissionZoneIds(const std::string &missionPath,
                                                   const std::string &missionId)
{
    if (missionId.empty() || !std::filesystem::exists(missionPath))
        return {};

    std::ifstream file(missionPath);
    if (!file.is_open())
        return {};

    const std::string raw((std::istreambuf_iterator<char>(file)), {});
    if (raw.empty())
        return {};

    std::vector<std::string> zoneIds;
    const auto missions = nlohmann::json::parse(raw);
    if (!missions.is_array())
        return {};

    for (const auto &mission : missions)
    {
        if (mission.value("id", std::string()) != missionId)
            continue;
        if (mission.contains("zoneIds") && mission["zoneIds"].is_array())
        {
            for (const auto &zoneId : mission["zoneIds"])
            {
                zoneIds.push_back(zoneId.get<std::string>());
            }
        }
        break;
    }
    return zoneIds;
}

// ── main ───────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    // ── Argument parsing ───────────────────────────────────────────────────────
    bool simMode = false;
    std::string configPath = defaultConfigPath();

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--sim") == 0)
        {
            simMode = true;
        }
        else
        {
            configPath = argv[i];
        }
    }

    // ── 1. Config ──────────────────────────────────────────────────────────────
    auto config = std::make_shared<sunray::Config>(configPath);
    const auto configDir = config->path().parent_path();

    // ── 2. Logger ──────────────────────────────────────────────────────────────
    sunray::WebSocketServer *wsLogSink = nullptr;
    auto stdoutLogger = std::make_shared<sunray::StdoutLogger>(sunray::LogLevel::INFO);
    auto logger = std::make_shared<sunray::FanoutLogger>(
        stdoutLogger,
        [&wsLogSink](sunray::LogLevel level, const std::string &module, const std::string &msg)
        {
            if (!wsLogSink)
                return;
            const char *tag = "INFO";
            switch (level)
            {
            case sunray::LogLevel::DEBUG:
                tag = "DEBUG";
                break;
            case sunray::LogLevel::INFO:
                tag = "INFO";
                break;
            case sunray::LogLevel::WARN:
                tag = "WARN";
                break;
            case sunray::LogLevel::ERROR:
                tag = "ERROR";
                break;
            }
            std::ostringstream line;
            line << "[" << tag << "] " << module << " " << msg;
            wsLogSink->broadcastLog(line.str());
        });
    logger->info("main", std::string("sunray-core starting") + (simMode ? " [SIMULATION]" : " [Alfred]") + " — config: " + configPath);

    // ── 3. Hardware driver (DI switch) ─────────────────────────────────────────
    sunray::SimulationDriver *simDrv = nullptr;
    std::unique_ptr<sunray::HardwareInterface> hw;

    if (simMode)
    {
        auto drv = std::make_unique<sunray::SimulationDriver>(config);
        simDrv = drv.get();
        hw = std::move(drv);
    }
    else
    {
        hw = std::make_unique<sunray::SerialRobotDriver>(config, logger);
    }

    // ── 4. Robot ───────────────────────────────────────────────────────────────
    sunray::Robot robot(std::move(hw), config, logger);
    std::thread signalThread = startSignalWaitThread(robot, logger);

    // ── 5. Initialise ──────────────────────────────────────────────────────────
    if (!robot.init())
    {
        logger->error("main", "Robot init failed — exiting");
        return EXIT_FAILURE;
    }

    // Optional GPS driver (disabled in simulator)
    if (!simMode)
    {
        auto gps = std::make_unique<sunray::UbloxGpsDriver>(config, logger);
        if (gps->init())
        {
            robot.setGpsDriver(std::move(gps));
            logger->info("main", "GPS driver enabled");
        }
        else
        {
            logger->warn("main", "GPS driver init failed — continuing without GPS");
        }
    }

    // ── 6. WebSocket + HTTP server ─────────────────────────────────────────────
    auto wsServer = std::make_unique<sunray::WebSocketServer>(config, logger);
    wsLogSink = wsServer.get();

    // Static file serving: resolve webui/dist from cwd and binary location.
    wsServer->setWebRoot(resolveWebRoot());

    // Map file: read/written by GET/POST /api/map; reloaded into Robot on POST
    const std::string mapPath =
        config->get<std::string>("map_path", (configDir / "map.json").string());
    const std::string missionPath =
        config->get<std::string>("mission_path", (configDir / "missions.json").string());
    wsServer->setMapPath(mapPath);
    wsServer->setMissionPath(missionPath);

    // OTA update script: resolved relative to cwd and running binary location.
    if (const std::string otaScript = resolveOtaScriptPath(); !otaScript.empty())
    {
        wsServer->setOtaScriptPath(otaScript);
    }
    if (const std::string flashScript = resolveFlashScriptPath(); !flashScript.empty())
    {
        wsServer->setStmFlashScriptPath(flashScript);
    }
    wsServer->onStmFlashStateChange([&robot](bool active, uint64_t recoveryGraceMs)
                                    { robot.setStmFlashMaintenance(active, recoveryGraceMs); });
    wsServer->onMapGet([&robot]() -> nlohmann::json
                       { return robot.getMapJson(); });
    wsServer->onMapReload([&robot, mapPath]()
                          { return robot.loadMap(mapPath); });

    // WebSocket commands → Robot
    wsServer->onCommand([&robot, logger, missionPath](const std::string &cmd,
                                                      const nlohmann::json &params)
                        {
        if (cmd != "drive") {
            logger->info("main", "WS command received: " + cmd);
        }
        if      (cmd == "start")  {
            const std::string missionId = params.value("missionId", std::string());
            if (!missionId.empty()) {
                try {
                    robot.requestStartMowingMission(
                        missionId, loadMissionZoneIds(missionPath, missionId));
                } catch (...) {
                    robot.requestStartMowing();
                }
            } else {
                robot.requestStartMowing();
            }
        }
        else if (cmd == "stop")   { robot.requestEmergencyStop(); }
        else if (cmd == "dock")   { robot.requestStartDocking(); }
        else if (cmd == "charge") { robot.requestStartDocking(); }  // alias
        else if (cmd == "setpos") {
            const float lat = static_cast<float>(params.value("lat", 0.0));
            const float lon = static_cast<float>(params.value("lon", 0.0));
            robot.requestSetPose(lon, lat, 0.0f);
        }
        else if (cmd == "drive") {
            const float lin = static_cast<float>(params.value("linear",  0.0));
            const float ang = static_cast<float>(params.value("angular", 0.0));
            robot.manualDrive(lin, ang);
        }
        else if (cmd == "reset") {
            robot.requestEmergencyStop();
        }
        else if (cmd == "startZones") {  // C.12: zone-filtered mowing
            std::vector<std::string> ids;
            if (params.contains("zones") && params["zones"].is_array())
                for (const auto& z : params["zones"]) ids.push_back(z.get<std::string>());
            robot.requestStartMowingZones(ids);
        }
        else if (cmd == "resume") {  // N6.3: resume after dock interrupt
            robot.requestResumeActiveMission();
        }
        else {
            logger->warn("main", "Unknown WebSocket command ignored: " + cmd);
        } });

    // N6.1: cache preview plan in Robot so startMowing() uses it.
    wsServer->onPlanPreview([&robot](sunray::nav::MissionPlan plan)
                            { robot.storePendingMissionPlan(std::move(plan)); });

    // Diagnostic commands → Robot (C.10b)
    wsServer->onDiag([&robot](const std::string &action,
                              const nlohmann::json &params) -> nlohmann::json
                     {
        if (action == "motor") {
            const std::string motor       = params.value("motor", "left");
            const float       pwm         = static_cast<float>(params.value("pwm", 0.15));
            const unsigned    dur         = params.value("duration_ms", 3000u);
            const int         revolutions = params.value("revolutions", 0);
            return robot.diagRunMotor(motor, pwm, dur, revolutions);
        } else if (action == "mow") {
            const bool on = params.value("on", false);
            return robot.diagMowMotor(on);
        } else if (action == "drive") {
            const float dist = static_cast<float>(params.value("distance_m", 3.0));
            const float pwm = static_cast<float>(params.value("pwm", 0.15));
            return robot.diagDriveStraight(dist, pwm);
        } else if (action == "turn") {
            const float angle = static_cast<float>(params.value("angle_deg", 90.0));
            const float pwm = static_cast<float>(params.value("pwm", 0.15));
            return robot.diagTurnInPlace(angle, pwm);
        } else if (action == "imu_calib") {
            return robot.diagImuCalib();
        }
        return {{"ok", false}, {"error", "unknown action: " + action}}; });

    // Schedule API (C.11)
    {
        auto schedulePath =
            std::filesystem::path(
                config->get<std::string>("config_dir", configDir.string())) /
            "schedule.json";
        robot.loadSchedule(schedulePath);
        wsServer->onScheduleGet([&robot]() -> nlohmann::json
                                { return robot.getSchedule(); });
        wsServer->onSchedulePut([&robot](const nlohmann::json &arr) -> nlohmann::json
                                { return nlohmann::json{{"ok", robot.setSchedule(arr)}}; });
    }

    // History/statistics API (C.17-i)
    wsServer->onHistoryEventsGet([&robot](unsigned limit) -> nlohmann::json
                                 { return robot.getHistoryEvents(limit); });
    wsServer->onHistorySessionsGet([&robot](unsigned limit) -> nlohmann::json
                                   { return robot.getHistorySessions(limit); });
    wsServer->onHistoryClear([&robot]() -> nlohmann::json
                             { return robot.clearHistory(); });
    wsServer->onStatisticsSummaryGet([&robot]() -> nlohmann::json
                                     { return robot.getHistoryStatisticsSummary(); });

    // Simulator commands → SimulationDriver (only wired in --sim mode)
    if (simDrv)
    {
        wsServer->onSimCommand([simDrv, logger](const std::string &action,
                                                const nlohmann::json &params)
                               {
            if (action == "bumper") {
                const std::string side = params.value("side", "both");
                if (side == "left"  || side == "both") simDrv->setBumperLeft(true);
                if (side == "right" || side == "both") simDrv->setBumperRight(true);
            } else if (action == "gps") {
                const std::string q = params.value("quality", "fix");
                if      (q == "fix")   simDrv->setGpsQuality(sunray::GpsQuality::FIX);
                else if (q == "float") simDrv->setGpsQuality(sunray::GpsQuality::FLOAT);
                else                   simDrv->setGpsQuality(sunray::GpsQuality::NO_FIX);
            } else if (action == "lift") {
                simDrv->setLift(true);
            } else if (action == "obstacle") {
                const double x = params.value("x", 0.0);
                const double y = params.value("y", 0.0);
                const double r = params.value("radius", 0.5);
                // 8-point circle approximation → Polygon
                sunray::Polygon poly;
                for (int i = 0; i < 8; ++i) {
                    const double a = i * 2.0 * M_PI / 8.0;
                    poly.push_back({ x + r * std::cos(a), y + r * std::sin(a) });
                }
                simDrv->addObstacle(std::move(poly));
            } else if (action == "obstacles_clear") {
                simDrv->clearObstacles();
            } else {
                logger->warn("main", "Unknown simulator command ignored: " + action);
            } });
    }

    wsServer->start();
    robot.setWebSocketServer(wsServer.get());

    // ── 7. MQTT client (optional — enabled via mqtt_enabled=true in config) ───
    auto mqttClient = std::make_unique<sunray::MqttClient>(config, logger);
    mqttClient->onCommand([&robot, logger, missionPath](const std::string &cmd,
                                                        const nlohmann::json &params)
                          {
        if      (cmd == "start")  {
            const std::string missionId = params.value("missionId", std::string());
            if (!missionId.empty()) {
                try {
                    robot.requestStartMowingMission(
                        missionId, loadMissionZoneIds(missionPath, missionId));
                } catch (...) {
                    robot.requestStartMowing();
                }
            } else {
                robot.requestStartMowing();
            }
        }
        else if (cmd == "stop")   { robot.requestEmergencyStop(); }
        else if (cmd == "reset")  { robot.requestEmergencyStop(); }
        else if (cmd == "dock")   { robot.requestStartDocking(); }
        else if (cmd == "charge") { robot.requestStartDocking(); }
        else if (cmd == "drive") {
            const float lin = static_cast<float>(params.value("linear",  0.0));
            const float ang = static_cast<float>(params.value("angular", 0.0));
            robot.manualDrive(lin, ang);
        }
        else {
            logger->warn("main", "Unknown MQTT command ignored: " + cmd);
        } });
    mqttClient->start();
    robot.setMqttClient(mqttClient.get());

    // ── 8. Run (blocks until stop()) ──────────────────────────────────────────
    robot.loop();

    if (signalThread.joinable())
    {
        signalThread.detach();
    }

    mqttClient->stop();
    wsServer->stop();

    logger->info("main", "sunray-core stopped cleanly");
    return EXIT_SUCCESS;
}
