# core — Platform-independent robot logic
# No hardware includes. Depends only on HardwareInterface (via sunray_hal).

add_library(sunray_core STATIC
    Config.cpp
    Robot.cpp
    WebSocketServer.cpp
    MqttClient.cpp
    control/OpenLoopDriveController.cpp
    op/Op.cpp
    op/IdleOp.cpp
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
    navigation/Map.cpp
    navigation/LineTracker.cpp
    navigation/GridMap.cpp
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
