# core — Platform-independent robot logic
# No hardware includes. Depends only on HardwareInterface (via sunray_hal).

add_library(sunray_core STATIC
    Config.cpp
    Robot.cpp
    WebSocketServer.cpp
    op/Op.cpp
    op/IdleOp.cpp
    op/ChargeOp.cpp
    op/MowOp.cpp
    op/DockOp.cpp
    op/EscapeReverseOp.cpp
    op/GpsWaitFixOp.cpp
    op/ErrorOp.cpp
    navigation/StateEstimator.cpp
    navigation/Map.cpp
    navigation/LineTracker.cpp
)

target_include_directories(sunray_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sunray_core PUBLIC
    sunray_hal            # HardwareInterface.h (interface only, no implementation)
    nlohmann_json::nlohmann_json
    Crow::Crow            # WebSocketServer.cpp (pimpl: crow.h only in .cpp)
)
