# SimulationDriver — software-only HardwareInterface (no real hardware)
# Runs on any platform: Linux, macOS, Windows.

add_library(sunray_simulation_driver STATIC
    SimulationDriver.cpp
)

target_include_directories(sunray_simulation_driver PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sunray_simulation_driver PUBLIC
    sunray_hal
    sunray_core
)
