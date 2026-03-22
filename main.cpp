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
#include "core/Robot.h"
#include "core/WebSocketServer.h"
#include "hal/SerialRobotDriver/SerialRobotDriver.h"
#include "hal/SimulationDriver/SimulationDriver.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <memory>

// ── Global robot pointer for signal handler ────────────────────────────────────
// Only stop() (atomic flag write) is called from the handler — safe.
static sunray::Robot* g_robot = nullptr;

static void signalHandler(int) {
    if (g_robot) g_robot->stop();
}

// ── main ───────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // ── Argument parsing ───────────────────────────────────────────────────────
    bool        simMode    = false;
    std::string configPath = "/etc/sunray/config.json";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--sim") == 0) {
            simMode = true;
        } else {
            configPath = argv[i];
        }
    }

    // ── 1. Config ──────────────────────────────────────────────────────────────
    auto config = std::make_shared<sunray::Config>(configPath);

    // ── 2. Logger ──────────────────────────────────────────────────────────────
    auto logger = std::make_shared<sunray::StdoutLogger>(sunray::LogLevel::INFO);
    logger->info("main", std::string("sunray-core starting")
                         + (simMode ? " [SIMULATION]" : " [Alfred]")
                         + " — config: " + configPath);

    // ── 3. Hardware driver (DI switch) ─────────────────────────────────────────
    sunray::SimulationDriver* simDrv = nullptr;
    std::unique_ptr<sunray::HardwareInterface> hw;

    if (simMode) {
        auto drv = std::make_unique<sunray::SimulationDriver>(config);
        simDrv   = drv.get();
        hw       = std::move(drv);
    } else {
        hw = std::make_unique<sunray::SerialRobotDriver>(config);
    }

    // ── 4. Robot ───────────────────────────────────────────────────────────────
    sunray::Robot robot(std::move(hw), config, logger);
    g_robot = &robot;

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── 5. Initialise ──────────────────────────────────────────────────────────
    if (!robot.init()) {
        logger->error("main", "Robot init failed — exiting");
        return EXIT_FAILURE;
    }

    // ── 6. WebSocket + HTTP server ─────────────────────────────────────────────
    auto wsServer = std::make_unique<sunray::WebSocketServer>(config, logger);

    // Static file serving: webui/dist relative to binary location
    wsServer->setWebRoot("webui/dist");

    // Map file: read/written by GET/POST /api/map; reloaded into Robot on POST
    const std::string mapPath = config->get<std::string>("map_path",
                                                          "/etc/sunray/map.json");
    wsServer->setMapPath(mapPath);
    wsServer->onMapReload([&robot, mapPath]() {
        return robot.loadMap(mapPath);
    });

    // WebSocket commands → Robot
    wsServer->onCommand([&robot](const std::string& cmd,
                                  const nlohmann::json& params) {
        if      (cmd == "start")  { robot.startMowing(); }
        else if (cmd == "stop")   { robot.emergencyStop(); }
        else if (cmd == "dock")   { robot.startDocking(); }
        else if (cmd == "charge") { robot.startDocking(); }  // alias
        else if (cmd == "setpos") {
            const float lat = static_cast<float>(params.value("lat", 0.0));
            const float lon = static_cast<float>(params.value("lon", 0.0));
            robot.setPose(lon, lat, 0.0f);
        }
        else if (cmd == "drive") {
            const float lin = static_cast<float>(params.value("linear",  0.0));
            const float ang = static_cast<float>(params.value("angular", 0.0));
            robot.manualDrive(lin, ang);
        }
    });

    // Simulator commands → SimulationDriver (only wired in --sim mode)
    if (simDrv) {
        wsServer->onSimCommand([simDrv](const std::string& action,
                                        const nlohmann::json& params) {
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
            }
        });
    }

    wsServer->start();
    robot.setWebSocketServer(wsServer.get());

    // ── 7. Run (blocks until stop()) ──────────────────────────────────────────
    robot.loop();

    wsServer->stop();

    logger->info("main", "sunray-core stopped cleanly");
    return EXIT_SUCCESS;
}
