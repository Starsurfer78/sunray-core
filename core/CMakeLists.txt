# core — Platform-independent robot logic
# No hardware includes. Depends only on HardwareInterface (via sunray_hal).

add_library(sunray_core STATIC
    ButtonActions.cpp
    BuzzerSequencer.cpp
    Config.cpp
    EventMessages.cpp
    Robot.cpp
    WebSocketServer.cpp
    MqttClient.cpp
    storage/EventRecorder.cpp
    storage/HistoryDatabase.cpp
    control/OpenLoopDriveController.cpp
    control/PidController.cpp
    control/DifferentialDriveController.cpp
    op/Op.cpp
    op/IdleOp.cpp
    op/ManualDriveOp.cpp
    op/ChargeOp.cpp
    op/UndockOp.cpp
    op/NavToStartOp.cpp
    op/WaitRainOp.cpp
    op/MowOp.cpp
    op/DockOp.cpp
    op/EscapeReverseOp.cpp
    op/GpsWaitFixOp.cpp
    op/ErrorOp.cpp
    navigation/StateEstimator.cpp
    navigation/Costmap.cpp
    navigation/PathPlanner.cpp
    navigation/Map.cpp
    navigation/RuntimeState.cpp
    navigation/MissionCompiler.cpp
    navigation/RouteValidator.cpp
    navigation/LineTracker.cpp
    navigation/GridMap.cpp
    navigation/ZonePlanner.cpp
    navigation/StripingAlgorithm.cpp
    navigation/TransitionRouter.cpp
    navigation/WaypointExecutor.cpp
    Schedule.cpp
)

target_include_directories(sunray_core PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(sunray_core PRIVATE 
    SUNRAY_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(sunray_core PUBLIC
    sunray_hal            # HardwareInterface.h (interface only, no implementation)
    nlohmann_json::nlohmann_json
    Crow::Crow            # WebSocketServer.cpp (pimpl: crow.h only in .cpp)
    Clipper2              # MissionCompiler.cpp — polygon offset
)

# ── Optional: libmosquitto for MqttClient ────────────────────────────────────
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MOSQUITTO QUIET libmosquitto)
endif()
if(MOSQUITTO_FOUND)
    message(STATUS "MQTT: libmosquitto ${MOSQUITTO_VERSION} found — MqttClient enabled")
    target_compile_definitions(sunray_core PRIVATE SUNRAY_HAVE_MOSQUITTO)
    target_include_directories(sunray_core PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
    target_link_libraries(sunray_core PRIVATE ${MOSQUITTO_LINK_LIBRARIES})
else()
    message(STATUS "MQTT: libmosquitto not found — MqttClient is a no-op stub "
                   "(install libmosquitto-dev to enable)")
endif()

find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIBRARY sqlite3)
if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
    message(STATUS "SQLite: found — history database support enabled")
    target_compile_definitions(sunray_core PRIVATE SUNRAY_HAVE_SQLITE)
    target_include_directories(sunray_core PRIVATE ${SQLITE3_INCLUDE_DIR})
    target_link_libraries(sunray_core PRIVATE ${SQLITE3_LIBRARY})
else()
    message(STATUS "SQLite: not found — history database runs in disabled stub mode")
endif()
